#pragma once

#include "host/ble_hs.h"
// #include "sys/queue.h"

#ifdef __cplusplus
extern "C"
{
#endif

    struct ble_node
    {
        ble_addr_t address;
        int rssi;
        char name[31];
        uint32_t pk;
    };

#define MAX_SCAN_LENGTH 16

    struct node_item
    {
        SLIST_ENTRY(node_item)
        next;
        struct ble_node node;
    };

    extern int scan_init(int max_node_items);
    // struct node_item * node_item_find(ble_addr_t address);
    extern int scan_update_nodes(struct ble_gap_disc_desc desc, char *const name, uint8_t len);
    extern int scan_get_count(void);
    struct node_item * scan_get_nodes(void);
    extern void scan_node_find(uint8_t * addr_val, struct ble_node *);
    extern void scan_node_find_by_name(uint8_t * addr_val, struct ble_node *);

#ifdef __cplusplus
}
#endif