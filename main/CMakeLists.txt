idf_component_register(SRCS "ota_manager.c" "http_server.c" "wifi_manager.c" "main.c"
                       "channel_parse.c"
                       "drv_keyadouble.c"
                       "sbus.c"
                       "cloud_client.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_http_server esp_https_ota app_update nvs_flash json spi_flash driver esp_http_client)
