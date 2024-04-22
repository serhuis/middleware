#include "zhaga_central.h"
#include "esp_log.h"
#include "nvs_flash.h"
/* BLE */
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

#include "peer.h"
#include "misc.h"
#include "ble_luna_defs.h"
#include "zhaga_gatt_client.h"
#include "luna_ota.h"
#include "nv_memory.h"
#include "scan.h"
// #include "ws.h"
#define LOGI(format, ...) ESP_LOGI("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGD(format, ...) ESP_LOGD("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(format, ...) ESP_LOGW("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(format, ...) ESP_LOGE("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOG_BUF(buf, len) ESP_LOG_BUFFER_HEXDUMP(__FILE__, buf, len, ESP_LOG_INFO)

// static uint8_t peer_addr[6];
typedef enum
{
    Disconnected = 0,
    Scan,
    Connecting,
    Connected,

} conn_status_t;

static uint16_t _conn_handle = -1;
// static bool _bond = false;
static struct ble_node _paired_node = {0};
static conn_status_t _conn_status = Disconnected;
// static volatile ble_addr_t _addr;
// static char _addr_str[] = CONFIG_DEFAULT_PEER_ADDR;
// static uint32_t _passkey = CONFIG_LUNA_PASS_KEY;
static bool _autoconnect = true;
static TimerHandle_t _xtimer;
static StaticTimer_t _xTimerBuffer;

void ble_store_config_init(void);
void zhaga_cent_host_task(void *param);
static void zhaga_cent_on_reset(int reason);
static void zhaga_cent_on_sync(void);
static void on_gatt_connected_cb(void);

static void vTimerCallback(TimerHandle_t xTimer);
static int zhaga_cent_gap_event(struct ble_gap_event *event, void *arg);

// static void zhaga_cent_print_keys(void);
static void passkey_entry(uint16_t conn_handle);
int zhaga_cent_write(uint16_t conn_handle, ble_uuid_t *svc_uuid, ble_uuid_t *chr_uuid, uint8_t *const data, size_t length);
static esp_err_t zhaga_cent_nvs_get_addr(void);
static esp_err_t zhaga_cent_nvs_get_pass(void);
static esp_err_t zhaga_cent_nvs_get_paired(void);

const char *zhaga_cent_nvs = "zhaga_ble";
const uint32_t _scan_time = 60000;

#define BT_NIMBLE_MAX_LEN (CONFIG_BT_NIMBLE_MAX_CONNECTIONS + \
                           CONFIG_BT_NIMBLE_MAX_BONDS +       \
                           CONFIG_BT_NIMBLE_MAX_CCCDS)
const char *keys_names[] = {
    "our_sec_1",
    "our_sec_2",
    "our_sec_3",
    "peer_sec_1",
    "peer_sec_2",
    "peer_sec_3",
    "cccd_sec_1",
    "cccd_sec_2",
    "cccd_sec_3",
    "cccd_sec_4",
    "cccd_sec_5",
    "cccd_sec_6",
    "cccd_sec_7",
    "cccd_sec_8",
};

const char *_dev_adds_nvs = "ble_addr";
const char *_dev_key_nvs = "ble_pass";
const char *_dev_paired_nvs = "ble_paired";

