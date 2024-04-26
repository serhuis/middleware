/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.    You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string.h>
#include <errno.h>
#include <assert.h>

#include "esp_log.h"

#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gatt/ble_svc_gatt.h"
//#include "../../../nimble/host/src/ble_att_priv.h"
//#include "../../../nimble/host/src/ble_gatt_priv.h"
#include "host/ble_gatt.h"

#include "zhaga_gatt_client.h"
#include "ble_luna_defs.h"
#include "peer.h"
#include "luna_ota.h"

#define CONTROLLER_INDEX 0
#define MAX_BUFFER_SIZE 256

#define LOGI(format, ...) ESP_LOGI("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGD(format, ...) ESP_LOGD("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(format, ...) ESP_LOGW("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(format, ...) ESP_LOGE("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOG_BUF(buf, len) ESP_LOG_BUFFER_HEXDUMP(__FILE__, buf, len, ESP_LOG_INFO)

/* 0000xxxx-8c26-476f-89a7-a108033a69c7 */
/*
#define PTS_UUID_DECLARE(uuid16)                            \
    ((const ble_uuid_t *)(&(ble_uuid128_t)BLE_UUID128_INIT( \
        0xc7, 0x69, 0x3a, 0x03, 0x08, 0xa1, 0xa7, 0x89,     \
        0x6f, 0x47, 0x26, 0x8c, uuid16, uuid16 >> 8, 0x00, 0x00)))
*/
/* 0000xxxx-8c26-476f-89a7-a108033a69c6 */
/*
#define PTS_UUID_DECLARE_ALT(uuid16)                        \
    ((const ble_uuid_t *)(&(ble_uuid128_t)BLE_UUID128_INIT( \
        0xc6, 0x69, 0x3a, 0x03, 0x08, 0xa1, 0xa7, 0x89,     \
        0x6f, 0x47, 0x26, 0x8c, uuid16, uuid16 >> 8, 0x00, 0x00)))

static uint8_t notify_state;
static uint16_t myconn_handle;
*/
/*
 * gatt_buf - cache used by a gatt client (to cache data read/discovered)
 * and gatt server (to store attribute user_data).
 * It is not intended to be used by client and server at the same time.
 */

static struct gatt_buf_t
{
    uint16_t wr_idx;
    uint16_t rd_idx;
    uint8_t buf[MAX_BUFFER_SIZE];
};

static bool _oad_mode = false;
static ble_uuid_t *_luna_svc_uuid = NULL;
static ble_uuid_t *_chr_write_uuid = NULL;
static ble_uuid_t *_chr_notify_uuid = NULL;
static ble_uuid_t *_chr_aux_uuid = NULL;
static ble_uuid_t *_ccc_uuid = NULL;
static struct gatt_buf_t _rx_buffer;


char svc_uuid_str[BLE_UUID_STR_LEN];
char chr_uuid_str[BLE_UUID_STR_LEN];

