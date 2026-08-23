#pragma once

#include <Arduino.h>

/* Screen standby: backlight hard-off, rendering paused, polling slowed to
 * 10s. Enter after the pet has been sleeping for the configured extra
 * timeout (or when no snapshot parses for 30 min). Wakes on touch, any
 * serial command, BOOT key, power-key activity, or the pet leaving
 * "sleeping" in a fresh snapshot. */

/* Runs the state machine; call from the Arduino loop with the LVGL lock
 * held (it touches LVGL timers/inactivity state and rebuilds widgets). */
void standby_poll();

bool standby_asleep();

/* Safe from any context (LVGL event callbacks, serial); consumed by the
 * next standby_poll(), i.e. within ~100ms. */
void standby_request_wake();
void standby_request_sleep_now(); /* serial "SLEEP NOW": bypass the guards */