extern void zhaga_cent_init(void)
{
    int rc;
    LOGI();
    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());

    /* Configure the host. */
    ble_hs_cfg.reset_cb = zhaga_cent_on_reset;
    ble_hs_cfg.sync_cb = zhaga_cent_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_KEYBOARD_DISPLAY;
    ble_hs_cfg.sm_our_key_dist = 0x0b;
    ble_hs_cfg.sm_their_key_dist = 0x0b;
    // zhaga_cent_print_keys();

    if (ESP_OK != zhaga_cent_nvs_get_paired())
        zhaga_cent_reset_paired();

    nimble_port_init();
    /* Initialize data structures to track connected peers. */
    rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("nimble-blecent");
    assert(rc == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();
    _xtimer = xTimerCreateStatic("Timer", pdMS_TO_TICKS(_scan_time), pdFALSE, (void *)0, vTimerCallback, &_xTimerBuffer);
    nimble_port_freertos_init(zhaga_cent_host_task);
}
/*
static void zhaga_cent_print_keys(void)
{
    nvs_handle_t my_handle;
    for (int i = 0; i < BT_NIMBLE_MAX_LEN; i++)
    {
        esp_err_t err = nvs_open(zhaga_cent_nvs, NVS_READWRITE, &my_handle);
        if (err == ESP_OK)
        {
            size_t required_size = 0; // value will default to 0, if not set yet in NVS
            err = nvs_get_blob(my_handle, keys_names[i], NULL, &required_size);
            LOGD("%s size = %d", keys_names[i], required_size);
            if ((err == ESP_OK) && (required_size > 0))
            {
                char *buf = malloc(required_size);
                err = nvs_get_blob(my_handle, keys_names[i], buf, &required_size);
                // LOG_BUF(buf, required_size);
                free(buf);
            }
            nvs_close(my_handle);
        }
    }
}
*/
static esp_err_t zhaga_cent_nvs_get_addr(void)
{
    esp_err_t err = NVMemory_getBlob(_dev_adds_nvs, (uint8_t *)&_paired_node.address, sizeof(_paired_node.address));
    LOGI("%s", addr_str(_paired_node.address.val));
    // LOG_BUF(_paired_node.address.val, 6);
    return err;
}

static esp_err_t zhaga_cent_nvs_get_pass(void)
{
    esp_err_t err = NVMemory_getBlob(_dev_key_nvs, (uint8_t *)&_paired_node.pk, sizeof(_paired_node.pk));
    LOGD("%d", _paired_node.pk);
    return err - (0 == _paired_node.pk) ? 1 : 0;
}

static esp_err_t zhaga_cent_nvs_get_paired(void)
{
    bzero(&_paired_node, sizeof(_paired_node));
    esp_err_t err = NVMemory_getBlob(_dev_paired_nvs, (uint8_t *)&_paired_node, sizeof(_paired_node));
    if (ESP_OK == err)
        LOGD("Paired: %s, %d", _paired_node.name, _paired_node.pk);
    return err - (0 == _paired_node.pk) ? 1 : 0;
}

static esp_err_t zhaga_cent_nvs_set_paired(struct ble_node *node)
{
    return NVMemory_setBlob(_dev_paired_nvs, (uint8_t *)node, sizeof(struct ble_node));
}

void zhaga_cent_host_task(void *param)
{
    LOGD("BLE Host Task Started");
    zhaga_gatt_init();
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/**
 * Initiates the GAP general discovery procedure.
 */
void zhaga_cent_scan(void)
{
    LOGD();
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params = {
        .filter_duplicates = 1,
        .passive = 0,
        .itvl = 0,
        .window = 0,
        .filter_policy = 0,
        .limited = 0,
    };
    int rc = scan_init(MAX_SCAN_LENGTH);
    assert(rc == 0);

    ble_hs_id_infer_auto(0, &own_addr_type);
    ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params, zhaga_cent_gap_event, NULL);
    _conn_status = Scan;
    xTimerStart(_xtimer, pdMS_TO_TICKS(_scan_time));
}

static void vTimerCallback(TimerHandle_t xTimer)
{
    LOGD("Scan finished. Found %d devices.", scan_get_count());
    ble_gap_disc_cancel();
    /*
        if (NULL != scan_node_find(_paired_node.address.val))
            zhaga_cent_connect(_paired_node.address.val);
    */
}

/**
 * Indicates whether we should try to connect to the sender of the specified
 * advertisement.  The function returns a positive result if the device
 * advertises connectability and support for the Alert Notification service.
 */
static bool zhaga_cent_should_connect(const struct ble_gap_disc_desc *disc)
{
    if (_autoconnect)
    {
        return (memcmp(_paired_node.address.val, disc->addr.val, sizeof(disc->addr.val)) == 0);
    }
    return false;
}

