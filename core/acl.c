/*******************************************************************************
*
* Copyright (c) 2017 Sierra Wireless and others.
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
*    Frederic DUR - initial API and implementation
*
*******************************************************************************/

#include "liblwm2m.h"
#include "internals.h"

typedef struct acl_ctrl_ri_s
{   // Linked list:
    struct acl_ctrl_ri_s *  nextP;      // Matches lwm2m_list_t::next
    uint16_t                resInstId;  // Matches lwm2m_list_t::id, ..serverID
    // Resource data:
    uint16_t                accCtrlValue;
} acl_ctrl_ri_t;

typedef struct acl_ctrl_oi_s
{   // Linked list:
    struct acl_ctrl_oi_s *  nextP;      // Matches lwm2m_list_t::next
    uint16_t                objInstId;  // Matches lwm2m_list_t::id
    // Resources
    uint16_t                objectId;       // Object Id
    uint16_t                objectInstId;   // Object instance Id
    uint16_t                accCtrlOwner;   // ACL owner
    acl_ctrl_ri_t*          accCtrlValListP;    // ACL
} acl_ctrl_oi_t;

/**
 * Add an object instance of object 2
 *
 * @return
 *  - true on success
 *  - false on failure.
 */
static bool prv_acl_addObjectInstance(lwm2m_object_t * objectP,
                                      lwm2m_data_t data)
{
    uint32_t i;
    acl_ctrl_oi_t * accCtrlOiP;
    acl_ctrl_ri_t * accCtrlRiP;

    if (objectP == NULL) return false;

    // Read each object instance
    accCtrlOiP = (acl_ctrl_oi_t *)objectP->instanceList;

    if (NULL == accCtrlOiP) return false;

    while (NULL != accCtrlOiP)
    {
        if (accCtrlOiP->objInstId == data.id)
        {
            size_t count;

            // List: key
            accCtrlOiP->objInstId    = data.id;
            // Object instance data
            accCtrlOiP->objectId     = (uint16_t)data.value.asChildren.array[LWM2M_ACL_OBJECTID_ID].value.asInteger;
            accCtrlOiP->objectInstId = (uint16_t)data.value.asChildren.array[LWM2M_ACL_OBJECT_INSTANCE_ID].value.asInteger;
            accCtrlOiP->accCtrlOwner = (uint16_t)data.value.asChildren.array[LWM2M_ACL_OWNER_ID].value.asInteger;

            // Resource instance number
            count = data.value.asChildren.array[LWM2M_ACL_ACCESS_ID].value.asChildren.count;
            accCtrlRiP = lwm2m_malloc(count*sizeof(acl_ctrl_ri_t));
            LOG_ARG("allocation for %d resource instances", count);
            if (!accCtrlRiP)
            {
                return false;
            }

            accCtrlOiP->accCtrlValListP = accCtrlRiP;

            for (i = 0; i < count; i++)
            {
                // ACL: multi-instancied resource
                if (accCtrlRiP + i)
                {
                    (accCtrlRiP + i)->resInstId = data.value.asChildren.array[LWM2M_ACL_ACCESS_ID].value.asChildren.array[i].id;
                    (accCtrlRiP + i)->accCtrlValue = data.value.asChildren.array[LWM2M_ACL_ACCESS_ID].value.asChildren.array[i].value.asInteger;
                    (accCtrlRiP + i)->nextP = NULL;
                    if (i)
                    {
                        (accCtrlRiP + i - 1)->nextP = (accCtrlRiP + i);
                    }
                }
                else
                {
                    return false;
                }
            }
            return true;
        }
        accCtrlOiP = accCtrlOiP->nextP;
    }
    return false;
}

/**
 * Check ACL rights
 *
 * @return
 *  - Bitfield with rights
 *  - 0 if access is not authorized
 */
