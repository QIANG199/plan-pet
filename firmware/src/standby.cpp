#include "standby.h"
#include "config.h"
#include "net.h"
#include "power.h"
#include "ui/ui.h"
#include "ui/ui_boot.h"
#include "ui/ui_settings.h"
#include "bsp/lcd_bl_pwm_bsp.h"
#include "lvgl.h"

/* Pet enters "sleeping" after SLEEP_ENTER_S on the server (pet.js); the
 * panel waits config_sleep_timeout() more minutes before blanking. */
static const long SLEEP_ENTER_S = 900;
static const uint32_t SRV_FRESH_MS = 60000;          /* snapshot age trusted for the trigger */
static const uint32_t POLL_FALLBACK_MS = 30UL * 60UL * 1000UL; /* no data at all -> blank anyway */
static const uint32_t WAKE_FRESH_MS = 15000;         /* snapshot age trusted to wake on events */
static const uint32_t ASLEEP_POLL_MS = 10000;
static const uint32_t AWAKE_POLL_MS = 1000;

static volatile bool wakeReq = false;
static volatile bool sleepNowReq = false;
static bool asleep = false;
static String entryPetState; /* pet state when we blanked; see wake check */
static bool forcedEntry = false; /* SLEEP NOW: manual wake only */

bool standby_asleep() { return asleep; }
void standby_request_wake() { wakeReq = true; }
void standby_request_sleep_now() { sleepNowReq = true; }

static bool pet_active(const String &st) {
  return st == "thinking" || st == "typing" || st == "happy" || st == "error";
}

/* Both server-side conditions need the snapshot to be current; otherwise
 * (network dead, panel unconfigured) fall back to "no data for 30 min". */
static bool server_says_idle() {
  const Snapshot &s = net_snapshot();
  if (s.fresh && millis() - net_last_ok_ms() < SRV_FRESH_MS) {
    long idleS = (long)(s.srvTs - s.petSince); /* ~ time since last coding event */
    return s.petState == "sleeping" && idleS >= SLEEP_ENTER_S + 60L * config_sleep_timeout();
  }
  return millis() - net_last_ok_ms() > POLL_FALLBACK_MS;
}

static bool touch_idle() {
  return lv_display_get_inactive_time(NULL) >= 60000UL * config_sleep_timeout();
}

static void enter_sleep(bool forced) {
  asleep = true;
  forcedEntry = forced;
  entryPetState = net_snapshot().petState;
  ui_set_sleep(true);
  lcd_bl_pwm_bsp_off();
  /* No panel DISPOFF here: the AXS15231B is one chip for display + touch, and
   * display-off hangs its touch I2C, which then stalls the LVGL lock and
   * starves every wake source (touch, BOOT, serial). */
  net_set_poll_ms(ASLEEP_POLL_MS);
  net_sleep_enter();
  /* No setCpuFrequencyMhz() here: the Arduino hal resets all APB peripherals
   * (I2C touch included) when the clock tree is reconfigured, which killed
   * every wake source. Not worth it for ~10mA. */
  Serial.println("[standby] sleep");
}

static void wake_up() {
  asleep = false;
  forcedEntry = false;
  net_sleep_exit();
  net_set_poll_ms(AWAKE_POLL_MS);
  lcd_bl_pwm_bsp_on(config_bright());
  ui_set_sleep(false); /* clears the gate before the setters below */
  ui_apply(net_snapshot());
  ui_set_link(net_wifi_up(), net_server_ok());
  ui_tick_clock();
  Serial.println("[standby] wake");
}

void standby_poll() {
  if (asleep) {
    const Snapshot &s = net_snapshot();
    /* Auto entry from "sleeping": any event flips the state away, wake on
     * the change. Fallback entry (no data): wake on real coding activity.
     * Forced entry (SLEEP NOW): manual wake sources only. */
    bool moved = entryPetState == "sleeping" ? s.petState != "sleeping"
                                             : pet_active(s.petState);
    bool event = !forcedEntry && s.fresh &&
                 millis() - net_last_ok_ms() < WAKE_FRESH_MS && moved;
    if (wakeReq || event || power_phase() != POWER_NORMAL) {
      wakeReq = false;
      wake_up();
    }
    return;
  }
  wakeReq = false; /* stale request while awake */
  bool forced = sleepNowReq;
  sleepNowReq = false;
  if (!forced) {
    if (config_sleep_timeout() == 0) return;
    if (ui_boot_active() || ui_settings_active()) return;
    if (power_phase() != POWER_NORMAL) return;
    if (!touch_idle()) return;
    if (!server_says_idle()) return;
  }
  enter_sleep(forced);
}
