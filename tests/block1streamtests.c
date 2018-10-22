//-------------------------------------------------------------------------------------------------
/**
 * @file block1streamtests.c
 *
 * Unitary test for coap block1 stream.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//-------------------------------------------------------------------------------------------------


#include "internals.h"
#include "liblwm2m.h"
#include "er-coap-13.h"
#include <lwm2mcore/lwm2mcore.h>
#include "coapHandlers.h"

#include "tests.h"
#include "CUnit/Basic.h"

//--------------------------------------------------------------------------------------------------
/**
 * CoAP test message
 */
//--------------------------------------------------------------------------------------------------
static coap_packet_t TestMessage;


//--------------------------------------------------------------------------------------------------
/**
 * CoAP test payload
 */
//--------------------------------------------------------------------------------------------------
static uint8_t TestPayload[1024];

//--------------------------------------------------------------------------------------------------
/**
 * CoAP received payload
 */
//--------------------------------------------------------------------------------------------------
static uint8_t ReceivedPayload[10*1024];


//--------------------------------------------------------------------------------------------------
/**
 * Total number of bytes received
 */
//--------------------------------------------------------------------------------------------------
static size_t TotalNumBytes = 0;


//--------------------------------------------------------------------------------------------------
/**
 * Setup CoAP test message
 */
//--------------------------------------------------------------------------------------------------
static void setup_test_message(coap_packet_t* testMsg,
                               uint16_t mid,
                               coap_message_type_t type,
                               uint32_t block1_num,
                               uint8_t block1_more,
                               uint16_t block1_size,
                               uint32_t payload_len,
                               uint8_t pattern)
{
    memset(testMsg, 0, sizeof(coap_packet_t));

    testMsg->mid = mid;
    testMsg->type = type;
    coap_set_header_block1(testMsg, block1_num, block1_more, block1_size);
    testMsg->payload_len = payload_len;
    testMsg->payload = TestPayload;

    memset(TestPayload, pattern, sizeof(TestPayload));
}

//--------------------------------------------------------------------------------------------------
/**
 * Handles CoAP messages from server such as read, write, execute and streams (block transfers)
 */
//--------------------------------------------------------------------------------------------------
static void CoapMessageHandler
(
    lwm2mcore_CoapRequest_t* requestRef
)
{
    LOG("Request: CoAP message received from server");

    // Get payload
    uint8_t* payloadPtr = (uint8_t*)lwm2mcore_GetRequestPayload(requestRef);
    size_t payloadLength = lwm2mcore_GetRequestPayloadLength(requestRef);

    LOG_ARG("payloadLength = %d", payloadLength);

    memcpy(ReceivedPayload + TotalNumBytes, payloadPtr, payloadLength);
    TotalNumBytes = TotalNumBytes + payloadLength;

    LOG_ARG("TotalNumBytes = %d", TotalNumBytes);
    LOG_ARG("payload = %d", *payloadPtr);
}