static int prv_acl_checkRights(uint16_t objectId,
                               uint16_t instanceId,
                               uint16_t shortId,
                               lwm2m_object_t * objectP,
                               uint16_t* ownerP)
{
    int ret = -1;

    acl_ctrl_oi_t * accCtrlOiP = NULL;

    if (NULL == objectP) return ret;
    if (NULL == ownerP) return ret;

    // Read each object instance and check that objectId and instanceId are present
    accCtrlOiP = (acl_ctrl_oi_t *)objectP->instanceList;

    // Check if at leat one object instance exists
    // else accept all commands
    if (accCtrlOiP == NULL)
        // Full access
        return LWM2M_ACL_R_RIGHTS | LWM2M_ACL_W_RIGHTS | LWM2M_ACL_E_RIGHTS | LWM2M_ACL_D_RIGHTS | LWM2M_ACL_C_RIGHTS ;

    while (NULL != accCtrlOiP)
    {
        LOG_ARG("In object 2: access to /%d/%d", accCtrlOiP->objectId, accCtrlOiP->objectInstId);
        if ((accCtrlOiP->objectId == objectId)
         && ((accCtrlOiP->objectInstId == instanceId) || (accCtrlOiP->objectInstId == LWM2M_MAX_ID)))
        {
            LOG_ARG("/%d/%d is found in ACL", objectId, instanceId);
            break;
        }
        accCtrlOiP = accCtrlOiP->nextP;
    }

    if (accCtrlOiP != NULL)
    {
        // One object instance of object 2 includes objectId and instanceId
        // Check the ACL
        int defaultRights = -1;
        acl_ctrl_ri_t* accCtrlValListP = NULL;
        LOG("One object instance of object 2 includes objectId and instanceId");
        LOG_ARG("accCtrlOiP->accCtrlOwner %d", accCtrlOiP->accCtrlOwner);

        // If server short ID is the ACL owner and if ACL resource are not present: full rights
        if ((accCtrlOiP->accCtrlOwner == shortId)
         && (accCtrlOiP->accCtrlValListP == NULL))
        {
            *ownerP = shortId;
            // Full access
            return LWM2M_ACL_R_RIGHTS | LWM2M_ACL_W_RIGHTS | LWM2M_ACL_E_RIGHTS | LWM2M_ACL_D_RIGHTS;
        }

        // Read all ACL resource instances
        accCtrlValListP = accCtrlOiP->accCtrlValListP;
        while (accCtrlValListP != NULL)
        {
            if (accCtrlValListP->resInstId == 0)
            {
                // Default access rights
                defaultRights = accCtrlValListP->accCtrlValue;
            }

            if (accCtrlValListP->resInstId == shortId)
            {
                // Server found
                LOG("Server found");
                ret = accCtrlValListP->accCtrlValue;
                break;
            }
            accCtrlValListP = accCtrlValListP->nextP;
        }

        if (ret != -1)
        {
            // Access rights were found
            *ownerP = accCtrlOiP->accCtrlOwner;
        }
        else if (defaultRights != -1)
        {
            // Access rights were not found
            // but default access rights are defined
            LOG("Default rights");
            ret = defaultRights;
            *ownerP = accCtrlOiP->accCtrlOwner;
        }
        else
        {
            // No access rights
            LOG("No access rights");
            ret = 0;
        }
    }
    else
    {
        LOG("No access rights");
        // No access rights
        ret = 0;
    }
    LOG_ARG("ACL rights 0x%x", ret);

    return ret;
}

/**
 * Read the object 2 and store data in RAM
 */
void acl_readObject(lwm2m_context_t * contextP)
{
    lwm2m_object_t * targetPtr = (lwm2m_object_t*)LWM2M_LIST_FIND(contextP->objectList,
                                                                  LWM2M_ACL_OBJECT_ID);
    if ((targetPtr != NULL) && (targetPtr->readFunc != NULL))
    {
        // Object 2 is registered
        lwm2m_data_t * dataP = NULL;
        int size = 0;
        uint32_t i;
        lwm2m_uri_t uri;
        memset(&uri, 0, sizeof(lwm2m_uri_t));
        uri.flag = LWM2M_URI_FLAG_OBJECT_ID;
        uri.objectId = LWM2M_ACL_OBJECT_ID;

        if (object_readData(contextP, &uri, &size, &dataP) == COAP_205_CONTENT)
        {
            for (i = 0; i < (uint32_t)size; i++)
            {
                if (false == prv_acl_addObjectInstance(targetPtr,
                                                       dataP[i]))
                {
                    LOG("Error when instance was added in object 2");
                }
            }

        }
        lwm2m_data_free(size, dataP);
    }
}

/**
 * Check access rights
 *
 * @return
 *  - true if access is authorized
 *  - false else
 */
