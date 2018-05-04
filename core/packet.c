/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    David Navarro, Intel Corporation - initial API and implementation
 *    domedambrosio - Please refer to git log
 *    Fabien Fleutot - Please refer to git log
 *    Fabien Fleutot - Please refer to git log
 *    Simon Bernard - Please refer to git log
 *    Toby Jaffey - Please refer to git log
 *    Pascal Rieux - Please refer to git log
 *    Bosch Software Innovations GmbH - Please refer to git log
 *
 *******************************************************************************/

/*
 Copyright (c) 2013, 2014 Intel Corporation

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

     * Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
     * Neither the name of Intel Corporation nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.

 David Navarro <david.navarro@intel.com>

*/

/*
Contains code snippets which are:

 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.

*/


#include "internals.h"

#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#if SIERRA
#include <lwm2mcore/lwm2mcore.h>
#include <internalCoapHandler.h>

#define PRV_QUERY_BUFFER_LENGTH 200

typedef struct
{
    lwm2m_context_t * contextP;
    lwm2m_server_t * serverP;
    uint16_t firstBlockMid;
    uint8_t * bufferP;
    size_t buffer_len;
    unsigned int content_type;
    lwm2m_push_ack_callback_t callbackP;
} push_state_t;

typedef struct
{
    uint8_t * bufferP;
    size_t length;
    unsigned int content_type;
} async_state_t;

static push_state_t current_push_state;
static async_state_t current_async_state;
#endif

static void handle_reset(lwm2m_context_t * contextP,
                         void * fromSessionH,
                         coap_packet_t * message)
{
#ifdef LWM2M_CLIENT_MODE
    LOG("Entering");
    observe_cancel(contextP, message->mid, fromSessionH);
#endif
}

#if SIERRA
static bool is_block_transfer(coap_packet_t * message, uint32_t * block_num, uint16_t * block_size, uint32_t * block_offset )
{
    async_state_t * async_stateP = &current_async_state;
    if (async_stateP->bufferP != NULL)
    {
        return coap_get_header_block2(message, block_num, NULL, block_size, block_offset);
    }
    else
    {
        LOG("Async state buffer is NULL");
        return false;
    }
}
#endif

static bool record_received_mid(lwm2m_server_t * serverP,
                                uint16_t mid)
{
    uint16_t expectedMid = serverP->midReceived + 1;
    bool validMid;

    if (!serverP->midReceived || (mid == expectedMid))
    {
        validMid = true;
        serverP->midReceived = true;
        serverP->lastRxMID = mid;
    }
    else
    {
        validMid = false;
        LOG_ARG("WARNING: Received unexpected request midReceived(%u) MID(%u != %u)",
            serverP->midReceived, mid, expectedMid);
    }

    return validMid;
}