/**
 * Connects to the sender of the specified advertisement of it looks
 * interesting.  A device is "interesting" if it advertises connectability and
 * support for the Alert Notification service.
 */
static void zhaga_cent_connect_if_interesting(const struct ble_gap_disc_desc *disc)
{
    if (zhaga_cent_should_connect(disc))
    {
        zhaga_cent_connect(disc->addr.val);
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.  blecent uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  blecent.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int zhaga_cent_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int ret = ESP_OK;
//    struct node n = {0};

    switch (event->type)
    {
    case BLE_GAP_EVENT_DISC:
        ret = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);

        if (ret != 0)
        {
            LOGE("BLE_GAP_EVENT_DISC ERROR: %d", ret);
            break;
        }
        // print_adv_fields(&fields);
        scan_update_nodes(event->disc, fields.name, fields.name_len);
        struct ble_node n = {0};
        scan_node_find(_paired_node.address.val, &n);
        if (n.rssi != 0)
        {
            _paired_node = n;
            /*
            bzero(_paired_node.name, sizeof(_paired_node.name));
            memcpy(_paired_node.name, n->name, strlen(n->name));*/
        }
        zhaga_cent_connect_if_interesting(&event->disc);
        break;

    case BLE_GAP_EVENT_CONNECT:
        LOGD("BLE_GAP_EVENT_CONNECT: %d", event->type);
        if (0 != event->connect.status)
        {
            LOGE("Error: Connection failed; status=%d\n", event->connect.status);
            zhaga_cent_scan();
            ret = event->connect.status;
            break;
        }
        ble_gap_conn_find(event->connect.conn_handle, &desc);
        LOGD("Connected to %s, bonded: %d, autenticated: %d", addr_str(desc.peer_ota_addr.val), desc.sec_state.bonded, desc.sec_state.authenticated);
        ble_gap_security_initiate(event->connect.conn_handle);
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        LOGD("BLE_GAP_EVENT_DISCONNECT: %d, reason: %x", event->type, event->disconnect.reason);
        ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        peer_delete(event->disconnect.conn.conn_handle);
        ble_gap_unpair(&desc.peer_id_addr);
        zhaga_cent_scan();
        _conn_status = Disconnected;
        break;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        LOGI("BLE_GAP_EVENT_DISC_COMPLETE: %d, reason=%x", event->type, event->disc_complete.reason);
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        LOGD("BLE_GAP_EVENT_ENC_CHANGE: peer: %s,  bonded: %d, autenticated: %d, status: %d",
             addr_str(desc.peer_ota_addr.val), desc.sec_state.bonded, desc.sec_state.authenticated, event->enc_change.status);
        // print_conn_desc(&desc);
        if ((ESP_OK == event->enc_change.status) && (0 != desc.sec_state.authenticated) && (0 != desc.sec_state.bonded))
        {
            _conn_handle = event->enc_change.conn_handle;
            peer_add(_conn_handle);
            zhaga_gatt_exchange_mtu(_conn_handle);
        }
        break;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        LOGI("PASSKEY_ACTION_EVENT: action=%d", event->passkey.params.action);
        passkey_entry(event->passkey.conn_handle);
        break;

    case BLE_GAP_EVENT_NOTIFY_RX:
        LOGI("BLE_GAP_EVENT_NOTIFY_RX: %d", event->type);
        LOGD("received %s; conn_handle=%d attr_handle=%d attr_len=%d\n",
             event->notify_rx.indication ? "indication" : "notification",
             event->notify_rx.conn_handle,
             event->notify_rx.attr_handle,
             OS_MBUF_PKTLEN(event->notify_rx.om));
        size_t len = zhaga_gatt_on_notify(event->notify_rx.conn_handle, event->notify_rx.attr_handle, event->notify_rx.om);
        if (zhaga_gatt_is_oad())
            luna_ota_rcv_evt(len);
        uint8_t buf[32] = {0};
        len = zhaga_gatt_read(buf, sizeof buf);
        break;

    case BLE_GAP_EVENT_MTU:
        LOGD("BLE_GAP_EVENT_MTU: channel_id: %d,  mtu: %d\n", event->mtu.channel_id, event->mtu.value);
        zhaga_gatt_disc_all(_conn_handle, on_gatt_connected_cb);
        _conn_status = Connecting;
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        LOGW("BLE_GAP_EVENT_REPEAT_PAIRING: %d", event->type);
        if (ESP_OK == ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc))
            ble_store_util_delete_peer(&desc.peer_id_addr);
        ret = BLE_GAP_REPEAT_PAIRING_RETRY;
        break;

    default:
        LOGE("Non processed event: %d", event->type);
    }
    return ret;
}