bool acl_checkAccess(lwm2m_context_t * contextP,
                     lwm2m_uri_t * uriP,
                     lwm2m_server_t * serverP,
                     coap_packet_t * messageP)
{
    int acl;
    uint16_t acl_owner = 0;
    uint16_t serverNb = 0;
    lwm2m_object_t * targetP;
    lwm2m_server_t * localServerP = (lwm2m_server_t *)contextP->serverList;

    // Get the registered server number
    while (localServerP != NULL)
    {
        serverNb++;
        localServerP = (lwm2m_server_t *)localServerP->next;
    }

    LOG_ARG("serverNb %d", serverNb);

    LOG_ARG("ACL check: short Id %d, for /%d/%d",
            serverP->shortID, uriP->objectId, uriP->instanceId);

    // Check if object 2 is registered
    targetP = (lwm2m_object_t *)LWM2M_LIST_FIND(contextP->objectList, LWM2M_ACL_OBJECT_ID);
    if (NULL == targetP)
    {
        // Object 2 is not registered: accept the command
        if (serverNb == 1)
        {
            LOG("Object 2 is not registered but only one server: accept the command");
            return true;
        }

        LOG("Object 2 is not registered: refuse the command");
        return false;
    }

    // Check if only one server is declared and if no ACL object instances are present
    // In this case, accept any command
    if ((serverNb == 1) && !(targetP->instanceList))
    {
        LOG("Only 1 DM server without any object instance in object 2: accept any command");
        return true;
    }

    // CREATE command
    if ((messageP->code == COAP_POST) && !LWM2M_URI_IS_SET_INSTANCE(uriP))
    {
        // Check if the object ID is present in one object instance of object 2
        acl = prv_acl_checkRights(uriP->objectId,
                                  LWM2M_MAX_ID,
                                  serverP->shortID,
                                  targetP,
                                  &acl_owner);
        // Check Create rights
        if ( (acl > 0)
          && (acl_owner == LWM2M_ACL_OWNER_BOOTSTRAP)
          && ((acl & LWM2M_ACL_C_RIGHTS) == LWM2M_ACL_C_RIGHTS))
            return true;
        else
            return false;
    }

    // Check if the object ID and object instance ID of the incoming command are filled in object 2
    // Check if server short ID is ACL owner and if ACL resource instance is present
    // Check if server short ID is indicated in ACL resource instance
    // If server short ID is not indicated in ACL resource instance, check for default access rights
    acl = prv_acl_checkRights(uriP->objectId,
                              (LWM2M_URI_IS_SET_INSTANCE(uriP))?uriP->instanceId:LWM2M_MAX_ID,
                              serverP->shortID,
                              targetP,
                              &acl_owner);
    if (acl <= 0)
    {
        return false;
    }

    // Check the command type
    LOG_ARG("acl 0x%x", acl);
    // Read rights: concerns commands: Read, Discover, Observe, Write-Attributes
    if ((messageP->code == COAP_GET)
    && ((acl & LWM2M_ACL_R_RIGHTS) == LWM2M_ACL_R_RIGHTS))
        // OK to read
        return true;

    // Write rights
    if ((((messageP->code == COAP_POST) && !LWM2M_URI_IS_SET_RESOURCE(uriP))
      || ((messageP->code == COAP_PUT) && LWM2M_URI_IS_SET_INSTANCE(uriP)))
    && ((acl & LWM2M_ACL_W_RIGHTS) == LWM2M_ACL_W_RIGHTS))
    {
        // Check if object Id = 2
        // In this case, acl owner needs to be compared to server short ID
        if ((uriP->objectId == LWM2M_ACL_OBJECT_ID)
        && (acl_owner == serverP->shortID))
            // OK to write
            return true;

        if (uriP->objectId != LWM2M_ACL_OBJECT_ID)
            return true;
    }

    // Execute rights
    if (((messageP->code == COAP_POST)
      && LWM2M_URI_IS_SET_INSTANCE(uriP)
      && LWM2M_URI_IS_SET_RESOURCE(uriP))
    && ((acl & LWM2M_ACL_E_RIGHTS) == LWM2M_ACL_E_RIGHTS))
        // OK to execute
        return true;

    // Delete rights
    if ((messageP->code == COAP_DELETE)
    && ((acl & LWM2M_ACL_D_RIGHTS) == LWM2M_ACL_D_RIGHTS))
    {
        // Check if object Id = 2
        // In this case, acl owner needs to be compared to server short ID
        if ((uriP->objectId == LWM2M_ACL_OBJECT_ID)
        && (acl_owner == serverP->shortID))
            // OK to delete
            return true;

        if (uriP->objectId != LWM2M_ACL_OBJECT_ID)
            return true;
    }

    return false;
}

/**
 * Free ACL list
 */
