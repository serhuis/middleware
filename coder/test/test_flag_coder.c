#include <limits.h>

#include "../../coder/flag_coder.h"
#include "unity.h"
#include "esp_log.h"


TEST_CASE("Test Flag_addFlags: out_array length", "[Flag_addFlags][flags][length]")
{
	uint8_t in_data[] = { 0x03, 0x71, 0x01 };
	uint8_t out_data[5] = { 0 };

	TEST_ASSERT_EQUAL_UINT16(5, Flag_addFlags(in_data, 3, out_data, 5));
}

TEST_CASE("Test Flag_addFlags: out_array data", "[Flag_addFlags][flags][data]")
{
    uint8_t in_data[] = { 0x03, 0x71, 0x01 };
    uint8_t out_data[5] = { 0 };
    uint8_t ctrl_data[5] = { 0xff, 0x03, 0x71, 0x01, 0xff };
    Flag_addFlags(in_data, 3, out_data, 5);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ctrl_data, out_data, 5);
}

TEST_CASE("Test Flag_addFlags multiframe", "[Flag_addFlags][flags][multiframe]")
{
    uint8_t in_data[] = { 0x03, 0x71, 0x02, 0x03, 0x71, 0x03 };
    uint8_t out_data[16] = { 0 };
    uint8_t ctrl_data[] = {0xff, 0x03, 0x71, 0x02, 0xff, 0xff, 0x03, 0x71, 0x03, 0xff};
    Flag_addFlags(in_data, sizeof(in_data), out_data, sizeof(out_data));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ctrl_data, out_data, sizeof(ctrl_data));
}

TEST_CASE("Test Flag_rmFlags: out_array length", "[Flag_rmFlags][flags][length]")
{
	static uint8_t in_data[] = { 0xff, 0x03, 0x71, 0x01, 0xff };
	static uint8_t out_data[3] = { 0 };

	TEST_ASSERT_EQUAL_UINT16(3, Flag_rmFlags(in_data, 5, out_data, 3));
}


TEST_CASE("Test Flag_rmFlags out_array data", "[Flag_rmFlags][flags][data]")
{
    static uint8_t in_data[] = { 0xff, 0x03, 0x71, 0x02, 0xff };
    static uint8_t out_data[3] = { 0 };
    static uint8_t ctrl_data[] = {0x03, 0x71, 0x02};
    Flag_rmFlags(in_data, sizeof(in_data), out_data, 3);
}

TEST_CASE("Test Flag_getData same buffer [return]", "[Flag_rmFlags] [flags] [return] [same_buffer]")
{
    uint8_t in_data[] = { 0xff, 0x03, 0x71, 0x02, 0xff };
    uint8_t ctrl_data[] = {0x03, 0x71, 0x02};
    TEST_ASSERT_EQUAL(sizeof(in_data), Flag_getData(in_data, sizeof(in_data), in_data, sizeof(in_data)));
}

TEST_CASE("Test Flag_getData [data]", "[Flag_rmFlags] [flags] [data] [same_buffer]")
{
    uint8_t in_data[] = { 0xff, 0x03, 0x71, 0x02, 0xff };
    uint8_t ctrl_data[] = {0x03, 0x71, 0x02};
    Flag_getData(in_data, sizeof(in_data), in_data, sizeof(in_data));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ctrl_data, in_data, sizeof(ctrl_data));
}


TEST_CASE("Test Flag_rmFlags multiframe", "[Flag_rmFlags][flags][multiframe]")
{
    uint8_t in_data[] = { 0xff, 0x03, 0x71, 0x02, 0xff, 0xff, 0x03, 0x71, 0x03, 0xff };
    uint8_t out_data[16] = { 0 };
    uint8_t ctrl_data[6] = {0x03, 0x71, 0x02, 0x03, 0x71, 0x03};
    Flag_rmFlags(in_data, sizeof(in_data), out_data, sizeof(out_data));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ctrl_data, out_data, 6);
}

TEST_CASE("Test Flag_addHeader data", "[Flag_addHeader][header]")
{
    uint8_t in_data[] = { 0x79};
    uint8_t out_data[16] = { 0 };
    uint8_t ctrl_data[] = {0x03, 0x70, 0x79};
    Flag_addHeader(in_data, sizeof(in_data), out_data, sizeof(out_data));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ctrl_data, out_data, sizeof(ctrl_data));
}

TEST_CASE("Flag_rmHeader data", "[Flag_rmHeader][header]")
{
    uint8_t in_data[] = { 0x03, 0x70, 0x79};
    uint8_t out_data[16] = { 0 };
    uint8_t ctrl_data[] = {0x79};
    Flag_rmHeader(in_data, sizeof(in_data), out_data, sizeof(out_data));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ctrl_data, out_data, sizeof(ctrl_data));
}


/*
TEST_CASE("Mean of a test vector", "[mean]")
{
    const int v[] = {1, 3, 5, 7, 9};
    TEST_ASSERT_EQUAL(5, testable_mean(v, countof(v)));
}

/// This test case currently fails, and developer has added a tag to indicate this.
/// For the test runner, "[fails]" string does not carry any special meaning.
/// However it can be used to filter out tests when running.
////
TEST_CASE("Another test case which fails", "[mean][fails]")
{
    const int v1[] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};
    TEST_ASSERT_EQUAL(INT_MAX, testable_mean(v1, countof(v1)));
}

TEST_CASE("Another EMPTY test case ", "[mean][fails]")
{
    const int v1[] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};
    TEST_ASSERT_EQUAL(INT_MAX, testable_mean(v1, countof(v1)));
}
*/