static struct peer_svc *zhaga_gatt_search_service(const struct peer *peer);
static void zhaga_gatt_on_disc_complete(const struct peer *peer, int status, void *arg);
static int zhaga_gatt_on_subscribe(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
static int zhaga_gatt_on_write(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
static size_t gatt_buf_write(const void *data, size_t len);
static size_t gatt_buf_read(const void *data, size_t len);
static void gatt_buf_clear(void);

static int zhaga_mtu_cb(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t mtu, void *arg);
static void (*_on_connected_cb)(void) = NULL;

extern void zhaga_gatt_init(void)
{
    gatt_buf_clear();
}

extern void zhaga_gatt_disc_all(uint16_t conn_handle, void (*connected_cb)(void))
{
    _on_connected_cb = connected_cb;
    LOGD("%d", conn_handle);
    peer_disc_all(conn_handle, zhaga_gatt_on_disc_complete, NULL);
}

extern void zhaga_gatt_subscribe(uint16_t conn_handle, ble_uuid_t *svc_uuid, ble_uuid_t *chr_uuid)
{
    const struct peer_dsc *dsc;
    uint8_t value[2] = {1, 0};
    int rc;
    const struct peer *peer = peer_find(conn_handle);

    LOGD("Subscribe request sent to %s : %s \n", ble_uuid_to_str(svc_uuid, svc_uuid_str), ble_uuid_to_str(chr_uuid, chr_uuid_str));

    dsc = peer_dsc_find_uuid(peer, svc_uuid, chr_uuid, BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (dsc)
    {
        rc = ble_gattc_write_flat(conn_handle, dsc->dsc.handle, value, sizeof value, zhaga_gatt_on_subscribe, chr_uuid);
        if (rc != 0)
        {
            LOGE("Error: Failed to subscribe to characteristic; rc=%d\n", rc);
            ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
    }
}

extern int zhaga_gatt_write(uint16_t conn_handle, uint8_t *const data, size_t length)
{
    LOGI();
    const struct peer_chr *chr;
    int rc;
    const struct peer *peer = peer_find(conn_handle);
    char buf[BLE_UUID_STR_LEN];
    if (!_luna_svc_uuid || !_chr_write_uuid || !peer)
    {
        LOGE("ERROR: conn_handle, %x, _luna_svc_uuid: %s, _chr_write_uuid: %s",
             conn_handle, ble_uuid_to_str(_luna_svc_uuid, buf), ble_uuid_to_str(_chr_write_uuid, buf));
        return 0;
    }
    chr = peer_chr_find_uuid(peer, _luna_svc_uuid, _chr_write_uuid);

    if (chr == NULL)
    {
        LOGE("Service: %s Characteristic %s not found!!",
             ble_uuid_to_str(_luna_svc_uuid, buf), ble_uuid_to_str(_chr_write_uuid, buf));
        return 0;
    }
    LOGD("def_handle: %d, val_handle: %d, properties: %d", chr->chr.def_handle, chr->chr.val_handle, chr->chr.properties);
    rc = ble_gattc_write_flat(conn_handle, chr->chr.val_handle, data, length, zhaga_gatt_on_write, length);
    if (rc != 0)
    {
        LOGE("Error: Failed to write characteristic; rc=%d\n", rc);
        return 0;
    }
    //LOG_BUF(data, length);
    return length;
}

extern int zhaga_gatt_send_reset(uint16_t conn_handle, const uint8_t flag)
{
    const struct peer_chr *chr;
    int rc;
    const struct peer *peer = peer_find(conn_handle);
    char buf[BLE_UUID_STR_LEN];
    if (!_luna_svc_uuid || !_chr_aux_uuid || !peer)
    {
        LOGE();
        return 0;
    }
    chr = peer_chr_find_uuid(peer, _luna_svc_uuid, _chr_aux_uuid);

    if (chr == NULL)
    {
        LOGE("Service or Characteristic  not found!!");
        return 0;
    }
    uint8_t value[] = {flag};
    LOGD("%d", flag);
    rc = ble_gattc_write_flat(conn_handle, chr->chr.val_handle, value, 1, zhaga_gatt_on_write, sizeof(value));
    return 1;
}

extern bool zhaga_gatt_is_oad(void)
{
    LOGD("%d", _oad_mode);
    return _oad_mode;
}

extern size_t zhaga_gatt_on_notify(uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om)
{
    /** Chained memory buffer. struct os_mbuf {
        // Current pointer to data in the structure
        uint8_t *om_data;

        //Flags associated with this buffer, see OS_MBUF_F_* defintions
        uint8_t om_flags;

        // Length of packet header
        uint8_t om_pkthdr_len;

        // Length of data in this buffer
        uint16_t om_len;

        // The mbuf pool this mbuf was allocated out of
        struct os_mbuf_pool *om_omp;

        SLIST_ENTRY(os_mbuf) om_next;

        //Pointer to the beginning of the data, after this buffer
        uint8_t om_databuf[0];
    }   */
    size_t l = 0;
    while (om)
    {
        LOGI("om_data %02x, om_len: %02x", *om->om_data, om->om_len);
         //LOG_BUF(om->om_data, om->om_len);

        l += gatt_buf_write(om->om_data, om->om_len);
        om = SLIST_NEXT(om, om_next);
    }
    return l;
}

extern size_t zhaga_gatt_read(uint8_t *const buf, const size_t len)
{
    return gatt_buf_read(buf, len);
}
/* Local event handlers */
static void zhaga_gatt_on_disc_complete(const struct peer *peer, int status, void *arg)
{
    LOGD("Service discovery complete; status=%d conn_handle=%d\n", status, peer->conn_handle);
    if (status != 0)
    {
        /* Service discovery failed.  Terminate the connection. */
        LOGE("Error: Service discovery failed; status=%d conn_handle=%d\n", status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    _ccc_uuid = _chr_write_uuid = _chr_notify_uuid = _chr_aux_uuid = NULL;
    struct peer_svc *svc = zhaga_gatt_search_service(peer);

    if (svc)
    {
        struct peer_chr *chr;
        char buf[BLE_UUID_STR_LEN];
        LOGD("Service: %s", ble_uuid_to_str(&svc->svc.uuid, svc_uuid_str));
        SLIST_FOREACH(chr, &svc->chrs, next)
        {
            ble_uuid_to_str(&chr->chr.uuid, buf);
            LOGD("Characteristic: %s permissions: %02x", buf, chr->chr.properties);

            if (chr->chr.properties == BLE_GATT_CHR_PROP_WRITE)
            {

                _chr_write_uuid = (ble_uuid_t *)&chr->chr.uuid;
                LOGD("BLE_GATT_CHR_PROP_WRITE: %s", ble_uuid_to_str(_chr_write_uuid, chr_uuid_str));
                continue;
            }
            if (chr->chr.properties == BLE_GATT_CHR_PROP_NOTIFY)
            {
                _chr_notify_uuid = (ble_uuid_t *)&chr->chr.uuid;
                const struct peer_dsc *dsc;
                LOGD("BLE_GATT_CHR_PROP_NOTIFY: %s", ble_uuid_to_str(_chr_notify_uuid, chr_uuid_str));

                SLIST_FOREACH(dsc, &chr->dscs, next)
                {
                    _ccc_uuid = (ble_uuid_t *)&dsc->dsc.uuid;
                    LOGD("Descriptor: %s", ble_uuid_to_str(_ccc_uuid, chr_uuid_str));
                }
                continue;
            }
            if (chr->chr.properties == (BLE_GATT_CHR_PROP_WRITE + BLE_GATT_CHR_PROP_WRITE_NO_RSP))
            {
                _chr_aux_uuid = (ble_uuid_t *)&chr->chr.uuid;
            }
        }
    }
    if (_chr_notify_uuid)
        zhaga_gatt_subscribe(peer->conn_handle, (ble_uuid_t *)&svc->svc.uuid, _chr_notify_uuid);
}

static int zhaga_gatt_on_subscribe(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg)
{
    ble_uuid_t *chr_uuid = (ble_uuid_t *)arg;
    LOGD("Subscribe complete; status: %d conn_handle: %d \nCharacteristic: %s\n", error->status, conn_handle, ble_uuid_to_str(chr_uuid, chr_uuid_str));

    if (_on_connected_cb)
        _on_connected_cb();
    /*
        if (_oad_mode)
            luna_ota_start("luna_app.bin");
        else
        {
            static uint8_t value[] = {1};
            zhaga_gatt_send_reset(conn_handle, true);
            LOGI("Inited OAD reset");
        }
        */
    return 0;
}

static int zhaga_gatt_on_write(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg)
{
    LOGD("Written: %d", arg);
    LOGD("Write complete; status=%d conn_handle=%d attr_handle=%d", error->status, conn_handle, attr->handle);
    // LOG_BUF(arg, 16);
    return 0;
}

static struct peer_svc *zhaga_gatt_search_service(const struct peer *peer)
{
    struct peer_svc *svc = NULL;
    _oad_mode = false;

    SLIST_FOREACH(svc, &peer->svcs, next)
    {
        _luna_svc_uuid = NULL;
        LOGD("%s", ble_uuid_to_str(&svc->svc.uuid, svc_uuid_str));
        // LOG_BUF(BLE_UUID128(&svc->svc.uuid)->value, 16);
        if (0 == memcmp(BLE_UUID128(&svc->svc.uuid)->value, (uint8_t[])LUNA_SERVICE_128, 16))
        {
            _luna_svc_uuid = &svc->svc.uuid;
            break;
        }
        if (0 == memcmp(BLE_UUID128(&svc->svc.uuid)->value, (uint8_t[])OAD_SERVICE_128, 16))
        {
            _oad_mode = true;
            _luna_svc_uuid = &svc->svc.uuid;
            break;
        }
    }
    if (!_luna_svc_uuid)
        LOGE("No apropriate service found! Try to reconnect!");

    return svc;
}

/* */
static size_t gatt_buf_write(const void *data, size_t len)
{
    void *ptr = _rx_buffer.buf + _rx_buffer.wr_idx;

    if ((len + _rx_buffer.wr_idx) > MAX_BUFFER_SIZE)
    {
        return 0;
    }

    if (data)
    {
        memcpy(ptr, data, len);
    }
    else
    {
        (void)memset(ptr, 0, len);
    }

    _rx_buffer.wr_idx += len;

    LOGD("%d/%d used", _rx_buffer.wr_idx, MAX_BUFFER_SIZE);

    return _rx_buffer.wr_idx;
}

static size_t gatt_buf_read(const void *data, size_t len)
{
    if (!data)
    {
        return 0;
    }

    size_t l = (len < (_rx_buffer.wr_idx - _rx_buffer.rd_idx)) ? len : (_rx_buffer.wr_idx - _rx_buffer.rd_idx);
    memcpy((const void *)data, (void*)&_rx_buffer.buf[_rx_buffer.rd_idx], l);
    _rx_buffer.rd_idx += l;
    LOGD("%d/%d read", _rx_buffer.rd_idx, _rx_buffer.wr_idx);
    if (_rx_buffer.rd_idx >= _rx_buffer.wr_idx)
    {
        _rx_buffer.rd_idx = _rx_buffer.wr_idx = 0;
    }

    return l;
}

static void gatt_buf_clear(void)
{
    (void)memset(&_rx_buffer, 0, sizeof(_rx_buffer));
}

static int zhaga_mtu_cb(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t mtu, void *arg)
{
    LOGD("status: %d, mtu: %d", error->status, mtu);
    // peer_disc_all(conn_handle, zhaga_gatt_on_disc_complete, NULL);
    return 0;
}

void zhaga_gatt_exchange_mtu(uint16_t conn_handle)
{
    ble_gattc_exchange_mtu(conn_handle, zhaga_mtu_cb, NULL);
}