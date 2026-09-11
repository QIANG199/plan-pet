#include "ota.h"
#include "config.h"
#include "net.h"
#include "power.h"
#include "standby.h"
#include "ui/ui.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Network.h>
#include <Update.h>
#include <WiFi.h>
#include <time.h>

/* WiFi OTA: the relay serves the built app image; we compare its mtime-based
 * build number against this image's compile time and stream it into the idle
 * OTA slot. A cut during download leaves the running slot untouched. */
static const uint32_t CHECK_INTERVAL_MS = 6UL * 60 * 60 * 1000;
static const uint32_t FAIL_BACKOFF_MS = 60UL * 60 * 1000;
static const uint32_t FIRST_CHECK_MS = 2 * 60 * 1000; /* let NTP settle first */
/* The bin's mtime lands minutes after the image's compile stamp (link + bake
 * tail), so the same build always reads "newer". Ignore deltas inside this
 * window; real remote updates are built far apart in time. */
static const uint32_t BUILD_TOLERANCE_SEC = 15 * 60;

static uint32_t nextCheckAt = FIRST_CHECK_MS;
static bool forced;
static uint32_t failShownAt;
static int lastPctShown;
static time_t lastPushAt; /* 已响应的指令推时刻，同一值不重复触发 */

/* Compile stamp as unix seconds; mktime applies the local zone (CST-8 once
 * NTP is up), matching the server's bin-mtime build number. */
static uint32_t fwBuild() {
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {0};
  int day, year, h, m, s;
  if (sscanf(__DATE__, "%3s %d %d", mon, &day, &year) != 3) return 0;
  if (sscanf(__TIME__, "%d:%d:%d", &h, &m, &s) != 3) return 0;
  const char *hit = strstr(months, mon);
  if (!hit) return 0;
  struct tm t = {0};
  t.tm_year = year - 1900;
  t.tm_mon = (int)(hit - months) / 3;
  t.tm_mday = day;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = s;
  time_t e = mktime(&t);
  return e > 0 ? (uint32_t)e : 0;
}

static void otaFail() {
  Update.abort();
  Serial.println("[ota] fail; retry in 1h");
  ui_set_ota("UPD fail");
  failShownAt = millis();
  nextCheckAt = millis() + FAIL_BACKOFF_MS;
}

static bool fetchInfo(uint32_t &build, uint32_t &size) {
  IPAddress ip;
  if (!net_resolve_host(ip)) return false;
  String url = "http://" + ip.toString() + ":" + String(config_port()) + "/api/ota/info";
  HTTPClient http;
  http.setTimeout(4000);
  http.begin(url);
  http.addHeader("X-Panel-Token", config_token());
  bool ok = false;
  if (http.GET() == 200) {
    JsonDocument doc;
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      build = doc["build"] | 0;
      size = doc["size"] | 0;
      ok = build > 0 && size > 0;
    }
  }
  http.end();
  return ok;
}

static void download(uint32_t size) {
  IPAddress ip;
  if (!net_resolve_host(ip)) {
    otaFail();
    return;
  }
  String url = "http://" + ip.toString() + ":" + String(config_port()) + "/api/ota/bin";
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(url);
  http.addHeader("X-Panel-Token", config_token());
  if (http.GET() != 200) {
    http.end();
    otaFail();
    return;
  }
  int len = http.getSize();
  if (len <= 0 || !Update.begin(len)) {
    http.end();
    otaFail();
    return;
  }
  Serial.printf("[ota] flashing %d bytes\n", len);
  ui_set_ota("UPD 0%");
  lastPctShown = 0;
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buf[4096];
  int written = 0;
  while (http.connected() && written < len) {
    int avail = stream->available();
    if (avail <= 0) {
      delay(2);
      continue;
    }
    int chunk = stream->readBytes(buf, min((size_t)avail, sizeof(buf)));
    if (Update.write(buf, chunk) != (size_t)chunk) break;
    written += chunk;
    int pct = (int)((int64_t)written * 100 / len);
    if (pct >= lastPctShown + 5) {
      lastPctShown = pct;
      char lbl[16];
      snprintf(lbl, sizeof(lbl), "UPD %d%%", pct);
      ui_set_ota(lbl);
    }
  }
  http.end();
  bool done = written >= len && Update.end(true);
  Serial.printf("[ota] %s (%d/%d)\n", done ? "verified" : "aborted", written, len);
  if (!done) {
    otaFail();
    return;
  }
  ui_set_ota("UPD ok");
  delay(800); /* let the label paint before the reset */
  ESP.restart();
}

void ota_begin() {
  Serial.printf("[ota] build=%u\n", (unsigned)fwBuild());
}

void ota_loop() {
  uint32_t now = millis();
  if (failShownAt && now - failShownAt > 10000) {
    failShownAt = 0;
    ui_set_ota(nullptr);
  }
  /* 指令推：服务端在快照里置位就等于「现在升级」，比 6 小时自检快得多。
   * 同一时刻只触发一次；息屏/非正常供电期间保持 pending，等条件满足再走。 */
  time_t push = net_snapshot().otaForceAt;
  if (push != 0 && push != lastPushAt) {
    lastPushAt = push;
    Serial.printf("[ota] server push at %ld\n", (long)push);
    ota_force();
  }
  if (!forced && (int32_t)(now - nextCheckAt) < 0) return;
  /* Auto conditions: awake, normal power phase, server reachable. Manual OTA
   * waits (forced stays set) until the wake request has been processed. */
  if (standby_asleep() || power_phase() != POWER_NORMAL || !net_server_ok()) {
    return;
  }
  bool manual = forced;
  forced = false;
  nextCheckAt = now + CHECK_INTERVAL_MS;
  bool charging = false;
  int pct = 0;
  power_read(&charging, &pct);
  if (!manual && !charging && pct < 30) {
    Serial.println("[ota] skip: on battery <30%");
    return;
  }
  uint32_t build = 0, size = 0;
  if (!fetchInfo(build, size)) {
    nextCheckAt = now + FAIL_BACKOFF_MS;
    return;
  }
  if (build <= fwBuild() + BUILD_TOLERANCE_SEC) {
    Serial.printf("[ota] up to date (server build=%u)\n", (unsigned)build);
    return;
  }
  Serial.printf("[ota] server build=%u > local, updating\n", (unsigned)build);
  download(size);
}

void ota_force() {
  nextCheckAt = millis();
  forced = true;
}
