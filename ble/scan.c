#include "scan.h"
#include "host/ble_hs.h"
#include <assert.h>
// #include "sys/queue.h"
#include <string.h>

#include "misc.h"

#define LOGI(format, ...)                                                     \
  ESP_LOGI ("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__,     \
            ##__VA_ARGS__)
#define LOGD(format, ...)                                                     \
  ESP_LOGD ("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__,     \
            ##__VA_ARGS__)
#define LOGW(format, ...)                                                     \
  ESP_LOGW ("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__,     \
            ##__VA_ARGS__)
#define LOGE(format, ...)                                                     \
  ESP_LOGE ("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__,     \
            ##__VA_ARGS__)
#define LOG_BUF(buf, len)                                                     \
  ESP_LOG_BUFFER_HEXDUMP (__FILE__, buf, len, ESP_LOG_INFO)

static const char LUNA_NAME_FILTER[] = "@Luna-";

static void *nodes_mem;
static volatile struct os_mempool nodes_pool = { 0 };
static volatile SLIST_HEAD (node_item_list, node_item) nodes;

static volatile SLIST_HEAD (pnode_item_list, node_item) *_pnodes = &nodes;

static uint32_t _total_nodes = 0;

void
scan_node_find (uint8_t *addr_val, struct ble_node *node)
{
  LOGD ("%d", _total_nodes);
  if (_total_nodes > 0)
    {
      struct node_item *n_i;
      SLIST_FOREACH (n_i, &nodes, next)
      {
        if (0 == memcmp (n_i->node.address.val, addr_val, 6))
          {
            LOGD ("Found node %s", n_i->node.name);
            *node = n_i->node;
            return;
          }
      }
    }
}

void
scan_node_find_by_name (uint8_t *name, struct ble_node * node)
{
    if (_total_nodes > 0)
    {
      struct node_item *n_i;
      SLIST_FOREACH (n_i, &nodes, next)
      {
        if (0 == memcmp (n_i->node.name, name, strlen((char*)name)))
          {
            LOGD ("Found node %s", n_i->node.name);
            *node = n_i->node;
            return;
          }
      }
    }
}

static void
scan_free_mem (void)
{
  free (nodes_mem);
  nodes_mem = NULL;
  _total_nodes = 0;
  SLIST_INIT (&nodes);
}

int
scan_update_nodes (struct ble_gap_disc_desc desc, char *const name,
                   uint8_t len)
{
  LOGD ("%d", _total_nodes);
  if (NULL == name || 0 == len)
    return 0;

  if (NULL == strstr ((char *)desc.data, LUNA_NAME_FILTER))
    {
      // LOGE("Not Luna devace %s", desc.data);
      //  LOG_BUF(desc.data, desc.length_data);
      return 0;
    }

  struct node_item *item = NULL;
  // item = scan_node_find (desc.addr.val);

  SLIST_FOREACH (item, &nodes, next)
  {
    if (0 == memcmp (item->node.address.val, desc.addr.val, 6))
      break;
  }

  // New node add
  if (item == NULL)
    {
      item = os_memblock_get (&nodes_pool);

      if (item == NULL)
        {
          return BLE_HS_ENOMEM;
        }

      memset (&item->node, 0, sizeof (struct ble_node));
      SLIST_INSERT_HEAD (&nodes, item, next);
      LOGD ("Added new node %s", addr_str (desc.addr.val));
      ++_total_nodes;
    }

  memcpy (&item->node.address, &desc.addr, sizeof (ble_addr_t));
  memcpy (item->node.name, name, len);
  item->node.rssi = desc.rssi;
  return 0;
}

extern int
scan_init (int max_nodes)
{
  LOGI ();
  int rc;
  scan_free_mem ();

  nodes_mem = malloc (OS_MEMPOOL_BYTES (max_nodes, sizeof (struct node_item)));
  if (nodes_mem == NULL)
    {
      rc = BLE_HS_ENOMEM;
      goto err;
    }

  rc = os_mempool_init ((struct os_mempool *)&nodes_pool, max_nodes, sizeof (struct node_item),
                        nodes_mem, "Nodes_Pool");
  if (rc != 0)
    {
      rc = BLE_HS_EOS;
      goto err;
    }
  LOGD ("Inited %s memory pool", nodes_pool.name);
  return 0;

err:
  LOGE ("ERROR %d", rc);
  scan_free_mem ();
  return rc;
}

int
scan_get_count (void)
{
  return _total_nodes;
}

struct node_item *
scan_get_nodes (void)
{
  if (_total_nodes > 0)
    return _pnodes;

  return NULL;
}