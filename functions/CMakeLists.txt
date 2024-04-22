idf_component_register(SRCS "functionEsp/fnEsp.c" "functionEsp/dcdEsp.c"
							"functionFuota/fnFuota.c" "functionFuota/dcdFuota.c" 
                            "functionBle/fnBle.c" "functionBle/dcdBle.c"
                    INCLUDE_DIRS "." "functionEsp" "functionFuota" "functionBle"
                    REQUIRES mediator drvGpio wifi board ble)