static uint8_t handle_request(lwm2m_context_t * contextP,
                              void * fromSessionH,
                              coap_packet_t * message,
                              coap_packet_t * response,
                              bool validMid)
{
    lwm2m_uri_t * uriP = NULL;
    uint8_t result = COAP_IGNORE;

#if SIERRA
    uint32_t block_num = 0;
    uint16_t block_size = REST_MAX_CHUNK_SIZE;
    uint32_t block_offset = 0;
#endif

    LOG("Entering");

#if SIERRA
    if (NULL == message->uri_path)
    {
        LOG ("No URI available for this request.");
    }
    else
    {
        if (NULL == message->uri_path->data)
        {
            LOG ("No payload in the URI.");
        }
        else
        {
            lwm2mcore_DataDump("COAP Message URI",
                                message->uri_path->data, message->uri_path->len);

            // Check if the prefix matches the legato app objects
            if (IsCoapUri(message->uri_path))
            {
                if (is_block_transfer(message, &block_num, &block_size,
                                      &block_offset) &&  (block_num != 0))
                {
                    async_state_t * async_stateP = &current_async_state;
                    coap_set_header_content_type(response, async_stateP->content_type);
                    coap_set_payload(response, async_stateP->bufferP, async_stateP->length);

                    return NO_ERROR;
                }
                else
                {
                    /* Send an ack (empty response) */
                    LOG("Send an empty response");
                    coap_init_message(response, COAP_TYPE_ACK, 0, message->mid);
                    message_send(contextP, response, fromSessionH);

                    LOG_ARG("Valid(%u) mid(%u)", validMid, message->mid);
                    if (validMid)
                    {
                        // Get actual response from user app
                        return lwm2mcore_CallCoapEventHandler(message);
                    }
                }
            }
        }
    }
#endif

#ifdef LWM2M_CLIENT_MODE
    uriP = uri_decode(contextP->altPath, message->uri_path);
#else
    uriP = uri_decode(NULL, message->uri_path);
#endif

    if (NULL == uriP)
    {
        return COAP_400_BAD_REQUEST;
    }

    switch(uriP->flag & LWM2M_URI_MASK_TYPE)
    {
#ifdef LWM2M_CLIENT_MODE
    case LWM2M_URI_FLAG_DM:
    {
        lwm2m_server_t * serverP;

        serverP = utils_findServer(contextP, fromSessionH);
        if (NULL != serverP)
        {
            result = dm_handleRequest(contextP, uriP, serverP, message, response);
        }
#ifdef LWM2M_BOOTSTRAP
        else
        {
            serverP = utils_findBootstrapServer(contextP, fromSessionH);
            if (NULL != serverP)
            {
                result = bootstrap_handleCommand(contextP, uriP, serverP, message, response);
            }
        }
#endif
    }
    break;

#ifdef LWM2M_BOOTSTRAP
    case LWM2M_URI_FLAG_DELETE_ALL:
        if (COAP_DELETE != message->code)
        {
            result = COAP_400_BAD_REQUEST;
        }
        else
        {
            result = bootstrap_handleDeleteAll(contextP, fromSessionH);
        }
        break;

    case LWM2M_URI_FLAG_BOOTSTRAP:
        if (message->code == COAP_POST)
        {
            result = bootstrap_handleFinish(contextP, fromSessionH);
        }
        break;
#endif
#endif

#ifdef LWM2M_SERVER_MODE
    case LWM2M_URI_FLAG_REGISTRATION:
        result = registration_handleRequest(contextP, uriP, fromSessionH, message, response);
        break;
#endif
#ifdef LWM2M_BOOTSTRAP_SERVER_MODE
    case LWM2M_URI_FLAG_BOOTSTRAP:
        result = bootstrap_handleRequest(contextP, uriP, fromSessionH, message, response);
        break;
#endif
    default:
        result = COAP_IGNORE;
        break;
    }

    coap_set_status_code(response, result);

    if (COAP_IGNORE < result && result < COAP_400_BAD_REQUEST)
    {
        result = NO_ERROR;
    }

    lwm2m_free(uriP);
    return result;
}

#if SIERRA
static lwm2m_transaction_t * prv_init_push_transaction
(
    lwm2m_context_t * contextP,
    lwm2m_server_t * server,
    lwm2m_media_type_t contentType

)
{
    lwm2m_transaction_t * transaction;
    char query[PRV_QUERY_BUFFER_LENGTH];
    int query_length = 0;
    int res;

    query_length = utils_stringCopy(query, PRV_QUERY_BUFFER_LENGTH, "?ep=");
    if (query_length < 0)
    {
        return -1;
    }
    res = utils_stringCopy(query + query_length, PRV_QUERY_BUFFER_LENGTH - query_length, contextP->endpointName);
    if (res < 0)
    {
        return -1;
    }

    transaction = transaction_new(server->sessionH, COAP_POST, NULL, NULL, contextP->nextMID++, 4, NULL);
    if (transaction == NULL) return NULL;

    LOG_ARG("Server location = %s", server->location);
    coap_set_header_uri_query(transaction->message, query);
    coap_set_header_uri_path(transaction->message, "/"URI_DATAPUSH_SEGMENT);
    coap_set_header_content_type(transaction->message, contentType);

    return transaction;
}


static void prv_end_push(push_state_t * push_stateP)
{
    if (push_stateP->bufferP != NULL)
    {
        lwm2m_free(push_stateP->bufferP);
        push_stateP->bufferP = NULL;
        push_stateP->buffer_len = 0;
    }
}

