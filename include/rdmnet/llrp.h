/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

/*! \file rdmnet/llrp.h
 *  \brief Functions for initializing/deinitializing LLRP and handling LLRP
 *         discovery and networking.
 *  \author Christian Reese and Sam Kearney
 */
#ifndef _RDMNET_LLRP_H_
#define _RDMNET_LLRP_H_

#include "lwpa_common.h"
#include "lwpa_cid.h"
#include "lwpa_int.h"
#include "lwpa_error.h"
#include "lwpa_inet.h"
#include "rdm/uid.h"
#include "rdm/message.h"

/*! \defgroup llrp LLRP
 *  \ingroup rdmnet_core_lib
 *  \brief Send and receive Low Level Recovery Protocol (LLRP) messages.
 *
 *  @{
 */

/*! \brief A handle for an instance of LLRP Manager or Target functionality. */
typedef struct LlrpBaseSocket *llrp_socket_t;

/*! An invalid LLRP socket value. */
#define LLRP_SOCKET_INVALID NULL

/*! Identifies the type of data contained in a struct llrp_data. */
typedef enum
{
  /*! This llrp_data contains no data. */
  kLLRPNoData,
  /*! This llrp_data contains an RDM message. */
  kLLRPDataRDM,
  /*! This llrp_data indicates that an LLRP Target was discovered. */
  kLLRPDataDiscoveredTarget,
  /*! This llrp_data indicates that LLRP Manager discovery is finished. */
  kLLRPDataDiscoveryFinished
} llrp_data_t;

/*! Identifies the type of RPT Component with which this LLRP Target is
 *  associated. */
typedef enum
{
  /*! This LLRP Target is associated with an RPT Device. */
  kLLRPCompRPTDevice = 0,
  /*! This LLRP Target is associated with an RPT Controller. */
  kLLRPCompRPTController = 1,
  /*! This LLRP Target is associated with a Broker. */
  kLLRPCompBroker = 2,
  /*! This LLRP Target is standalone or associated with an unknown Component
   *  type. */
  kLLRPCompUnknown = 255
} llrp_component_t;

/*! An RDM command or response sent over LLRP. */
typedef struct LlrpRdmMessage
{
  /*! The CID of the LLRP Component sending this message. */
  LwpaCid source_cid;
  /*! The LLRP transaction number of this message. */
  uint32_t transaction_num;
  /*! The RDM message. */
  RdmBuffer msg;
} LlrpRdmMessage;

/*! A set of information associated with an LLRP Target. */
typedef struct LlrpTarget
{
  /*! The LLRP Target's CID. */
  LwpaCid target_cid;
  /*! The LLRP Target's UID. */
  RdmUid target_uid;
  /*! The LLRP Target's hardware address (usually the MAC address) */
  uint8_t hardware_address[6];
  /*! The type of RPT Component this LLRP Target is associated with. */
  llrp_component_t component_type;
} LlrpTarget;

/*! A data value associated with an LLRP socket. Used with llrp_update(). */
typedef struct LlrpData
{
  /*! The type of data contained in this structure. */
  llrp_data_t type;
  /*! The data. */
  union
  {
    LlrpRdmMessage rdm;
    LlrpTarget discovered_target;
  } contents;
} LlrpData;

#define llrp_data_is_nodata(llrpdataptr) ((llrpdataptr)->type == kLLRPNoData)
#define llrp_data_is_rdm(llrpdataptr) ((llrpdataptr)->type == kLLRPDataRDM)
#define llrp_data_is_disc_target(llrpdataptr) ((llrpdataptr)->type == kLLRPDataDiscoveredTarget)
#define llrp_data_is_disc_finished(llrpdataptr) ((llrpdataptr)->type == kLLRPDataDiscoveryFinished)
#define llrp_data_rdm(llrpdataptr) (&(llrpdataptr)->contents.rdm)
#define llrp_data_disc_target(llrpdataptr) (&(llrpdataptr)->contents.discovered_target)
#define llrp_data_set_nodata(llrpdataptr) ((llrpdataptr)->type = kLLRPNoData)
#define llrp_data_set_rdm(llrpdataptr, rdm_to_set) \
  do                                               \
  {                                                \
    (llrpdataptr)->type = kLLRPDataRDM;            \
    (llrpdataptr)->contents.rdm = rdm_to_set;      \
  } while (0)
#define llrp_data_set_disc_target(llrpdataptr, disc_target_to_set)  \
  do                                                                \
  {                                                                 \
    (llrpdataptr)->type = kLLRPDataDiscoveredTarget;                \
    (llrpdataptr)->contents.discovered_target = disc_target_to_set; \
  } while (0)
#define llrp_data_set_disc_finished(llrpdataptr) ((llrpdataptr)->type = kLLRPDataDiscoveryFinished)

/*! A structure representing an LLRP socket to be polled by llrp_update(). */
typedef struct LlrpPoll
{
  /*! The LLRP socket to be polled. */
  llrp_socket_t handle;
  /*! Any returned data associated with this socket. */
  LlrpData data;
  /*! The result of the llrp_update() operation for this socket. */
  lwpa_error_t err;
} LlrpPoll;

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t llrp_init();
void llrp_deinit();

llrp_socket_t llrp_create_manager_socket(const LwpaIpAddr *netint, const LwpaCid *manager_cid);
llrp_socket_t llrp_create_target_socket(const LwpaIpAddr *netint, const LwpaCid *target_cid, const RdmUid *target_uid,
                                        const uint8_t *hardware_address, llrp_component_t component_type);
bool llrp_close_socket(llrp_socket_t handle);
bool llrp_start_discovery(llrp_socket_t handle, uint8_t filter);
bool llrp_stop_discovery(llrp_socket_t handle);

int llrp_update(LlrpPoll *poll_array, size_t poll_array_size, int timeout_ms);

void llrp_target_update_connection_state(llrp_socket_t handle, bool connected_to_broker);

lwpa_error_t llrp_send_rdm_command(llrp_socket_t handle, const LwpaCid *destination, const RdmBuffer *command,
                                   uint32_t *transaction_num);

lwpa_error_t llrp_send_rdm_response(llrp_socket_t handle, const LwpaCid *destination, const RdmBuffer *response,
                                    uint32_t transaction_num);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif  // _RDMNET_LLRP_H_
