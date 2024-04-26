idf_component_register(SRCS "wifi_connect.c" "wifi_config.c"
                    INCLUDE_DIRS "."
#                    PRIV_REQUIRES 
                    PRIV_REQUIRES nv_memory esp_netif esp_wifi esp_event
                    )