void acl_free(lwm2m_context_t * contextP)
{
    lwm2m_object_t * targetPtr = (lwm2m_object_t*)LWM2M_LIST_FIND(contextP->objectList,
                                                                  LWM2M_ACL_OBJECT_ID);
    if (targetPtr)
    {
        acl_ctrl_oi_t * accCtrlOiP = targetPtr->instanceList;
        while (accCtrlOiP)
        {
            lwm2m_free(accCtrlOiP->accCtrlValListP);
            accCtrlOiP = accCtrlOiP->nextP;
            lwm2m_free(accCtrlOiP);
        }
    }
}

/**
 * Delete ACL object instance related to a specific object instance
 *
 * * @return
 *  - true if object instance were deleted
 *  - false on failure
 */
bool acl_deleteRelatedObjectInstance(lwm2m_context_t * contextP, uint16_t oid, uint16_t oiid)
{
    bool result = false;
    lwm2m_object_t * objectP = (lwm2m_object_t*)LWM2M_LIST_FIND(contextP->objectList,
                                                                LWM2M_ACL_OBJECT_ID);
    LOG_ARG("Delete ACL oid for /%d/%d", oid, oiid);
    if (objectP != NULL)
    {
        // Object 2 is registered
        uint16_t instanceId;
        acl_ctrl_oi_t * accCtrlOiP;
        acl_ctrl_ri_t * accCtrlRiP;

        accCtrlOiP = (acl_ctrl_oi_t *)(objectP->instanceList);

        while (accCtrlOiP)
        {
            if ((accCtrlOiP->objectId == oid)
             && (accCtrlOiP->objectInstId == oiid))
            {
                LOG_ARG("Delete object instance /2/%d for /%d/%d",
                        accCtrlOiP->objInstId, accCtrlOiP->objectId, accCtrlOiP->objectInstId);
                if ( (objectP->deleteFunc)
                  && (COAP_202_DELETED == objectP->deleteFunc(accCtrlOiP->objInstId, objectP) ) )
                {
                    return true;
                }
            }
            else
            {
                accCtrlOiP = accCtrlOiP->nextP;
            }
        }
    }
    return false;
}

/**
 * Add an object instance of object 2
 *
 * @return
 *  - true on success
 *  - false on failure.
 */
bool acl_addObjectInstance(lwm2m_context_t * contextP, lwm2m_data_t data)
{
    lwm2m_object_t * objectP = (lwm2m_object_t *)LWM2M_LIST_FIND(contextP->objectList,
                                                                 LWM2M_ACL_OBJECT_ID);
    return prv_acl_addObjectInstance(objectP, data);
}

/**
 * Delete an object instance of object 2
 *
 * @return
 *  - true on success
 *  - false on failure.
 */
bool lwm2m_acl_deleteObjectInstance(lwm2m_object_t * objectP, uint16_t oiid)
{
    acl_ctrl_oi_t * accCtrlOiP;
    acl_ctrl_ri_t * accCtrlRiP;

    if (!objectP)
        return false;

    accCtrlOiP = (acl_ctrl_oi_t *)LWM2M_LIST_FIND(objectP->instanceList, oiid);

    if (accCtrlOiP)
    {
        lwm2m_client_t* clientP;
        LOG_ARG("remove access rights for /2/%d", accCtrlOiP->objInstId);

        // Remove the object Id and corresponding access rights
        accCtrlRiP = accCtrlOiP->accCtrlValListP;
        while (accCtrlRiP)
        {
            acl_ctrl_ri_t * TempP = accCtrlRiP->nextP;
            lwm2m_free(accCtrlRiP);
            accCtrlRiP = TempP;
        }

        // Delete the object instance
        objectP->instanceList = LWM2M_LIST_RM(objectP->instanceList,
                                              accCtrlOiP->objInstId,
                                              &clientP);
        lwm2m_free(accCtrlOiP);
        return true;
    }
    return false;
}

/**
 * Erase all object instances but keep allocation for instanceList
 */
void acl_erase(lwm2m_context_t * contextP)
{
    lwm2m_object_t * objectP = (lwm2m_object_t *)LWM2M_LIST_FIND(contextP->objectList,
                                                                 LWM2M_ACL_OBJECT_ID);
    acl_ctrl_oi_t * accCtrlOiP;

    if (!objectP)
    {
        return;
    }

    accCtrlOiP = objectP->instanceList;
    while (accCtrlOiP)
    {
        lwm2m_free(accCtrlOiP->accCtrlValListP);

        accCtrlOiP->objectId = LWM2M_MAX_ID;
        accCtrlOiP->objectInstId = LWM2M_MAX_ID;
        accCtrlOiP->accCtrlOwner = LWM2M_ACL_OWNER_BOOTSTRAP;
        accCtrlOiP = accCtrlOiP->nextP;
    }
}
