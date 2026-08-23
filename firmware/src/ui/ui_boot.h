#pragma once

/* Boot splash: loops the baked pet animations until the first dashboard
 * snapshot lands (or a timeout), then fades into the desktop. Tap to skip. */
void ui_boot_begin();
void ui_boot_poll();
bool ui_boot_active();
