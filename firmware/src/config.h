#pragma once

#include <Arduino.h>

void config_begin();
void config_poll_serial();

String config_wifi_ssid();
String config_wifi_pass();
String config_token();
String config_host();
uint16_t config_port();
bool config_dark();
void config_set_dark(bool dark);