static void zhaga_cent_on_reset(int reason)
{
    LOGE("Resetting state; reason=%d\n", reason);
}

static void zhaga_cent_on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    zhaga_cent_scan();
}

static void on_gatt_connected_cb(void)
{
    xTimerStop(_xtimer, pdMS_TO_TICKS(100));
    _conn_status = Connected;
}

static void passkey_entry(uint16_t conn_handle)
{
    struct ble_sm_io pk = {.action = BLE_SM_IOACT_INPUT, .passkey = zhaga_gatt_is_oad() ? CONFIG_LUNA_OAD_PASS_KEY : _paired_node.pk};
    LOGW("pk.passkey: %d", pk.passkey);
    if (ESP_OK != ble_sm_inject_io(conn_handle, &pk))
    {
        LOGE("ble_sm_inject_io(%d, %d) error", conn_handle, pk.passkey);
    }
}

extern size_t zhaga_cent_send(uint8_t *const data, const size_t length)
{
    return zhaga_gatt_write(_conn_handle, data, length);
}

extern size_t zhaga_cent_receive(uint8_t *const data, const size_t length)
{
    return zhaga_gatt_read(data, length);
}

extern size_t zhaga_cent_reset(const uint8_t flag)
{
    static bool connect_backup = false;
    LOGI("%d", flag);
    if (1 == flag)
    {
        connect_backup = zhaga_cent_get_autoconnect();
        zhaga_cent_set_autoconnect(true);
    }
    else
        zhaga_cent_set_autoconnect(connect_backup);
    zhaga_gatt_send_reset(_conn_handle, flag);
    zhaga_cent_scan();
    return 0;
}

// Extarnal API
esp_err_t zhaga_cent_disconnect(void)
{
    ble_gap_terminate(_conn_handle, BLE_ERR_CONN_TERM_LOCAL);
    LOGI("%s", _paired_node.name);
    return 0;
}

char *zhaga_cent_get_status(void)
{
    if (_conn_status == Disconnected)
        return "Disconnected";
    if (_conn_status == Connecting)
        return "Connecting...";
    if (_conn_status == Connected)
        return "Connected";
    if (_conn_status == Scan)
        return "Searching...";
    return "Unknown";
}

size_t zhaga_cent_get_paired_addr(uint8_t *const buffer, const size_t length)
{
    if (NULL != buffer && (length >= sizeof(_paired_node.address.val)))
    {
        memcpy(buffer, _paired_node.address.val, sizeof(_paired_node.address.val));
        return sizeof(_paired_node.address.val);
    }
    return 0;
}

char *zhaga_cent_get_paired_name(void)
{
    LOGD("%s", _paired_node.name);
    return (char*)_paired_node.name;
}