static void prv_end_async()
{
    async_state_t * async_stateP = &current_async_state;

    if (async_stateP->bufferP != NULL)
    {
        lwm2m_free(async_stateP->bufferP);
        async_stateP->bufferP = NULL;
        async_stateP->length = 0;
    }
}

static void prv_push_callback(lwm2m_transaction_t * transacP, void * message)
{
    push_state_t * push_stateP = &current_push_state;
    coap_packet_t * ack_message = transacP->message;
    coap_packet_t * packet = (coap_packet_t *)message;

    if (push_stateP->callbackP == NULL)
    {
        LOG("Push ack callback is NULL.");
        return;
    }

    uint16_t ackMid = ack_message->mid;

    if (IS_OPTION(ack_message, COAP_OPTION_BLOCK1))
    {
        // parse block1 header
        uint32_t block1_num;
        uint8_t  block1_more;
        uint16_t block1_size;
        coap_get_header_block1(ack_message, &block1_num, &block1_more, &block1_size, NULL);
        LOG_ARG("Callback for block num %u (SZX %u/ SZX Max%u) MORE %u",
                                                        block1_num,
                                                        block1_size,
                                                        REST_MAX_CHUNK_SIZE,
                                                        block1_more);

        // For block transfer, report the message ID that was generated
        // when we initiated the transfer.
        ackMid = push_stateP->firstBlockMid;

        // wait till the last block is acked.
        if (block1_more)
        {
            if ((transacP->ack_received) && (COAP_408_REQ_ENTITY_INCOMPLETE != packet->code))
            {
                LOG("Wait for ack of last block.");
                return;
            }
            else
            {
                LOG("Block transfer time out.");
                prv_end_push(push_stateP);
            }
        }
        else
        {
            LOG("Callback for last block.");
            prv_end_push(push_stateP);
        }
    }

    if (transacP->ack_received && (COAP_408_REQ_ENTITY_INCOMPLETE != packet->code))
    {
        LOG_ARG("mid = %d, retransmit_count = %d ", ackMid, transacP->retrans_counter);
        push_stateP->callbackP(LWM2M_ACK_RECEIVED, ackMid);
    }
    else
    {
        LOG_ARG("mid = %d, retransmit_count = %d ", ackMid, transacP->retrans_counter);
        push_stateP->callbackP(LWM2M_ACK_TIMEOUT, ackMid);
    }
}
#endif

/* This function is an adaptation of function coap_receive() from Erbium's er-coap-13-engine.c.
 * Erbium is Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 */
