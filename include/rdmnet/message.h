/******************************************************************************
 * Copyright 2020 ETC Inc.
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
 ******************************************************************************
 * This file is a part of RDMnet. For more information, go to:
 * https://github.com/ETCLabs/RDMnet
 *****************************************************************************/

/**
 * @file rdmnet/message.h
 * @brief Basic types for parsed RDMnet messages.
 */

#ifndef RDMNET_MESSAGE_H_
#define RDMNET_MESSAGE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "etcpal/acn_rlp.h"
#include "etcpal/inet.h"
#include "etcpal/uuid.h"
#include "rdm/message.h"
#include "rdm/uid.h"
#include "rdmnet/common.h"

/**
 * @addtogroup rdmnet_api_common
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name RDM commands and responses
 * @{
 */

/** An RDMnet RDM command received by this component. */
typedef struct RdmnetRdmCommand
{
  /** The UID of the component that sent this command. */
  RdmUid rdmnet_source_uid;
  /** The local endpoint to which this command is addressed. */
  uint16_t dest_endpoint;
  /** The command's sequence number, to be echoed in its response. */
  uint32_t seq_num;
  /** The header information from the encapsulated RDM command. */
  RdmCommandHeader rdm_header;
  /** Pointer to buffer containing any associated RDM parameter data. */
  const uint8_t* data;
  /** The length of any associated RDM parameter data. */
  uint8_t data_len;
} RdmnetRdmCommand;

/** Whether an RdmnetRdmCommand is addressed to the default responder. */
#define RDMNET_COMMAND_IS_TO_DEFAULT_RESPONDER(cmd_ptr) ((cmd_ptr) && ((cmd_ptr)->dest_endpoint == E133_NULL_ENDPOINT))

/** An RDM command received by this component and saved for a later response. */
typedef struct RdmnetSavedRdmCommand
{
  /** The UID of the component that sent this command. */
  RdmUid rdmnet_source_uid;
  /** The local endpoint to which this command is addressed. */
  uint16_t dest_endpoint;
  /** The command's sequence number, to be echoed in its response. */
  uint32_t seq_num;
  /** The header information from the encapsulated RDM command. */
  RdmCommandHeader rdm_header;
  /** Buffer containing any associated RDM parameter data. */
  uint8_t data[RDM_MAX_PDL];
  /** The length of any associated RDM parameter data. */
  uint8_t data_len;
} RdmnetSavedRdmCommand;

/** An RDMnet RDM response received by a local component. */
typedef struct RdmnetRdmResponse
{
  /** The UID of the RDMnet component that sent this response. */
  RdmUid rdmnet_source_uid;
  /** The endpoint from which the response was sent. */
  uint16_t source_endpoint;
  /** The sequence number of the response, for matching with a corresponding command. */
  uint32_t seq_num;
  /**
   * @brief Whether the response was sent in response to a command previously sent by this controller.
   * @details If this is false, the command was a broadcast sent to all controllers.
   */
  bool is_response_to_me;

  /** The header of the original command associated with this response; valid if seq_num != 0. */
  RdmCommandHeader original_cmd_header;
  /** Any parameter data associated with the original RDM command; valid if seq_num != 0. */
  const uint8_t* original_cmd_data;
  /**
   * The length of the parameter data associated with the original RDM command; valid if
   * seq_num != 0.
   */
  uint8_t original_cmd_data_len;

  /** The header information from the encapsulated RDM response. */
  RdmResponseHeader rdm_header;
  /** Any parameter data associated with the RDM response. */
  const uint8_t* rdm_data;
  /** The length of the parameter data associated with the RDM response. */
  size_t rdm_data_len;

  /**
   * This message contains partial RDM data. This can be set when the library runs out of static
   * memory in which to store RDM response data and must deliver a partial data buffer before
   * continuing (this only applies to the data buffer within the RDM response). The application
   * should store the partial data but should not act on it until another RdmnetRdmResponse
   * is received with more_coming set to false.
   */
  bool more_coming;
} RdmnetRdmResponse;