static bool CheckReceivedBytes(size_t numBytes)
{
    size_t i;

    int expData = 0;

    for(i = 0; i < numBytes; i++)
    {
        expData = i / 1024;

        if (ReceivedPayload[i] != expData)
        {
            LOG_ARG("Offset = %d", i);
            LOG_ARG("Received Data = %d", ReceivedPayload[i]);
            LOG_ARG("Expected Data = %d", expData);
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
/**
 * Tests the nominal case where all messages arrive in sequence.
 */
//--------------------------------------------------------------------------------------------------
static void test_block1_stream_nominal(void)
{
    lwm2m_block1_data_t * blk1 = NULL;
    size_t bsize;
    uint8_t st;
    uint8_t *resultBuffer = NULL;
    TotalNumBytes = 0;

    lwm2mcore_SetCoapExternalHandler(CoapMessageHandler);

    memset(TestPayload, 0, sizeof(TestPayload));
    setup_test_message(&TestMessage, 123, COAP_TYPE_CON, 0, 1, 1024, 1024, 0);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_231_CONTINUE);

    memset(TestPayload, 1, sizeof(TestPayload));
    setup_test_message(&TestMessage, 124, COAP_TYPE_CON, 1, 1, 1024, 1024,1);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_231_CONTINUE);

    memset(TestPayload, 2, sizeof(TestPayload));
    setup_test_message(&TestMessage, 125, COAP_TYPE_CON, 2, 1, 1024, 1024, 2);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_231_CONTINUE);

    memset(TestPayload, 3, sizeof(TestPayload));
    setup_test_message(&TestMessage, 126, COAP_TYPE_CON, 3, 0, 1024, 256, 3);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(bsize, 3328);
    bool rc = CheckReceivedBytes(bsize);
    CU_ASSERT_EQUAL(rc, true);

    if (blk1 != NULL)
    {
        free(blk1);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Tests the case where few blocks arrive out-of-order
 */
//--------------------------------------------------------------------------------------------------
static void test_block1_stream_retransmit(void)
{
    lwm2m_block1_data_t * blk1 = NULL;
    size_t bsize;
    uint8_t st;
    uint8_t *resultBuffer = NULL;
    int retransmit_count;

    TotalNumBytes = 0;

    lwm2mcore_SetCoapExternalHandler(CoapMessageHandler);

    // Send block 0
    setup_test_message(&TestMessage, 123, COAP_TYPE_CON, 0, 1, 1024, 1024, 0);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_231_CONTINUE);

    // Try retransmitting block 0 with same message id (should get COAP_IGNORE)
    for (retransmit_count = 0 ; retransmit_count <=1; retransmit_count++)
    {
        setup_test_message(&TestMessage, 123, COAP_TYPE_CON, 0, 1, 1024, 1024, 0xBA);
        st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
        CU_ASSERT_EQUAL(st, COAP_IGNORE);
    }

    // Send block 1
    setup_test_message(&TestMessage, 124, COAP_TYPE_CON, 1, 1, 1024, 1024, 1);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_231_CONTINUE);

    // Try retransmitting block 1 with same message id (should get COAP_IGNORE)
    for (retransmit_count = 0 ; retransmit_count <=1; retransmit_count++)
    {
        setup_test_message(&TestMessage, 124, COAP_TYPE_CON, 1, 1, 1024, 1024, 0xBA);
        st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
        CU_ASSERT_EQUAL(st, COAP_IGNORE);
    }

    // Try retransmitting block 0 again with same message id (should get COAP_IGNORE)
    for (retransmit_count = 0 ; retransmit_count <=1; retransmit_count++)
    {
        setup_test_message(&TestMessage, 123, COAP_TYPE_CON, 0, 1, 1024, 1024, 0xBA);
        st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
        CU_ASSERT_EQUAL(st, COAP_IGNORE);
    }

    // Try retransmitting block 1 again with same message id (should get COAP_IGNORE)
    for (retransmit_count = 0 ; retransmit_count <=1; retransmit_count++)
    {
        setup_test_message(&TestMessage, 124, COAP_TYPE_CON, 1, 1, 1024, 1024, 0xBA);
        st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
        CU_ASSERT_EQUAL(st, COAP_IGNORE);
    }

    // block 2
    setup_test_message(&TestMessage, 125, COAP_TYPE_CON, 2, 1, 1024, 1024, 2);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_231_CONTINUE);

    // block 3
    setup_test_message(&TestMessage, 126, COAP_TYPE_CON, 3, 0, 1024, 256, 3);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(bsize, 3328);

    if (blk1 != NULL)
    {
        free(blk1);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Tests the case where the server is not able to accept the block size
 */
//--------------------------------------------------------------------------------------------------
static void test_block1_stream_large(void)
{
    lwm2m_block1_data_t * blk1 = NULL;
    size_t bsize;
    uint8_t st;
    uint8_t *resultBuffer = NULL;

    TotalNumBytes = 0;

    lwm2mcore_SetCoapExternalHandler(CoapMessageHandler);

    // large block side should result in COAP_413_ENTITY_TOO_LARGE
    setup_test_message(&TestMessage, 123, COAP_TYPE_CON, 0, 1, 1025, 1024, 0);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_413_ENTITY_TOO_LARGE);
    CU_ASSERT_EQUAL(bsize, 0);

    // restart block transfer and check for COAP_413_ENTITY_TOO_LARGE
    memset(TestPayload, 0, sizeof(TestPayload));
    setup_test_message(&TestMessage, 123, COAP_TYPE_CON, 0, 1, 1024, 1024, 0);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_231_CONTINUE);

    memset(TestPayload, 1, sizeof(TestPayload));
    setup_test_message(&TestMessage, 124, COAP_TYPE_CON, 1, 1, 1024, 1024, 1);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_231_CONTINUE);

    memset(TestPayload, 2, sizeof(TestPayload));
    setup_test_message(&TestMessage, 125, COAP_TYPE_CON, 2, 1, 1024, 1024, 2);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_231_CONTINUE);

    // large block side should result in COAP_413_ENTITY_TOO_LARGE
    setup_test_message(&TestMessage, 126, COAP_TYPE_CON, 0, 1, 1025, 1024, 3);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_413_ENTITY_TOO_LARGE);
    CU_ASSERT_EQUAL(bsize, 0);

    if (blk1 != NULL)
    {
        free(blk1);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Tests the case where the server hasn't received all the blocks necessary to proceed
 */
//--------------------------------------------------------------------------------------------------
static void test_block1_stream_incomplete(void)
{
    lwm2m_block1_data_t * blk1 = NULL;
    size_t bsize;
    uint8_t st;
    uint8_t *resultBuffer = NULL;

    TotalNumBytes = 0;

    // send block num 0, 1 and 3 (no more)
    memset(TestPayload, 0, sizeof(TestPayload));
    setup_test_message(&TestMessage, 123, COAP_TYPE_CON, 0, 1, 1024, 1024, 0);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_231_CONTINUE);

    memset(TestPayload, 1, sizeof(TestPayload));
    setup_test_message(&TestMessage, 124, COAP_TYPE_CON, 1, 1, 1024, 1024, 1);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_231_CONTINUE);

    // block 3
    setup_test_message(&TestMessage, 125, COAP_TYPE_CON, 3, 0, 1024, 256, 0xBA);
    st = coap_block1_stream_handler(&blk1, &TestMessage, &resultBuffer, &bsize);
    CU_ASSERT_EQUAL(st, COAP_408_REQ_ENTITY_INCOMPLETE);
    CU_ASSERT_EQUAL(bsize, 0);

    if (blk1 != NULL)
    {
        free(blk1);
    }
}

static struct TestTable table[] = {
        { "test of test_block1_stream_nominal()", test_block1_stream_nominal },
        { "test of test_block1_stream_retransmit()", test_block1_stream_retransmit },
        { "test of test_block1_stream_large()", test_block1_stream_large },
        { "test of test_block1_stream_incomplete()", test_block1_stream_incomplete },
        { NULL, NULL },//
};


CU_ErrorCode create_block1_stream_suit() {
    CU_pSuite pSuite = NULL;
    printf("1\r\n");
    pSuite = CU_add_suite("Suite_block1_stream", NULL, NULL);

    if (NULL == pSuite) {
        return CU_get_error();
    }

    return add_tests(pSuite, table);
}