void lwm2m_handle_packet(lwm2m_context_t * contextP,
                         uint8_t * buffer,
                         int length,
                         void * fromSessionH)
{
    uint8_t coap_error_code = NO_ERROR;
    static coap_packet_t message[1];
    static coap_packet_t response[1];
    uint16_t payload_length;
    uint8_t* payloadP;
#if SIERRA
    push_state_t * push_stateP = &current_push_state;
#endif

    LOG("Entering");
    coap_error_code = coap_parse_message(message, buffer, (uint16_t)length);
    if (coap_error_code == NO_ERROR)
    {
        LOG_ARG("Parsed: blk: %u ver %u, type %u, tkl %u, code %u.%.2u, mid %u, Content type: %d",
                message->block1_num, message->version, message->type, message->token_len, message->code >> 5,
                message->code & 0x1F, message->mid, message->content_type);
        LOG_ARG("Payload: %.*s", message->payload_len, message->payload);
        if (message->code >= COAP_GET && message->code <= COAP_DELETE)
        {
            uint32_t block_num = 0;
            uint16_t block_size = REST_MAX_CHUNK_SIZE;
            uint32_t block_offset = 0;
            int64_t new_offset = 0;
            bool validMid = true;

            /* prepare response */
            if (message->type == COAP_TYPE_CON)
            {
                /* Reliable CON requests are answered with an ACK. */
                coap_init_message(response, COAP_TYPE_ACK, COAP_205_CONTENT, message->mid);
            }
            else
            {
                /* Unreliable NON requests are answered with a NON as well. */
                coap_init_message(response, COAP_TYPE_NON, COAP_205_CONTENT, contextP->nextMID++);
            }

            /* mirror token */
            if (message->token_len)
            {
                coap_set_header_token(response, message->token, message->token_len);
            }

            /* get offset for blockwise transfers */
            if (coap_get_header_block2(message, &block_num, NULL, &block_size, &block_offset))
            {
                LOG_ARG("Blockwise: block request %u (%u/%u) @ %u bytes", block_num, block_size, REST_MAX_CHUNK_SIZE, block_offset);
                block_size = MIN(block_size, REST_MAX_CHUNK_SIZE);
                new_offset = block_offset;
            }

            /* handle block1 option */
            if (IS_OPTION(message, COAP_OPTION_BLOCK1))
            {
#ifdef LWM2M_CLIENT_MODE
                // get server
                lwm2m_server_t * serverP;
                serverP = utils_findServer(contextP, fromSessionH);
#ifdef LWM2M_BOOTSTRAP
                if (serverP == NULL)
                {
                    serverP = utils_findBootstrapServer(contextP, fromSessionH);
                }
#endif
                if (serverP == NULL)
                {
                    coap_error_code = COAP_500_INTERNAL_SERVER_ERROR;
                }
                else
                {
                    uint32_t block1_num;
                    uint8_t  block1_more;
                    uint16_t block1_size;
                    uint8_t * complete_buffer = NULL;
                    size_t complete_buffer_size;

                    // parse block1 header
                    coap_get_header_block1(message, &block1_num, &block1_more, &block1_size, NULL);
                    LOG_ARG("Blockwise: block1 request NUM %u (SZX %u/ SZX Max%u) MORE %u", block1_num, block1_size, REST_MAX_CHUNK_SIZE, block1_more);

                    // handle block 1
                    coap_error_code = coap_block1_handler(&serverP->block1Data, message->mid, message->payload, message->payload_len, block1_size, block1_num, block1_more, &complete_buffer, &complete_buffer_size);

                    // if payload is complete, replace it in the coap message.
                    if (coap_error_code == NO_ERROR)
                    {
                        message->payload = complete_buffer;
                        message->payload_len = complete_buffer_size;
                    }
                    else if (coap_error_code == COAP_231_CONTINUE)
                    {
                        block1_size = MIN(block1_size, REST_MAX_CHUNK_SIZE);
                        coap_set_header_block1(response,block1_num, block1_more,block1_size);
                    }
                }
#else
                coap_error_code = COAP_501_NOT_IMPLEMENTED;
#endif
            }
            else
            {
#ifdef LWM2M_CLIENT_MODE
                // get server
                lwm2m_server_t *serverP = utils_findServer(contextP, fromSessionH);
#ifdef LWM2M_BOOTSTRAP
                if (serverP == NULL)
                {
                    serverP = utils_findBootstrapServer(contextP, fromSessionH);
                }
#endif
#endif
                if (serverP != NULL)
                {
                    validMid = record_received_mid(serverP, message->mid);
                }
            }
            if (coap_error_code == NO_ERROR)
            {
                coap_error_code = handle_request(contextP, fromSessionH, message, response, validMid);
            }
            if (coap_error_code==NO_ERROR)
            {
                if ( IS_OPTION(message, COAP_OPTION_BLOCK2) )
                {
                    /* unchanged new_offset indicates that resource is unaware of blockwise transfer */
                    if (new_offset==block_offset)
                    {
                        LOG_ARG("Blockwise: unaware resource with payload length %u/%u", response->payload_len, block_size);
                        if (block_offset >= response->payload_len)
                        {
                            LOG("handle_incoming_data(): block_offset >= response->payload_len");

                            response->code = COAP_402_BAD_OPTION;
                            coap_set_payload(response, "BlockOutOfScope", 15); /* a const char str[] and sizeof(str) produces larger code size */
                        }
                        else
                        {
                            coap_set_header_block2(response, block_num, response->payload_len - block_offset > block_size, block_size);
                            payload_length = MIN(response->payload_len - block_offset, block_size);
                            payloadP = (uint8_t *)lwm2m_malloc(payload_length);

                            memcpy(payloadP,response->payload+block_offset, payload_length);
                            coap_set_payload(response, payloadP, payload_length);

                            if(!response->block2_more)
                            {
                                LOG("End of block2 transfer");
                                prv_end_async();
                            }
                        } /* if (valid offset) */
                    }
                    else
                    {
                        /* resource provides chunk-wise data */
                        LOG_ARG("Blockwise: blockwise resource, new offset %d", (int) new_offset);
                        coap_set_header_block2(response, block_num, new_offset!=-1 || response->payload_len > block_size, block_size);

                        if (response->payload_len > block_size)
                        {
                            payloadP = (uint8_t *)lwm2m_malloc(block_size);
                            memcpy(payloadP,response->payload, block_size);
                            coap_set_payload(response, payloadP, block_size);

                            if(!response->block2_more)
                            {
                                LOG("End of block2 transfer");
                                prv_end_async();
                            }
                        }
                    } /* if (resource aware of blockwise) */
                }
                else if (new_offset!=0)
                {
                    LOG_ARG("Blockwise: no block option for blockwise resource, using block size %u", REST_MAX_CHUNK_SIZE);

                    coap_set_header_block2(response, 0, new_offset!=-1, REST_MAX_CHUNK_SIZE);
                    coap_set_payload(response, response->payload, MIN(response->payload_len, REST_MAX_CHUNK_SIZE));
                } /* if (blockwise request) */

#if SIERRA
                LOG_ARG("response->mid = %d", response->mid);
                LOG_ARG("response->token_len = %d", response->token_len);

                lwm2mcore_DataDump("COAP token", response->token, response->token_len);

                LOG_ARG("total payload length = %d", response->payload_len);
                LOG_ARG("Block transfer %u/%u/%u @ %u bytes",
                                    response->block2_num,
                                    response->block2_more,
                                    response->block2_size,
                                    response->block2_offset);
#endif

                coap_error_code = message_send(contextP, response, fromSessionH);
                lwm2m_free(response->payload);
                response->payload = NULL;
                response->payload_len = 0;
            }
            else if (coap_error_code != COAP_IGNORE)
            {
                if (1 == coap_set_status_code(response, coap_error_code))
                {
                    coap_error_code = message_send(contextP, response, fromSessionH);
                }
            }
        }
        else
        {
            /* Responses */
            switch (message->type)
            {
            case COAP_TYPE_NON:
            case COAP_TYPE_CON:
                {
                    bool done = transaction_handleResponse(contextP, fromSessionH, message, response);

#ifdef LWM2M_SERVER_MODE
                    if (!done && IS_OPTION(message, COAP_OPTION_OBSERVE) &&
                        ((message->code == COAP_204_CHANGED) || (message->code == COAP_205_CONTENT)))
                    {
                        done = observe_handleNotify(contextP, fromSessionH, message, response);
                    }
#endif
                    if (!done && message->type == COAP_TYPE_CON )
                    {
                        coap_init_message(response, COAP_TYPE_ACK, 0, message->mid);
                        coap_error_code = message_send(contextP, response, fromSessionH);
                    }
                }
                break;

            case COAP_TYPE_RST:
                /* Cancel possible subscriptions. */
                handle_reset(contextP, fromSessionH, message);
                transaction_handleResponse(contextP, fromSessionH, message, NULL);
                break;

            case COAP_TYPE_ACK:
#if SIERRA
                if (IS_OPTION(message, COAP_OPTION_BLOCK1) && (message->code == COAP_231_CONTINUE))
                {
                    lwm2m_transaction_t * transaction;
                    lwm2m_transaction_t * transacP;
                    uint32_t block1_num;
                    uint8_t  block1_more;
                    uint16_t block1_size;
                    uint32_t next_offset;
                    coap_packet_t *block1_resp;
                    uint32_t next_block;

                    transacP = contextP->transactionList;
                    while (NULL != transacP)
                    {
                        if (lwm2m_session_is_equal(fromSessionH, transacP->peerH, contextP->userData))
                        {
                            LOG_ARG("Next mid %u", transacP->mID);
                            if (transacP->mID != message->mid)
                            {
                                LOG_ARG("Ignore mid %u", message->mid);
                                coap_free_header(message);
                                return;
                            }

                            break;
                        }

                        transacP = transacP->next;
                    }

                    transaction = prv_init_push_transaction(push_stateP->contextP, push_stateP->serverP, push_stateP->content_type);
                    if (transaction == NULL) return;

                    block1_resp = transaction->message;

                    // parse block1 message.
                    coap_get_header_block1(message, &block1_num, &block1_more, &block1_size, NULL);
                    LOG_ARG("Blockwise: server acked NUM %u (SZX %u/ SZX Max%u) MORE %u", block1_num, block1_size, REST_MAX_CHUNK_SIZE, block1_more);

                    next_block = block1_num + 1;
                    next_offset = next_block * block1_size;

                    if (next_offset > push_stateP->buffer_len)
                    {
                        LOG_ARG("Error block offset %d out of scope - payloadLength is %d", next_offset, push_stateP->buffer_len);
                        block1_resp->code = COAP_402_BAD_OPTION;
                    }
                    else
                    {
                        block1_size = MIN(block1_size, REST_MAX_CHUNK_SIZE);

                        coap_set_header_block1(block1_resp, next_block, push_stateP->buffer_len - next_offset > block1_size, block1_size);

                        LOG_ARG("Blockwise: device responds with NUM %u (SZX %u/ SZX Max%u) MORE %u OFFSET %u",
                                block1_resp->block1_num,
                                block1_resp->block1_size,
                                REST_MAX_CHUNK_SIZE,
                                block1_resp->block1_more,
                                next_offset);

                        if (push_stateP->bufferP != NULL)
                        {
                            coap_set_payload(block1_resp, push_stateP->bufferP + next_offset, MIN(push_stateP->buffer_len - next_offset, block1_size));
                            coap_set_header_content_type(block1_resp, push_stateP->content_type);
                        }
                        else
                        {
                            coap_error_code =  COAP_500_INTERNAL_SERVER_ERROR;
                        }
                    }

                    // Notify when data push is acked or timed out
                    transaction->callback = prv_push_callback;

                    // Initiate the transaction.
                    contextP->transactionList = (lwm2m_transaction_t *)LWM2M_LIST_ADD(contextP->transactionList, transaction);
                    if (transaction_send(contextP, transaction) != 0)
                    {
                        LOG("transaction failed");
                        /* TODO: end block transfer and inform user app */
                        coap_error_code =  COAP_500_INTERNAL_SERVER_ERROR;
                    }
                }
                else if (coap_error_code == COAP_204_CHANGED)
                {
                    // ToDo: use this for push ack
                    LOG("All blocks transferred.");
                    prv_end_push(push_stateP);
                }
#endif
                transaction_handleResponse(contextP, fromSessionH, message, NULL);
                break;

            default:
                break;
            }
        } /* Request or Response */
        coap_free_header(message);
    } /* if (parsed correctly) */
    else
    {
        LOG_ARG("Message parsing failed %u.%2u", coap_error_code >> 5, coap_error_code & 0x1F);
    }

#if SIERRA
    if (coap_error_code != NO_ERROR && coap_error_code != COAP_IGNORE && coap_error_code != MANUAL_RESPONSE)
#else
    if (coap_error_code != NO_ERROR && coap_error_code != COAP_IGNORE)
#endif
    {
        REPORT_COAP(coap_error_code);
        LOG_ARG("ERROR %u: %s", coap_error_code, coap_error_message);

        /* Set to sendable error code. */
        if (coap_error_code >= 192)
        {
            coap_error_code = COAP_500_INTERNAL_SERVER_ERROR;
        }
        /* Reuse input buffer for error message. */
        coap_init_message(message, COAP_TYPE_ACK, coap_error_code, message->mid);
        coap_set_payload(message, coap_error_message, strlen(coap_error_message));
        message_send(contextP, message, fromSessionH);
    }
}


