#pragma once

#include <Arduino.h>

void ota_begin();
void ota_loop();
void ota_force(); /* serial OTA: check and install right now */
