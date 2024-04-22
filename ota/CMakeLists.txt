idf_component_register(SRCS "esp_ota.c" "luna_ota.c"
                    INCLUDE_DIRS .
                    REQUIRES fatfs spiffs firmware app_update

)