#if SIERRA
bool prv_async_response(lwm2m_context_t * contextP,
                        lwm2m_server_t * server,
                        uint16_t mid,
                        uint32_t code,
                        uint8_t* token,
                        uint8_t token_len,
                        uint16_t content_type,
                        uint8_t * payload,
                        size_t payload_len
                       )
{
    lwm2m_transaction_t * transaction;
    async_state_t * async_stateP = &current_async_state;
    coap_packet_t * response;

    /* initialize the transaction */
    transaction = transaction_new(server->sessionH, COAP_GET, NULL, NULL, mid, token_len, token);
    if (transaction == NULL) return false;

    /* set the result code */
    coap_set_status_code(transaction->message, code);

    /* set uri */
    coap_set_header_uri_path(transaction->message, server->location);

    /* set the payload type & payload */
    if (payload != NULL)
    {
        coap_set_header_content_type(transaction->message, content_type);

        /* initiate block transfer for a bigger block */
        if (payload_len > REST_MAX_CHUNK_SIZE)
        {
            LOG_ARG("Initiate Blockwise transfer with block_size %u", REST_MAX_CHUNK_SIZE);
            coap_set_header_block2(transaction->message, 0, 1, REST_MAX_CHUNK_SIZE);

            /* save the response payload */
            async_stateP->bufferP = (uint8_t *)lwm2m_malloc(payload_len);
            if (async_stateP->bufferP == NULL)
            {
                LOG("async buffer allocation failed");
                if (transaction)
                {
                    transaction_free(transaction);
                }

                return false;
            }
            else
            {
                LOG("save async buffer for block transfer");
                async_stateP->length = payload_len;
                memcpy(async_stateP->bufferP, payload, payload_len);
                async_stateP->content_type = content_type;
            }
        }
        coap_set_payload(transaction->message, payload, MIN(payload_len, REST_MAX_CHUNK_SIZE));
    }

    response = transaction->message;
    LOG_ARG("total payload length = %d", response->payload_len);
    LOG_ARG("Block transfer %u/%u/%u @ %u bytes",
                                response->block2_num,
                                response->block2_more,
                                response->block2_size,
                                response->block2_offset);

    /* send transaction */
    contextP->transactionList = (lwm2m_transaction_t *)LWM2M_LIST_ADD(contextP->transactionList, transaction);
    if (transaction_send(contextP, transaction) != 0) return false;

    return true;
}

