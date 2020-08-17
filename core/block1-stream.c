/**
 * @file block1-stream.c
 *
 * Block-1 stream handling
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

#define MAX_BLOCK1_SIZE 1024

void coap_end_block1_stream(lwm2m_block1_data_t ** pBlock1Data,
                            uint8_t ** outputBuffer,
                            size_t * outputLength)
{
    lwm2m_block1_data_t * block1Data;

    // Reset output buffer
    if ((outputBuffer != NULL) && (outputLength != NULL))
    {
        *outputLength = 0;
        *outputBuffer = NULL;
    }

    block1Data = *pBlock1Data;;
    if (block1Data == NULL)
    {
        return;
    }

    if (block1Data->block1buffer != NULL)
    {
        lwm2m_free(block1Data->block1buffer);
    }

    if (block1Data != NULL)
    {
        // free current element
        lwm2m_free(block1Data);
        *pBlock1Data = NULL;
    }
}

uint8_t coap_block1_stream_handler(lwm2m_block1_data_t ** pBlock1Data,
                                   coap_packet_t* message,
                                   uint8_t ** outputBuffer,
                                   size_t * outputLength)
{
    lwm2m_block1_data_t * block1Data;
    uint16_t blockSize;
    uint32_t blockNum;
    bool blockMore;

    // parse block1 header
    coap_get_header_block1(message, (uint32_t*)&blockNum, (uint8_t*)&blockMore, &blockSize, NULL);

    if (message->type == COAP_TYPE_ACK)
    {
        // This is an ack message for block-1 initiated from device (push ack)
        if (blockMore)
        {
            lwm2mcore_CallCoapExternalHandler(message, LWM2MCORE_TX_STREAM_IN_PROGRESS);
        }
        else
        {
            lwm2mcore_CallCoapExternalHandler(message, LWM2MCORE_TX_STREAM_END);
        }

        // Actual response will be sent by app
        return COAP_MANUAL_RESPONSE;
    }

    if (pBlock1Data != NULL)
    {
        block1Data = *pBlock1Data;
    }
    else
    {
        LOG("Block-1 buffer not allocated");
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    if (blockSize > MAX_BLOCK1_SIZE)
    {
        LOG("payload length larger than max.");
        // Indicate stream failure to app.
        // ToDo: Does app need to receive this event?
        lwm2mcore_CallCoapExternalHandler(message, LWM2MCORE_RX_STREAM_ERROR);

        coap_end_block1_stream(pBlock1Data, outputBuffer, outputLength);

        return COAP_413_ENTITY_TOO_LARGE;
    }

    // manage new block1 transfer
    if (blockNum == 0)
    {
        // we already have block1 data for this server, clear it
        if (block1Data != NULL)
        {
            // If this is a retransmission, we already did that.
            if ((message->mid > block1Data->lastmid) && (message->mid != 0))
            {
                lwm2m_free(block1Data->block1buffer);
            }
            else
            {
                LOG("Retransmitted packet discarded");
                if(message->block1_more)
                {
                    return COAP_231_CONTINUE;
                }
                else
                {
                    return COAP_204_CHANGED;
                }
            }
        }
        else
        {
            coap_end_block1_stream(pBlock1Data, outputBuffer, outputLength);

            block1Data = lwm2m_malloc(sizeof(lwm2m_block1_data_t));
            *pBlock1Data = block1Data;
            if (NULL == block1Data)
            {
               // Send the status event.
               lwm2mcore_CallCoapExternalHandler(message, LWM2MCORE_RX_STREAM_ERROR);

               return COAP_500_INTERNAL_SERVER_ERROR;
            }
        }

        block1Data->block1buffer = lwm2m_malloc(MAX_BLOCK1_SIZE);
        block1Data->block1bufferSize = message->payload_len;
        block1Data->block1Num = blockNum;

        // write new block in buffer
        memcpy(block1Data->block1buffer, message->payload, message->payload_len);
        block1Data->lastmid = message->mid;
    }
    // manage already started block1 transfer
    else
    {
        if (block1Data == NULL)
        {
           // Send the status event.
           lwm2mcore_CallCoapExternalHandler(message, LWM2MCORE_RX_STREAM_ERROR);
           coap_end_block1_stream(pBlock1Data, outputBuffer, outputLength);

           return COAP_408_REQ_ENTITY_INCOMPLETE;
        }

        // If this is a retransmission, we already did that.
        if ((message->mid > block1Data->lastmid) && (message->mid != 0))
        {
           if (block1Data->block1bufferSize != blockSize * blockNum)
           {
               // Send the status event.
               lwm2mcore_CallCoapExternalHandler(message, LWM2MCORE_RX_STREAM_ERROR);
               coap_end_block1_stream(pBlock1Data, outputBuffer, outputLength);

               return COAP_408_REQ_ENTITY_INCOMPLETE;
           }

           block1Data->block1bufferSize += message->payload_len;
           memcpy(block1Data->block1buffer, message->payload, message->payload_len);
           block1Data->lastmid = message->mid;
           block1Data->block1Num = blockNum;
        }
        else
        {
            LOG("Retransmitted packet discarded");
            if(message->block1_more)
            {
                return COAP_231_CONTINUE;
            }
            else
            {
                return COAP_204_CHANGED;
            }
        }
    }

    if (blockMore)
    {
        *outputLength = (size_t)-1;

        // Send the status event.
        lwm2mcore_CallCoapExternalHandler(message, LWM2MCORE_RX_STREAM_IN_PROGRESS);

        return COAP_MANUAL_RESPONSE;
    }
    else
    {
        *outputLength = block1Data->block1bufferSize;
        *outputBuffer = block1Data->block1buffer;
        LOG_ARG("Total buffer size received = %d", block1Data->block1bufferSize);

        // Send the status event.
        lwm2mcore_CallCoapExternalHandler(message, LWM2MCORE_RX_STREAM_END);

        // Actual response will be sent by app
        return COAP_MANUAL_RESPONSE;
    }
}
#endif