/**
 * @brief An RDM response received over RDMnet and saved for later processing.
 *
 * This type is not used by the library API, but can come in handy if an application wants to queue
 * or copy RDM responses before acting on them. The rdm_data member is heap-allocated and owned; be
 * sure to call rdmnet_free_saved_rdm_response() to free this data before disposing of an instance.
 */
typedef struct RdmnetSavedRdmResponse
{
  /** The UID of the RDMnet component that sent this response. */
  RdmUid rdmnet_source_uid;
  /** The endpoint from which the response was sent. */
  uint16_t source_endpoint;
  /** The sequence number of the response, for matching with a corresponding command. */
  uint32_t seq_num;
  /**
   * @brief Whether the response was sent in response to a command previously sent by this controller.
   * @details If this is false, the command was a broadcast sent to all controllers.
   */
  bool is_response_to_me;

  /** The header of the original command associated with this response; valid if seq_num != 0. */
  RdmCommandHeader original_cmd_header;
  /** Any parameter data associated with the original RDM command; valid if seq_num != 0. */
  uint8_t original_cmd_data[RDM_MAX_PDL];
  /**
   * The length of the parameter data associated with the original RDM command; valid if
   * seq_num != 0.
   */
  uint8_t original_cmd_data_len;

  /** The header information from the encapsulated RDM response. */
  RdmResponseHeader rdm_header;
  /**
   * Any parameter data associated with the RDM response. This pointer is owned and must be freed
   * before this type goes out of scope using rdmnet_free_saved_rdm_response().
   */
  uint8_t* rdm_data;
  /** The length of the parameter data associated with the RDM response. */
  size_t rdm_data_len;
} RdmnetSavedRdmResponse;

/**
 * @brief Whether the original command is included in an RdmnetRdmResponse or RdmnetSavedRdmResponse.
 *
 * If this is true, the members original_cmd_header, original_cmd_data and original_cmd_data_len
 * will be valid; otherwise, they contain unspecified values.
 *
 * @param resp Pointer to RdmnetRdmResponse or RdmnetSavedRdmResponse to inspect.
 */
#define RDMNET_RESP_ORIGINAL_COMMAND_INCLUDED(resp) ((resp) && ((resp)->seq_num == 0))

/**
 * @}
 */

/**
 * @name Other RPT messages
 * @{
 */

/** An RDMnet RPT status message received by a local component. */
typedef struct RdmnetRptStatus
{
  /** The UID of the RDMnet component that sent this status message. */
  RdmUid source_uid;
  /** The endpoint from which the status message was sent. */
  uint16_t source_endpoint;
  /** The sequence number of the status message, for matching with a corresponding command. */
  uint32_t seq_num;
  /** A status code that indicates the specific error or status condition. */
  rpt_status_code_t status_code;
  /** An optional implementation-defined status string to accompany this status message. */
  const char* status_string;
} RdmnetRptStatus;

/**
 * @brief An RPT status received over RDMnet and saved for later processing.
 *
 * This type is not used by the library API, but can come in handy if an application wants to queue
 * or copy RPT status messages before acting on them. The status_string member is heap-allocated
 * and owned; be sure to call rdmnet_free_saved_rpt_status() to free this data before disposing of
 * an instance.
 */
typedef struct RdmnetSavedRptStatus
{
  /** The UID of the RDMnet component that sent this status message. */
  RdmUid source_uid;
  /** The endpoint from which the status message was sent. */
  uint16_t source_endpoint;
  /** The sequence number of the status message, for matching with a corresponding command. */
  uint32_t seq_num;
  /** A status code that indicates the specific error or status condition. */
  rpt_status_code_t status_code;
  /**
   * An optional implementation-defined status string to accompany this status message. This
   * pointer is owned and must be freed before this type goes out of scope using
   * rdmnet_free_saved_rpt_status().
   */
  char* status_string;
} RdmnetSavedRptStatus;

/** A mapping from a dynamic UID to a responder ID (RID). */
typedef struct RdmnetDynamicUidMapping
{
  /** The response code - indicates whether the broker was able to assign or look up dynamic UID. */
  rdmnet_dynamic_uid_status_t status_code;
  /** The dynamic UID. */
  RdmUid uid;
  /** The corresponding RID to which the dynamic UID is mapped. */
  EtcPalUuid rid;
} RdmnetDynamicUidMapping;

