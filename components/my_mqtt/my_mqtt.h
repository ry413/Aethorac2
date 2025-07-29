#ifndef MY_MQTT_H
#define MY_MQTT_H

#include "esp_err.h"
#include <string>

void mqtt_app_start();
void mqtt_app_stop(void);
void mqtt_publish_message(const std::string& message, int qos, int retain);

void report_states();

int my_log_send_func(const char *fmt, va_list args);

extern bool mqtt_connected;

#endif // MY_MQTT_H