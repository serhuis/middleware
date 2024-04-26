idf_component_register(SRCS "functionEsp/fnEsp.c" "functionEsp/dcdEsp.c"
                            "functionBle/fnBle.c" "functionBle/dcdBle.c"
                    INCLUDE_DIRS "." "functionEsp" "functionBle"
                    REQUIRES 
                    mediator 
                    drvGpio
                    wifi 
                    ble)