#idf_component_register(SRCS "peer.c" "misc.c" "zhaga_central.c" "zhaga_gatt_client.c" "scan.c"
idf_component_register( SRC_DIRS . 
                        INCLUDE_DIRS "." "$ENV{IDF_PATH}/components/bt/host/nimble"
                        REQUIRES bt ota http nv_memory)