/** A list of mappings from dynamic UIDs to responder IDs received from an RDMnet broker. */
typedef struct RdmnetDynamicUidAssignmentList
{
  /** An array of dynamic UID mappings. */
  RdmnetDynamicUidMapping* mappings;
  /** The size of the mappings array. */
  size_t num_mappings;
  /**
   * This message contains a partial list. This can be set when the library runs out of static
   * memory in which to store RdmnetDynamicUidMappings and must deliver the partial list before
   * continuing. The application should store the entries in the list but should not act on the
   * list until another RdmnetDynamicUidAssignmentList is received with more_coming set to false.
   */
  bool more_coming;
} RdmnetDynamicUidAssignmentList;

/**
 * @}
 */

/**
 * @name EPT messages
 * @{
 */

/** An RDMnet EPT data message received by a local component. */
typedef struct RdmnetEptData
{
  /** The CID of the EPT client that sent this data. */
  EtcPalUuid source_cid;
  /** The ESTA manufacturer ID that identifies the EPT sub-protocol. */
  uint16_t manufacturer_id;
  /** The protocol ID that identifies the EPT sub-protocol. */
  uint16_t protocol_id;
  /** The data associated with this EPT message. */
  const uint8_t* data;
  /** The length of the data associated with this EPT message. */
  size_t data_len;
} RdmnetEptData;

/**
 * @brief An EPT data message received over RDMnet and saved for later processing.
 *
 * This type is not used by the library API, but can come in handy if an application wants to queue
 * or copy EPT data messages before acting on them. The data member is heap-allocated and owned; be
 * sure to call rdmnet_free_saved_ept_data() to free this data before disposing of an instance.
 */
typedef struct RdmnetSavedEptData
{
  /** The CID of the EPT client that sent this data. */
  EtcPalUuid source_cid;
  /** The ESTA manufacturer ID that identifies the EPT sub-protocol. */
  uint16_t manufacturer_id;
  /** The protocol ID that identifies the EPT sub-protocol. */
  uint16_t protocol_id;
  /**
   * The data associated with this EPT message. This pointer is owned and must be freed before this
   * type goes out of scope using rdmnet_free_saved_ept_data().
   */
  const uint8_t* data;
  /** The length of the data associated with this EPT message. */
  size_t data_len;
} RdmnetSavedEptData;

/** An RDMnet EPT status message received by a local component. */
typedef struct RdmnetEptStatus
{
  /** The CID of the EPT client that sent this status message. */
  EtcPalUuid source_cid;
  /** A status code that indicates the specific error or status condition. */
  ept_status_code_t status_code;
  /** An optional implementation-defined status string to accompany this status message. */
  const char* status_string;
} RdmnetEptStatus;

/**
 * @brief An EPT status received over RDMnet and saved for later processing.
 *
 * This type is not used by the library API, but can come in handy if an application wants to queue
 * or copy EPT status messages before acting on them. The status_string member is heap-allocated
 * and owned; be sure to call rdmnet_free_saved_ept_status() to free this data before disposing of
 * an instance.
 */
typedef struct RdmnetSavedEptStatus
{
  /** The CID of the EPT client that sent this status message. */
  EtcPalUuid source_cid;
  /** A status code that indicates the specific error or status condition. */
  ept_status_code_t status_code;
  /**
   * An optional implementation-defined status string to accompany this status message. This
   * pointer is owned and must be freed before this type goes out of scope using
   * rdmnet_free_saved_ept_status().
   */
  const char* status_string;
} RdmnetSavedEptStatus;

/**
 * @}
 */

/**
 * @name Client list messages
 * @{
 */

/** An RPT client type. */
typedef enum
{
  /** An RPT device receives RDM commands and sends responses. */
  kRPTClientTypeDevice = E133_RPT_CLIENT_TYPE_DEVICE,
  /** An RPT controller originates RDM commands and receives responses. */
  kRPTClientTypeController = E133_RPT_CLIENT_TYPE_CONTROLLER,
  /** A placeholder for when a type has not been determined. */
  kRPTClientTypeUnknown = 0xffffffff
} rpt_client_type_t;