void zhaga_cent_connect(uint8_t *const addr)
{
    if (NULL == addr)
        return;

    uint8_t own_addr_type;
    ble_addr_t peer_addr = {.type = 0};

    memcpy(peer_addr.val, addr, sizeof(peer_addr.val));

    int rc = ble_gap_disc_cancel();
    if ((rc != 0) && (rc != BLE_HS_EALREADY))
    {
        LOGD("Failed to cancel scan; rc=%d\n", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        LOGE("error determining address type; rc=%d\n", rc);
        return;
    }

    rc = ble_gap_connect(0, &peer_addr, 30000, NULL, zhaga_cent_gap_event, NULL);
    if (rc != 0)
    {
        LOGE("Error: Failed to connect to device; addr_type=%d addr=%s; rc=%x\n", peer_addr.type, addr_str(peer_addr.val), rc);
    }
}

void zhaga_cent_connect_paired(void)
{
    zhaga_cent_connect(_paired_node.address.val);
}

bool zhaga_cent_get_autoconnect(void)
{
    LOGD("%d", _autoconnect);
    return _autoconnect;
}

void zhaga_cent_set_autoconnect(const int connect)
{
    LOGD("%d", connect);
    _autoconnect = (bool)connect;
}

esp_err_t zhaga_cent_set_paired_addr(uint8_t *const addr)
{
    // LOGI();
    LOGD("%s", addr_str(((ble_addr_t *)addr)->val));
    /*  LOG_BUF(addr, 6);
      if (addr)
      {
          if (memcmp(addr, &_paired_node.address.val, sizeof(_paired_node.address.val)))
          {
              ble_gap_unpair(&_paired_node.address);
              memcpy(_paired_node.address.val, addr, sizeof(_paired_node.address.val));
              // LOGI("%s", _addr_str);
              NVMemory_setBlob(_dev_adds_nvs, (uint8_t *)&_paired_node.address, sizeof(_paired_node.address));
          }
          return ESP_OK;
      }
  */
    return ESP_FAIL;
}

esp_err_t zhaga_cent_set_paired_passkey(const uint32_t passkey)
{
    LOGD("%d", passkey);
    /*
    if (passkey)
    {
        if (passkey == _paired_node.pk)
            return ESP_OK;
        ble_gap_unpair(&_paired_node.address);
        _paired_node.pk = passkey;
        LOGI("%d", _paired_node.pk);
        return NVMemory_setBlob(_dev_key_nvs, &_paired_node.pk, sizeof(_paired_node.pk));
    }
    */
    return ESP_FAIL;
}
/*
esp_err_t zhaga_cent_set_paired(uint8_t *const addr, uint32_t passkey)
{
    LOGW("%s, %d", addr_str(addr), passkey);

    struct ble_node node;
    scan_node_find(addr, &node);
    if (node.rssi)
    {
        LOGI("Found node: %s [%s], rssi: %d", node.name, addr_str(addr), node.rssi);
        node.pk = passkey;
        _paired_node = node;
        zhaga_cent_nvs_set_paired(&node);
        return NVMemory_setBlob(_dev_paired_nvs, (uint8_t *)&node, sizeof(struct ble_node));
    }
    LOGE("node %s not found", addr_str(addr));
    return ESP_OK;
}
*/
esp_err_t
zhaga_cent_set_paired_name (char *const name)
{
    LOGW("%s, %d", name);
    struct ble_node node = {0};
    scan_node_find_by_name((void*)name, &node);
    if (node.rssi)
    {
        LOGI("Found node: %s [%s], rssi: %d", node.name, addr_str(node.address.val), node.rssi);
        //node.pk = passkey;
        _paired_node = node;
        zhaga_cent_nvs_set_paired(&node);
        return NVMemory_setBlob(_dev_paired_nvs, (uint8_t *)&node, sizeof(struct ble_node));
    }
    LOGE("node %s not found", name);
  return ESP_OK;
}

esp_err_t zhaga_cent_reset_paired(void)
{
    LOGD("%s, %d", CONFIG_DEFAULT_PEER_ADDR, CONFIG_LUNA_PASS_KEY);
    bzero((void *)&_paired_node, sizeof(_paired_node));
    sscanf(CONFIG_DEFAULT_PEER_ADDR, "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx",
           &_paired_node.address.val[5], &_paired_node.address.val[4], &_paired_node.address.val[3],
           &_paired_node.address.val[2], &_paired_node.address.val[1], &_paired_node.address.val[0]);
    sprintf(_paired_node.name, "Not found");
    _paired_node.pk = CONFIG_LUNA_PASS_KEY;
    LOGI("%s[%s], %d", _paired_node.name, addr_str(&_paired_node.address.val), _paired_node.pk);
    return ESP_OK;
}
