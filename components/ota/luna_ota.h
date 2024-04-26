#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "esp_err.h"

typedef struct __attribute__((packed))
{
  uint8_t   imgID[8];       //!< User-defined Image Identification bytes
  uint32_t  crc32;          //!< Image's 32-bit CRC value
  uint8_t   bimVer;         //!< BIM version
  uint8_t   metaVer;        //!< Metadata version
  uint16_t  techType;       //!< Wireless protocol type BLE/TI-MAC/ZIGBEE etc.
  uint8_t   imgCpStat;      //!< Image copy status
  uint8_t   crcStat;        //!< CRC status
  uint8_t   imgType;        //!< Image Type
  uint8_t   imgNo;          //!< Image number of 'image type'
  uint32_t  imgVld;         //!< This field is RFU
  uint32_t  len;            //!< Image length in bytes.
  uint32_t  prgEntry;       //!< Program entry address
  uint8_t   softVer[4];     //!< Software version of the image
  uint32_t  imgEndAddr;     //!< Address of the last byte of a contiguous image
  uint16_t  hdrLen;         //!< Total length of the image header
  uint16_t  rfu;            //!< Reserved bytes
}imgFixedHdr_t;

typedef struct __attribute__((packed))
{
  uint8_t   segTypeImg;     //!< Segment type - for Contiguous image payload
  uint16_t  wirelessTech;   //!< Wireless technology type
  uint8_t   rfu;            //!< Reserved byte
  uint32_t  imgSegLen;      //!< Payload segment length
  uint32_t  startAddr;      //!< Start address of image on internal flash
}imgPayloadSeg_t;

typedef struct __attribute__((packed))
{
   imgFixedHdr_t         fixedHdr;    //!< Required core image header
   imgPayloadSeg_t       imgPayload;  //!< Required contiguous image segment
}imgHdr_t;


extern esp_err_t luna_ota_init(char * const base_path,
                               size_t (*send_fn)(uint8_t *const data, const size_t length),
                               size_t (*rcv_fn)(uint8_t *const data, const size_t length),
                               size_t (*reset_fn)(const uint8_t flag));
extern void luna_ota_rcv_evt(const size_t length);
//esp_err_t luna_ota_start(char *const filename);
esp_err_t luna_ota_start(char * const filename);
extern esp_err_t luna_ota_abort(esp_err_t error);

bool luna_ota_getUpdateAvailable(void);
const char * luna_ota_getUpdateVersion(void);
const char * luna_ota_getUpdateFname(void);
void luna_ota_clear_info(char * const filename);
int luna_ota_get_progress(void);
char * luna_ota_get_status(void);
int luna_ota_get_fwLength(void);