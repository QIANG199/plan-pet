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
void config_set_wifi(const String &ssid, const String &pass);
void config_set_host(const String &host);
void config_set_token(const String &token);
void config_set_port(uint16_t port);
uint8_t config_bright();
void config_set_bright(uint8_t v);
