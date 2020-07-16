//-------------------------------------------------------------------------------------------------
/**
 * @file coaptests.c
 *
 * Unitary test for er-coap
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//-------------------------------------------------------------------------------------------------


#include "internals.h"
#include "liblwm2m.h"
#include "er-coap-13.h"

#include "tests.h"
#include "CUnit/Basic.h"

//--------------------------------------------------------------------------------------------------
/**
 * Tests the case where the server hasn't received all the blocks necessary to proceed
 */
//--------------------------------------------------------------------------------------------------
static void test_coap_bad_option(void)
{
    coap_status_t status;

    // Simulate an incorrect CoAP message (invalid option length)
    uint8_t data[] = {0x44, 0x02, 0xE6, 0xE2, 0xE2, 0xE6, 0x81, 0x67, 0xB2, 0x72, 0x64, 0x11};
    uint16_t data_len = 12;
    static coap_packet_t message[1];

    status = coap_parse_message(message, data, data_len);
    CU_ASSERT_EQUAL(status, BAD_REQUEST_4_00);
}

//--------------------------------------------------------------------------------------------------
/**
 * Tests the case where the CoAP version is incorrect
 */
//--------------------------------------------------------------------------------------------------
static void test_coap_bad_version(void)
{
    coap_status_t status;

    // Simulate an incorrect CoAP message (invalid CoAP version)
    uint8_t data[] = {0x84, 0x01, 0xC4, 0x09, 0x74, 0x65, 0x73, 0x74, 0xB7, 0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65};
    uint16_t data_len = 16;
    static coap_packet_t message[1];

    status = coap_parse_message(message, data, data_len);
    CU_ASSERT_EQUAL(status, BAD_REQUEST_4_00);
}

//--------------------------------------------------------------------------------------------------
/**
 * Tests the case where the proxy-URI option is filled
 */
//--------------------------------------------------------------------------------------------------
static void test_coap_proxy_uri(void)
{
    coap_status_t status;

    // Simulate an incorrect CoAP message (Proxy-URI is filled)
    // Location is filled and proxy-uri
    uint8_t data[] = {0x44, // Version and TKL
                      0x01, // Get method
                      0xC4, 0x09, // MID
                      0x74, 0x65, 0x73, 0x74, // Token
                      0x8a, 0x69, 0x73, 0x5a, 0x77, 0x30, 0x62, 0x54, 0x45, 0x64, 0x38, // location option
                      0xDD, 0x0E, 0x02, 0x74, 0x65, 0x73, 0x74, 0x6F, 0x66, 0x61, 0x70, 0x72, 0x6F, 0x78, 0x79, 0x75, 0x72, 0x6C}; // proxy-uri option
    uint16_t data_len = 37;
    static coap_packet_t message[1];

    status = coap_parse_message(message, data, data_len);
    CU_ASSERT_EQUAL(status, PROXYING_NOT_SUPPORTED_5_05);
}

//--------------------------------------------------------------------------------------------------
/**
 * Structure for all CoAP tests
 */
//--------------------------------------------------------------------------------------------------
static struct TestTable table[] = {
        { "test of test_coap_bad_option()\n", test_coap_bad_option },
        { "test of test_coap_bad_version()\n", test_coap_bad_version },
        { "test of test_coap_proxy_uri()\n", test_coap_proxy_uri },
        { NULL, NULL },
};

//--------------------------------------------------------------------------------------------------
/**
 * Function for CoAP tests suite
 */
//--------------------------------------------------------------------------------------------------
CU_ErrorCode create_coap_suit
(
    void
)
{
    CU_pSuite pSuite = NULL;
    printf("1\r\n");
    pSuite = CU_add_suite("Suite_coap", NULL, NULL);

    if (NULL == pSuite) {
        return CU_get_error();
    }

    return add_tests(pSuite, table);
}