bool lwm2m_async_response(lwm2m_context_t * contextP,
                          uint16_t shortServerId,
                          uint16_t mid,
                          uint32_t code,
                          uint8_t* token,
                          uint8_t token_len,
                          uint16_t content_type,
                          uint8_t * payload,
                          size_t payload_len)
{
    lwm2m_server_t * targetP;
    uint8_t result;

    LOG_ARG("State: %s, shortServerId: %d", STR_STATE(contextP->state), shortServerId);

    targetP = contextP->serverList;
    if (targetP == NULL)
    {
        if (object_getServers(contextP, false) == -1)
        {
            LOG("No server found");
            return false;
        }
    }
    while (targetP != NULL)
    {
        if (targetP->shortID == shortServerId)
        {
            /* found the server, send async response */
            if (targetP->status == STATE_REGISTERED)
            {
                return prv_async_response(contextP,
                                          targetP,
                                          mid,
                                          code,
                                          token,
                                          token_len,
                                          content_type,
                                          payload,
                                          payload_len);
            }
            else
            {
                LOG("Server unregistered");
                return false;
            }
        }
        targetP = targetP->next;
    }

    /* no server found */
    return false;
}
#endif

uint8_t message_send(lwm2m_context_t * contextP,
                     coap_packet_t * message,
                     void * sessionH)
{
    uint8_t result = COAP_500_INTERNAL_SERVER_ERROR;
    uint8_t * pktBuffer;
    size_t pktBufferLen = 0;
    size_t allocLen;

    LOG("Entering");
    allocLen = coap_serialize_get_size(message);
    LOG_ARG("Size to allocate: %d", allocLen);
    if (allocLen == 0) return COAP_500_INTERNAL_SERVER_ERROR;

    pktBuffer = (uint8_t *)lwm2m_malloc(allocLen);
    if (pktBuffer != NULL)
    {
        pktBufferLen = coap_serialize_message(message, pktBuffer);
        LOG_ARG("coap_serialize_message() returned %d", pktBufferLen);
        if (0 != pktBufferLen)
        {
            result = lwm2m_buffer_send(sessionH, pktBuffer, pktBufferLen, contextP->userData, message->block1_num == 0);
        }
        lwm2m_free(pktBuffer);
    }

    return result;
}

