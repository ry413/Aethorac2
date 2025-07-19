idf_component_register(SRCS "network.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES app_update esp_timer esp_https_ota rs485_comm driver esp_eth esp_netif esp_wifi nvs_flash)
