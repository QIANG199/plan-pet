#include "rtc.h"
#include "bsp/i2c_bsp.h"
#include <sys/time.h>
#include <time.h>

static const uint8_t REG_SECONDS = 0x04;
static bool saved;

static uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0f)); }
static uint8_t bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

static bool time_ok(time_t t) { return t > 1700000000; }

void rtc_begin() {
  setenv("TZ", "CST-8", 1);
  tzset();
  if (!rtc_dev_handle) return;
  uint8_t raw[7];
  if (i2c_read_buff(rtc_dev_handle, REG_SECONDS, raw, 7) != 0) return;
  if (raw[0] & 0x80) return;
  struct tm t = {};
  t.tm_sec = bcd2bin(raw[0] & 0x7f);
  t.tm_min = bcd2bin(raw[1] & 0x7f);
  t.tm_hour = bcd2bin(raw[2] & 0x3f);
  t.tm_mday = bcd2bin(raw[3] & 0x3f);
  t.tm_mon = bcd2bin(raw[5] & 0x1f) - 1;
  t.tm_year = bcd2bin(raw[6]) + 100;
  t.tm_isdst = -1;
  if (t.tm_mon < 0 || t.tm_mon > 11) return;
  time_t epoch = mktime(&t);
  if (!time_ok(epoch)) return;
  struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
  settimeofday(&tv, nullptr);
}

void rtc_save_if_synced() {
  if (saved || !rtc_dev_handle) return;
  time_t now = time(nullptr);
  if (!time_ok(now)) return;
  struct tm t;
  if (!localtime_r(&now, &t)) return;
  uint8_t raw[7] = {
      bin2bcd((uint8_t)t.tm_sec),
      bin2bcd((uint8_t)t.tm_min),
      bin2bcd((uint8_t)t.tm_hour),
      bin2bcd((uint8_t)t.tm_mday),
      bin2bcd((uint8_t)t.tm_wday),
      bin2bcd((uint8_t)(t.tm_mon + 1)),
      bin2bcd((uint8_t)(t.tm_year % 100)),
  };
  if (i2c_write_buff(rtc_dev_handle, REG_SECONDS, raw, 7) == 0) saved = true;
}
