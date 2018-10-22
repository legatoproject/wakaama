/**
 * @file block2-stream.c
 *
 * Block-2 stream handling
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */
#include "internals.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if SIERRA
#include <lwm2mcore/lwm2mcore.h>

#define MAX_BLOCK2_SIZE 1024

uint16_t LastBlock2Mid = 0;

void coap_end_block2_stream()
{
    LastBlock2Mid = 0;
}

coap_status_t coap_block2_stream_handler(coap_packet_t* message,
                                         uint32_t expBlockNum)
{
    uint16_t blockSize;
    uint32_t blockNum;
    bool blockMore;

    coap_status_t rc = COAP_IGNORE;

    // parse block2 header
    coap_get_header_block2(message, &blockNum, &blockMore, &blockSize, NULL);

    switch (message->code)
    {
        case COAP_408_REQ_ENTITY_INCOMPLETE:
        case COAP_413_ENTITY_TOO_LARGE:
            coap_end_block2_stream();
            lwm2mcore_CallCoapExternalHandler(message, LWM2MCORE_TX_STREAM_ERROR);
            rc = COAP_IGNORE;
            break;

        case COAP_GET:

            // Check if block_size 1KB. Assuming that server is capable of receiving a block size of 1KB.
            if (blockSize != MAX_BLOCK2_SIZE)
            {
                LOG_ARG("Unexpected block size %d, expected block size %d", blockSize, MAX_BLOCK2_SIZE);
                lwm2mcore_CallCoapExternalHandler(message, LWM2MCORE_TX_STREAM_ERROR);
                coap_end_block2_stream();
                return COAP_500_INTERNAL_SERVER_ERROR;
            }

            // Check if this is a retransmission request
            if ((message->mid <= LastBlock2Mid) && (message->mid != 0))
            {
                // Just ignore this message for now.
                LOG_ARG("Retransmission for block number %d", blockNum);
                return COAP_IGNORE;;
            }

            if (blockNum != expBlockNum)
            {
                LOG_ARG("Unexpected block number %d, expected block num %d", blockNum, expBlockNum);
                lwm2mcore_CallCoapExternalHandler(message, LWM2MCORE_TX_STREAM_ERROR);
                coap_end_block2_stream();
                return COAP_500_INTERNAL_SERVER_ERROR;
            }

            LastBlock2Mid = message->mid;
            rc = lwm2mcore_CallCoapExternalHandler(message, LWM2MCORE_TX_STREAM_IN_PROGRESS);
            break;

        default:
            LOG_ARG("Unexpected coap message code %d", message->code);
            coap_end_block2_stream();
            rc = COAP_500_INTERNAL_SERVER_ERROR;
            break;
    }

    return rc;
}
#endif