/** A descriptive structure for an RPT client. */
typedef struct RdmnetRptClientEntry
{
  EtcPalUuid        cid;  /**< The client's Component Identifier (CID). */
  RdmUid            uid;  /**< The client's RDM UID. */
  rpt_client_type_t type; /**< Whether the client is a controller or device. */
  EtcPalUuid binding_cid; /**< An optional identifier for another component that the client is associated with. */
} RdmnetRptClientEntry;

/** The maximum length of an EPT sub-protocol string, including the null terminator. */
#define EPT_PROTOCOL_STRING_PADDED_LENGTH 32

/**
 * @brief A description of an EPT sub-protocol.
 *
 * EPT clients can implement multiple protocols, each of which is identified by a two-part
 * identifier including an ESTA manufacturer ID and a protocol ID.
 */
typedef struct RdmnetEptSubProtocol
{
  /** The ESTA manufacturer ID under which this protocol is namespaced. */
  uint16_t manufacturer_id;
  /** The identifier for this protocol. */
  uint16_t protocol_id;
  /** A descriptive string for the protocol. */
  const char* protocol_string;
} RdmnetEptSubProtocol;

/** A descriptive structure for an EPT client. */
typedef struct RdmnetEptClientEntry
{
  EtcPalUuid            cid;           /**< The client's Component Identifier (CID). */
  RdmnetEptSubProtocol* protocols;     /**< A list of EPT protocols that this client implements. */
  size_t                num_protocols; /**< The size of the protocols array. */
} RdmnetEptClientEntry;

/** A structure that represents a list of RPT Client Entries. */
typedef struct RdmnetRptClientList
{
  /** An array of RPT Client Entries. */
  RdmnetRptClientEntry* client_entries;
  /** The size of the client_entries array. */
  size_t num_client_entries;
  /**
   * This message contains a partial list. This can be set when the library runs out of static
   * memory in which to store Client Entries and must deliver the partial list before continuing.
   * The application should store the entries in the list but should not act on the list until
   * another RdmnetRptClientList is received with more_coming set to false.
   */
  bool more_coming;
} RdmnetRptClientList;

/** A structure that represents a list of EPT Client Entries. */
typedef struct RdmnetEptClientList
{
  /** An array of EPT Client Entries. */
  RdmnetEptClientEntry* client_entries;
  /** The size of the client_entries array. */
  size_t num_client_entries;
  /**
   * This message contains a partial list. This can be set when the library runs out of static
   * memory in which to store Client Entries and must deliver the partial list before continuing.
   * The application should store the entries in the list but should not act on the list until
   * another RdmnetEptClientList is received with more_coming set to false.
   */
  bool more_coming;
} RdmnetEptClientList;

/**
 * @}
 */

/**
 * @name LLRP messages
 * @{
 */

/** An RDM command received from a remote LLRP Manager. */
typedef struct LlrpRdmCommand
{
  /** The CID of the LLRP Manager from which this command was received. */
  EtcPalUuid source_cid;
  /** The sequence number received with this command, to be echoed in the corresponding response. */
  uint32_t seq_num;
  /**
   * An ID for the network interface on which this command was received. This helps the LLRP
   * library send the response on the same interface on which it was received.
   */
  EtcPalMcastNetintId netint_id;
  /** The header information from the encapsulated RDM command. */
  RdmCommandHeader rdm_header;
  /** Pointer to buffer containing any associated RDM parameter data. */
  const uint8_t* data;
  /** The length of any associated RDM parameter data. */
  uint8_t data_len;
} LlrpRdmCommand;

/** An RDM command received from a remote LLRP Manager. */
typedef struct LlrpSavedRdmCommand
{
  /** The CID of the LLRP Manager from which this command was received. */
  EtcPalUuid source_cid;
  /** The sequence number received with this command, to be echoed in the corresponding response. */
  uint32_t seq_num;
  /**
   * An ID for the network interface on which this command was received. This helps the LLRP
   * library send the response on the same interface on which it was received.
   */
  EtcPalMcastNetintId netint_id;
  /** The header information from the encapsulated RDM command. */
  RdmCommandHeader rdm_header;
  /** Pointer to buffer containing any associated RDM parameter data. */
  uint8_t data[RDM_MAX_PDL];
  /** The length of any associated RDM parameter data. */
  uint8_t data_len;
} LlrpSavedRdmCommand;