#if SIERRA

void lwm2m_set_push_callback(lwm2m_push_ack_callback_t callbackP)
{
    push_state_t * push_stateP = &current_push_state;
    push_stateP->callbackP = callbackP;
}

static int prv_data_push(lwm2m_context_t * contextP,
                        lwm2m_server_t * serverP,
                        uint8_t * payloadP,
                        size_t payload_len,
                        lwm2m_media_type_t contentType,
                        uint16_t * midPtr
                       )
{
    lwm2m_transaction_t * transaction;
    push_state_t * push_stateP = &current_push_state;

    if ((payloadP == NULL) || (payload_len == 0))
    {
        LOG("Payload invalid");
        return COAP_400_BAD_REQUEST;
    }

    if (push_stateP->bufferP != NULL)
    {
        LOG("Block transfer in progress");
        return COAP_412_PRECONDITION_FAILED;
    }

    transaction = prv_init_push_transaction(contextP, serverP, contentType);
    if (transaction == NULL) return COAP_500_INTERNAL_SERVER_ERROR;

    /* set the payload type & payload */
    if (payloadP != NULL)
    {
        /* initiate block transfer for a bigger block */
        if (payload_len > REST_MAX_CHUNK_SIZE)
        {
            LOG_ARG("Initiate Blockwise transfer with block_size %u", REST_MAX_CHUNK_SIZE);
            coap_set_header_block1(transaction->message, 0, 1, REST_MAX_CHUNK_SIZE);

            push_stateP->contextP = contextP;
            push_stateP->serverP = serverP;

            /* save the response payload */
            push_stateP->bufferP = (uint8_t *)lwm2m_malloc(payload_len);

            if (push_stateP->bufferP == NULL)
            {
                LOG("push buffer allocation failed");
                if (transaction)
                {
                    transaction_free(transaction);
                }

                return COAP_500_INTERNAL_SERVER_ERROR;
            }
            else
            {
                LOG("save push buffer for block transfer");
                push_stateP->buffer_len = payload_len;
                memcpy(push_stateP->bufferP, payloadP, payload_len);
                push_stateP->content_type = contentType;
            }
        }

        // set payload
        coap_set_payload(transaction->message, payloadP, MIN(payload_len, REST_MAX_CHUNK_SIZE));
    }

    // Notify when data push is acked or timed out
    transaction->callback = prv_push_callback;

    // Initiate the transaction.
    contextP->transactionList = (lwm2m_transaction_t *)LWM2M_LIST_ADD(contextP->transactionList, transaction);
    if (transaction_send(contextP, transaction) != 0)
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    // Save the mid of the first transaction for block transfer.
    if (push_stateP->bufferP != NULL)
    {
        push_stateP->firstBlockMid = transaction->mID;
    }

    // This message ID will be passed in the callback function.
    *midPtr = transaction->mID;

    return COAP_NO_ERROR;
}

