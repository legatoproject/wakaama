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
#include <internalCoapHandler.h>

#define MAX_BLOCK2_SIZE 1024

typedef struct
{
    coap_packet_t response;
    uint8_t payload[MAX_BLOCK2_SIZE];
    uint32_t payload_len;
    uint32_t expBlock2Num;
    uint16_t lastBlock2Mid;
}block2_context_t;

static block2_context_t blk2_ctxt;

void coap_end_block2_stream()
{
    memset(&blk2_ctxt, 0, sizeof(block2_context_t));
}

coap_status_t coap_block2_stream_handler(coap_packet_t* message,
                                         coap_packet_t* response)
{
    uint16_t blockSize;
    uint32_t blockNum;
    bool blockMore;

    coap_status_t rc = COAP_IGNORE;

    // parse block2 header
    coap_get_header_block2(message, (uint32_t*)&blockNum, (uint8_t*)&blockMore, &blockSize, NULL);

    LOG_ARG("Block transfer %u/%u/%u @ %u bytes",
                                                    message->block2_num,
                                                    message->block2_more,
                                                    message->block2_size,
                                                    message->block2_offset);

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
            // If the current mid is equal to the last block2 mid, retransmit the saved block.
            // We are not supposed to receive any message id's that are older than current mid.
            if ((message->mid <= blk2_ctxt.lastBlock2Mid) && (message->mid != 0))
            {
                LOG_ARG("Retransmission for block number %d", blockNum);

                memcpy(response, &blk2_ctxt.response, sizeof(coap_packet_t));
                coap_set_payload(response, blk2_ctxt.payload, MIN(blk2_ctxt.payload_len, MAX_BLOCK2_SIZE));

                LOG_ARG("Response code = %d", response->code);
                LOG_ARG("Payload length = %d", response->payload_len);
                LOG_ARG("Block transfer %u/%u/%u @ %u bytes",
                                            response->block2_num,
                                            response->block2_more,
                                            response->block2_size,
                                            response->block2_offset);

                return response->code;
            }
            else if (blockNum != blk2_ctxt.expBlock2Num)
            {
                LOG_ARG("Unexpected block number %d, expected block num %d", blockNum, blk2_ctxt.expBlock2Num);
                lwm2mcore_CallCoapExternalHandler(message, LWM2MCORE_TX_STREAM_ERROR);
                coap_end_block2_stream();
                return COAP_500_INTERNAL_SERVER_ERROR;
            }

            blk2_ctxt.lastBlock2Mid = message->mid;
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

void coap_block2_handle_response(coap_packet_t* response,
                                 lwm2mcore_StreamStatus_t streamStatus)
{
    /* initiate block transfer for a bigger block */
    if (streamStatus == LWM2MCORE_TX_STREAM_START)
    {
        blk2_ctxt.expBlock2Num = 0;
        LOG_ARG("Initiate Blockwise transfer with block_size %u", REST_MAX_CHUNK_SIZE);
        coap_set_header_block2(response, blk2_ctxt.expBlock2Num++, 1, REST_MAX_CHUNK_SIZE);
    }
    else if (streamStatus == LWM2MCORE_TX_STREAM_IN_PROGRESS)
    {
        coap_set_header_block2(response, blk2_ctxt.expBlock2Num++, 1, REST_MAX_CHUNK_SIZE);
    }
    else if (streamStatus == LWM2MCORE_TX_STREAM_END)
    {
        coap_set_header_block2(response, blk2_ctxt.expBlock2Num++, 0, REST_MAX_CHUNK_SIZE);
    }

    /* save last block */
    memcpy(&blk2_ctxt.response, response, sizeof(coap_packet_t));
    memcpy(blk2_ctxt.payload, response->payload, response->payload_len);
    blk2_ctxt.payload_len = response->payload_len;
}

#endif