/** An RDM response received from a remote LLRP Target. */
typedef struct LlrpRdmResponse
{
  /** The CID of the LLRP Target from which this command was received. */
  EtcPalUuid source_cid;
  /** The sequence number of this response (to be associated with a previously-sent command). */
  uint32_t seq_num;
  /** The header information from the encapsulated RDM response. */
  RdmResponseHeader rdm_header;
  /** Any parameter data associated with the RDM response. */
  const uint8_t* rdm_data;
  /** The length of the parameter data associated with the RDM response. */
  uint8_t rdm_data_len;
} LlrpRdmResponse;

/** An RDM command received from a remote LLRP Manager. */
typedef struct LlrpSavedRdmResponse
{
  /** The CID of the LLRP Target from which this command was received. */
  EtcPalUuid source_cid;
  /** The sequence number of this response (to be associated with a previously-sent command). */
  uint32_t seq_num;
  /** The header information from the encapsulated RDM response. */
  RdmResponseHeader rdm_header;
  /** Any parameter data associated with the RDM response. */
  uint8_t rdm_data[RDM_MAX_PDL];
  /** The length of the parameter data associated with the RDM response. */
  uint8_t rdm_data_len;
} LlrpSavedRdmResponse;

/**
 * @}
 */

const char* rdmnet_rpt_client_type_to_string(rpt_client_type_t client_type);

etcpal_error_t rdmnet_save_rdm_command(const RdmnetRdmCommand* command, RdmnetSavedRdmCommand* saved_command);
etcpal_error_t rdmnet_save_rdm_response(const RdmnetRdmResponse* response, RdmnetSavedRdmResponse* saved_response);
etcpal_error_t rdmnet_append_to_saved_rdm_response(const RdmnetRdmResponse* new_response,
                                                   RdmnetSavedRdmResponse*  previously_saved_response);
etcpal_error_t rdmnet_save_rpt_status(const RdmnetRptStatus* status, RdmnetSavedRptStatus* saved_status);

etcpal_error_t rdmnet_copy_saved_rdm_response(const RdmnetSavedRdmResponse* saved_resp_old,
                                              RdmnetSavedRdmResponse*       saved_resp_new);
etcpal_error_t rdmnet_copy_saved_rpt_status(const RdmnetSavedRptStatus* saved_status_old,
                                            RdmnetSavedRptStatus*       saved_status_new);

etcpal_error_t rdmnet_free_saved_rdm_response(RdmnetSavedRdmResponse* saved_response);
etcpal_error_t rdmnet_free_saved_rpt_status(RdmnetSavedRptStatus* saved_status);

etcpal_error_t rdmnet_save_ept_data(const RdmnetEptData* data, RdmnetSavedEptData* saved_data);
etcpal_error_t rdmnet_save_ept_status(const RdmnetEptStatus* status, RdmnetSavedEptStatus* saved_status);

etcpal_error_t rdmnet_copy_saved_ept_data(const RdmnetSavedEptData* saved_data_old, RdmnetSavedEptData* saved_data_new);
etcpal_error_t rdmnet_copy_saved_ept_status(const RdmnetSavedEptStatus* saved_status_old,
                                            RdmnetSavedEptStatus*       saved_status_new);

etcpal_error_t rdmnet_free_saved_ept_data(RdmnetSavedEptData* saved_data);
etcpal_error_t rdmnet_free_saved_ept_status(RdmnetSavedEptStatus* saved_status);

etcpal_error_t rdmnet_save_llrp_rdm_command(const LlrpRdmCommand* command, LlrpSavedRdmCommand* saved_command);
etcpal_error_t rdmnet_save_llrp_rdm_response(const LlrpRdmResponse* response, LlrpSavedRdmResponse* saved_response);
etcpal_error_t rdmnet_copy_saved_llrp_rdm_response(const LlrpSavedRdmResponse* saved_resp_old,
                                                   LlrpSavedRdmResponse*       saved_resp_new);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* RDMNET_MESSAGE_H_ */
