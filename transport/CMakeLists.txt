idf_component_register(SRCS "transport_uart.c" "transport_tcp.c" "transport_ws.c"
                    INCLUDE_DIRS "." 
                    REQUIRES ringbuf mediator coder)