// End lwm2m data push
void lwm2m_end_push(void)
{
    push_state_t * push_stateP = &current_push_state;
    prv_end_push(push_stateP);
}

// Initiate a data push transaction at "/push"
int lwm2m_data_push(lwm2m_context_t * contextP,
                    uint16_t shortServerID,
                    uint8_t * payloadP,
                    size_t payload_len,
                    lwm2m_media_type_t contentType,
                    uint16_t * midP
                   )
{
    lwm2m_server_t * targetP;
    int result = COAP_404_NOT_FOUND;

    LOG_ARG("State: %s, shortServerID: %d", STR_STATE(contextP->state), shortServerID);

    targetP = contextP->serverList;
    if (targetP == NULL)
    {
        if (object_getServers(contextP, false) == -1)
        {
            LOG("No server found");
            result = COAP_404_NOT_FOUND;
        }
    }

    while (targetP != NULL)
    {
        if (targetP->shortID == shortServerID)
        {
            // found the server, trigger the data push transaction
            if (targetP->status == STATE_REGISTERED)
            {
                // push the data
                result = prv_data_push(contextP, targetP, payloadP, payload_len, contentType, midP);
            }
            else
            {
                LOG("Server unregistered");
                result = COAP_400_BAD_REQUEST;
            }
        }
        targetP = targetP->next;
    }

    // no server found
    return result;
}
#endif
