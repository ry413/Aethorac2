#pragma once

typedef enum {
    NET_TYPE_NONE,
    NET_TYPE_WIFI,
    NET_TYPE_ETHERNET
} net_type_t;

bool network_is_ready();
net_type_t network_current_type();
void set_ip_raw(uint32_t ip);
uint32_t get_ip_raw();
void restore_last_network();
bool save_wifi_credentials(const char* ssid, const char* pass);
void change_network_type_and_reboot(net_type_t new_type);