#include "liblwm2m.h"
#include "connection.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include "../../core/er-coap-13/er-coap-13.h"


//--------------------------------------------------------------------------------------------------
/**
 * @brief Enum for LwM2M push content type
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    LWM2MCORE_PUSH_CONTENT_CBOR = 60,    ///< Uncompressed CBOR
    LWM2MCORE_PUSH_CONTENT_ZCBOR = 12118 ///< Proprietary compressed CBOR (zlib + CBOR)
} lwm2mcore_PushContent_t;


//--------------------------------------------------------------------------------------------------
/**
 * @brief CoAP receive stream status (write request block-2)
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    LWM2MCORE_STREAM_NONE = 0,
            ///< Payload is less than 1KB.
    LWM2MCORE_RX_STREAM_START = 1,
        ///< Beginning of a new CoAP receive stream
    LWM2MCORE_RX_STREAM_IN_PROGRESS = 2,
        ///< Incoming CoAP Stream is in progress
    LWM2MCORE_RX_STREAM_END = 3,
        ///< Incoming CoAP Stream completed successfully
    LWM2MCORE_RX_STREAM_ERROR = 4,
        ///< Error in receiving incoming stream
    LWM2MCORE_TX_STREAM_START = 5,
        ///< Start an outgoing stream
    LWM2MCORE_TX_STREAM_IN_PROGRESS = 6,
        ///< Continue streaming
    LWM2MCORE_TX_STREAM_END = 7,
        ///< All blocks sent successfully
    LWM2MCORE_TX_STREAM_ERROR = 8,
        ///< Error in sending stream
    LWM2MCORE_STREAM_INVALID = 9
        ///< Stream status invalid
}
lwm2mcore_StreamStatus_t;

//--------------------------------------------------------------------------------------------------
/**
 * @brief Event types
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    EVENT_TYPE_BOOTSTRAP,       ///< Bootstrap event: started, succeeded or failed
    EVENT_TYPE_REGISTRATION,    ///< Registration event: started, succeeded or failed
    EVENT_TYPE_REG_UPDATE,      ///< Registration update event: started, succeeded or failed
    EVENT_TYPE_DEREG,           ///< Deregistration event: started, succeeded or failed
    EVENT_TYPE_AUTHENTICATION,  ///< Authentication event: started, succeeded or failed
    EVENT_TYPE_RESUMING,        ///< DTLS resuming/re-authentication event: started, succeeded or
                                ///< failed
    EVENT_SESSION,              ///< Session event: started or done with success or failure
    EVENT_TYPE_MAX = 0xFF,      ///< Internal usage
}smanager_EventType_t;

//--------------------------------------------------------------------------------------------------
/**
 * @brief Event status
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    EVENT_STATUS_STARTED,       ///< Event started
    EVENT_STATUS_DONE_SUCCESS,  ///< Event stopped successfully
    EVENT_STATUS_DONE_FAIL,     ///< Event stopped with failure
    EVENT_STATUS_INACTIVE,      ///< Event inactive
    EVENT_STATUS_FINISHING,     ///< Event finishing
    EVENT_STATUS_MAX = 0xFF,    ///< Internal usage
}smanager_EventStatus_t;

//--------------------------------------------------------------------------------------------------
/**
 * @brief Enum for LwM2M push ack callback
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    LWM2MCORE_ACK_RECEIVED = 0,     ///< Data transferred successfully.
    LWM2MCORE_ACK_TIMEOUT,          ///< Transaction time out
    LWM2MCORE_ACK_FAILURE,          ///< Data is not correctly transferred
    LWM2MCORE_ACK_REJECTED          ///< Data is rejected
} lwm2mcore_AckResult_t;


void acl_free(lwm2m_context_t * contextP)
{
    (void) contextP;
    return;
}

void acl_readObject(lwm2m_context_t * contextP)
{
    (void) contextP;
    return;
}

void smanager_SendSessionEvent
(
    smanager_EventType_t eventId,         ///< [IN] Event Id
    smanager_EventStatus_t eventstatus    ///< [IN] Event status
)
{
    (void) eventId;
    (void) eventstatus;
}


bool lwm2mcore_GetRegistrationID
(
    uint16_t shortID,            ///< [IN] Server ID
    char*    registrationIdPtr,  ///< [INOUT] Registration ID pointer
    size_t   len                 ///< [IN] Registration ID length
)
{
    (void) shortID;
    (void) registrationIdPtr;
    (void) len;

    return false;
}


bool lwm2mcore_IsServerActive
(
    uint16_t        serverId        ///< [IN] server ID
)
{
    (void) serverId;
    return false;
}

bool lwm2mcore_IsEdmEnabled
(
    void
)
{
    return false;
}

void lwm2mcore_DataDump
(
    char *descPtr,                  ///< [IN] data description
    void *addrPtr,                  ///< [IN] Data address to be dumped
    int len                         ///< [IN] Data length
)
{
    (void) descPtr;
    (void) addrPtr;
    (void) len;
}

void acl_erase(lwm2m_context_t * contextP)
{
    (void) contextP;
}

bool acl_checkAccess(lwm2m_context_t * contextP,
                     lwm2m_uri_t * uriP,
                     lwm2m_server_t * serverP,
                     coap_packet_t * messageP)
{
    (void) contextP;
    (void) uriP;
    (void) serverP;
    (void) messageP;

    return true;
}

bool acl_addObjectInstance(lwm2m_context_t * contextP, lwm2m_data_t data)
{
    (void) contextP;
    (void) data;

    return false;
}

bool acl_deleteRelatedObjectInstance(lwm2m_context_t * contextP, uint16_t oid, uint16_t oiid)
{
    (void) contextP;
    (void) oid;
    (void) oiid;

    return false;
}

uint8_t lwm2m_report_coap_status
(
    const char* file,  ///< [IN] File path from where this function is called
    const char* func,  ///< [IN] Name of the caller function
    int code           ///< [IN] CoAP error code as defined in RFC 7252 section 12.1.2
)
{
    (void) file;
    (void) func;
    (void) code;

    return 0;
}

void lwm2mcore_SetRegistrationID
(
    uint16_t    shortID,             ///< [IN] Server ID
    const char* registrationIdPtr    ///< [IN] Registration ID pointer
)
{
    (void) shortID;
    (void) registrationIdPtr;
}

void lwm2mcore_DeleteRegistrationID
(
    int    shortID    ///< [IN] Server ID. Use -1 value to delete all servers registration ID.
)
{
    (void) shortID;
}

//--------------------------------------------------------------------------------------------------
/**
 * Stub for the function that executes a previously added post LWM2M request handler for the
 * request that has just been processed and responded to
 */
//--------------------------------------------------------------------------------------------------
void lwm2mcore_ExecPostRequestHandler
(
    void* connP
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Function to force a DTLS handshake
 *
 */
//--------------------------------------------------------------------------------------------------
void smanager_ForceDtlsHandshake
(
    void
)
{}
