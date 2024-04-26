#pragma once
#include <stdbool.h>
#include "esp_err.h"
//#include "esp_app_desc.h"

extern void Esp_Ota_init(char * const root);
extern esp_err_t ESP_ota_start_update (char *const filename);
extern char * Esp_Ota_getName(void);
extern char * Esp_Ota_getBuildDate(void);
extern char * Esp_Ota_getBuildTime(void);
extern char * Esp_Ota_getVersion(void);
extern char * Esp_Ota_getIdfVersion(void);
extern int Esp_Ota_getStatus(void);
extern int Esp_Ota_getProgress(void);
extern int Esp_Ota_getFwLength(void);
extern bool Esp_Ota_getUpdateAvailable(void);
extern char * Esp_Ota_getUpdateVersion(void);
extern char * Esp_Ota_getUpdateDate(void);
extern char * Esp_Ota_getUpdateTime(void);
extern char * Esp_Ota_getUpdateIdfVersion(void);
extern char * Esp_Ota_getUpdateFname(void);
void Esp_Ota_clearUpdateInfo(char * const filename);