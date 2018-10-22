//-------------------------------------------------------------------------------------------------
/**
 * @file block2streamtests.c
 *
 * Unitary test for coap block2 stream.
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

static lwm2mcore_StreamStatus_t StreamStatus;

static uint32_t TotalBlocksRequested = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Setup CoAP test message
 */
//--------------------------------------------------------------------------------------------------
static void setup_test_message(coap_packet_t* testMsg,
                               uint16_t mid,
                               coap_message_type_t type,
                               uint8_t code,
                               uint32_t block2_num,
                               uint8_t block2_more,
                               uint16_t block2_size)
{
    memset(testMsg, 0, sizeof(coap_packet_t));

    testMsg->mid = mid;
    testMsg->type = type;
    testMsg->code = code;
    coap_set_header_block2(testMsg, block2_num, block2_more, block2_size);
}

//--------------------------------------------------------------------------------------------------
/**
 * Handles CoAP messages from server such as read, write, execute and streams (block transfers)
 */
//--------------------------------------------------------------------------------------------------
static uint8_t CoapMessageHandler
(
    lwm2mcore_CoapRequest_t* requestRef
)
{
    LOG("Request: CoAP message received from server");

    StreamStatus = lwm2mcore_GetStreamStatus(requestRef);

    LOG_ARG("StreamStatus = %d", StreamStatus);

    if (StreamStatus == LWM2MCORE_TX_STREAM_IN_PROGRESS)
    {
        TotalBlocksRequested++;
    }

    return COAP_205_CONTENT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Tests the nominal case where all messages arrive in sequence.
 */
//--------------------------------------------------------------------------------------------------
static void test_block2_stream_nominal(void)
{
    uint8_t st;
    TotalBlocksRequested = 0;
    lwm2mcore_SetCoapExternalHandler(CoapMessageHandler);

    // Assume the device initiated the block transfer and ask for block number 1
    setup_test_message(&TestMessage, 124, COAP_TYPE_CON, COAP_GET, 1, 1, 1024);
    st = coap_block2_stream_handler(&TestMessage, 1);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(StreamStatus, LWM2MCORE_TX_STREAM_IN_PROGRESS);

    setup_test_message(&TestMessage, 125, COAP_TYPE_CON, COAP_GET, 2, 1, 1024);
    st = coap_block2_stream_handler(&TestMessage, 2);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(StreamStatus, LWM2MCORE_TX_STREAM_IN_PROGRESS);

    setup_test_message(&TestMessage, 126, COAP_TYPE_CON, COAP_GET, 3, 1, 1024);
    st = coap_block2_stream_handler(&TestMessage, 3);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(StreamStatus, LWM2MCORE_TX_STREAM_IN_PROGRESS);

    CU_ASSERT_EQUAL(TotalBlocksRequested, 3);
}

//--------------------------------------------------------------------------------------------------
/**
 * Tests the case where few blocks arrive out-of-order
 */
//--------------------------------------------------------------------------------------------------
static void test_block2_stream_retransmit(void)
{

    uint8_t st;

    lwm2mcore_SetCoapExternalHandler(CoapMessageHandler);
    TotalBlocksRequested = 0;

    // Assume the device initiated the block transfer and ask for block number 1
    setup_test_message(&TestMessage, 224, COAP_TYPE_CON, COAP_GET, 1, 1, 1024);
    st = coap_block2_stream_handler(&TestMessage, 1);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(StreamStatus, LWM2MCORE_TX_STREAM_IN_PROGRESS);

    // retransmit block num 1
    setup_test_message(&TestMessage, 224, COAP_TYPE_CON, COAP_GET, 1, 1, 1024);
    st = coap_block2_stream_handler(&TestMessage, 1);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(StreamStatus, LWM2MCORE_TX_STREAM_IN_PROGRESS);

    // retransmit block num 1
    setup_test_message(&TestMessage, 224, COAP_TYPE_CON, COAP_GET, 1, 1, 1024);
    st = coap_block2_stream_handler(&TestMessage, 1);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(StreamStatus, LWM2MCORE_TX_STREAM_IN_PROGRESS);

    // transmit block num 2
    setup_test_message(&TestMessage, 225, COAP_TYPE_CON, COAP_GET, 2, 1, 1024);
    st = coap_block2_stream_handler(&TestMessage, 2);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(StreamStatus, LWM2MCORE_TX_STREAM_IN_PROGRESS);

    // retransmit block num 2
    setup_test_message(&TestMessage, 225, COAP_TYPE_CON, COAP_GET, 2, 1, 1024);
    st = coap_block2_stream_handler(&TestMessage, 2);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(StreamStatus, LWM2MCORE_TX_STREAM_IN_PROGRESS);

    // transmit block num 3
    setup_test_message(&TestMessage, 226, COAP_TYPE_CON, COAP_GET, 3, 1, 1024);
    st = coap_block2_stream_handler(&TestMessage, 3);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(StreamStatus, LWM2MCORE_TX_STREAM_IN_PROGRESS);

    // retransmit block num 3
    setup_test_message(&TestMessage, 226, COAP_TYPE_CON, COAP_GET, 3, 1, 1024);
    st = coap_block2_stream_handler(&TestMessage, 3);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(StreamStatus, LWM2MCORE_TX_STREAM_IN_PROGRESS);

    LOG_ARG("TotalBlocksRequested = %d", TotalBlocksRequested);
    CU_ASSERT_EQUAL(TotalBlocksRequested, 3);
}

//--------------------------------------------------------------------------------------------------
/**
 * Tests the case where the server is not able to accept the block size
 */
//--------------------------------------------------------------------------------------------------
static void test_block2_stream_large(void)
{
    uint8_t st;

    // try requesting larger block size
    setup_test_message(&TestMessage, 324, COAP_TYPE_CON, COAP_GET, 1, 1, 1025);
    st = coap_block2_stream_handler(&TestMessage, 1);
    CU_ASSERT_EQUAL(st, COAP_500_INTERNAL_SERVER_ERROR);
    CU_ASSERT_EQUAL(StreamStatus, LWM2MCORE_TX_STREAM_ERROR);

    // report COAP_413_ENTITY_TOO_LARGE
    setup_test_message(&TestMessage, 324, COAP_TYPE_CON, COAP_413_ENTITY_TOO_LARGE, 1, 1, 1024);
    st = coap_block2_stream_handler(&TestMessage, 1);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(StreamStatus, LWM2MCORE_TX_STREAM_ERROR);
}

//--------------------------------------------------------------------------------------------------
/**
 * Tests the case where the server hasn't received all the blocks necessary to proceed
 */
//--------------------------------------------------------------------------------------------------
static void test_block2_stream_incomplete(void)
{
    uint8_t st;

    // report COAP_413_ENTITY_TOO_LARGE
    setup_test_message(&TestMessage, 424, COAP_TYPE_CON, COAP_408_REQ_ENTITY_INCOMPLETE, 1, 1, 1024);
    st = coap_block2_stream_handler(&TestMessage, 1);
    CU_ASSERT_EQUAL(st, COAP_IGNORE);
    CU_ASSERT_EQUAL(StreamStatus, LWM2MCORE_TX_STREAM_ERROR);
}

static struct TestTable table[] = {
        { "test of test_block2_stream_nominal()", test_block2_stream_nominal },
        { "test of test_block2_stream_retransmit()", test_block2_stream_retransmit },
        { "test of test_block2_stream_large()", test_block2_stream_large },
        { "test of test_block2_stream_incomplete()", test_block2_stream_incomplete },
        { NULL, NULL },//
};


CU_ErrorCode create_block2_stream_suit() {
    CU_pSuite pSuite = NULL;
    printf("1\r\n");
    pSuite = CU_add_suite("Suite_block2_stream", NULL, NULL);

    if (NULL == pSuite) {
        return CU_get_error();
    }

    return add_tests(pSuite, table);
}
