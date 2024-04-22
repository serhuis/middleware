#include <limits.h>
#include "mediator.h"
#include "transport_tcp.h"
#include "unity.h"


TEST_CASE("Test mediator -> Mtor_rcvCb", "[Mtor][rcvCb]")
{
	static volatile uint8_t in_data[] = { 0xff, 0x04, 0x71, 0x01, 0x01, 0xff };
	//Mtor_rcvCb(1, in_data, sizeof(in_data));
	TransportTcp_msgCb(3, 32, in_data, sizeof(in_data));
	///TEST_ASSERT_EQUAL_UINT16(5, 5);
}
