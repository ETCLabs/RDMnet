/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2019 ETC Inc.
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

#ifndef _RDMNET_DEFS_H_
#define _RDMNET_DEFS_H_

/******************************************************************************
 * Entertainment Services Technology Association (ESTA)
 * BSR E1.33 (RDMnet) Message Transport and Device Management of ANSI E1.20
 * (RDM) over IP Networks
 ******************************************************************************
 * Appendix A Defines for all RDMnet Protocols.
 * This header is meant as a companion to the RDM.h that is available on
 * http://www.rdmprotocol.org.
 *****************************************************************************/

/* clang-format off */

/* Protocol version. */
#define E133_VERSION 1

#define E133_MDNS_IPV4_MULTICAST_ADDRESS "224.0.0.251"
#define E133_MDNS_IPV6_MULTICAST_ADDRESS "ff02::fb"
#define E133_MDNS_PORT                   5353
#define E133_DEFAULT_SCOPE               "default"
#define E133_DEFAULT_DOMAIN              "local."

/* TODO: THIS IS A DRAFT type.  The SRV_TYPE includes the service protocol and has the prepended '_'
 * in all fields */
#define E133_DNSSD_SRV_TYPE "_draft-e133._tcp."
#define E133_DNSSD_SRV_TYPE_PADDED_LENGTH (sizeof(E133_DNSSD_SRV_TYPE))
#define E133_DNSSD_TXTVERS  1
#define E133_DNSSD_E133VERS 1

#define E133_TCP_HEARTBEAT_INTERVAL_SEC 15 /* seconds */
#define E133_HEARTBEAT_TIMEOUT_SEC      45 /* seconds */
#define E133_CONTROLLER_BACKOFF_SEC     6  /* seconds */

#define E133_NULL_ENDPOINT      0x0000u
#define E133_BROADCAST_ENDPOINT 0xFFFFu

/* Not in tables, but defined elsewhere in the standard */
#define E133_SCOPE_STRING_PADDED_LENGTH           63u  /* Section 6.2.1, 6.3.1.2 */
#define E133_DOMAIN_STRING_PADDED_LENGTH          231u /* Section 6.3.1.2 */
#define E133_SERVICE_NAME_STRING_PADDED_LENGTH    64u  /* RFC676 4.1.1 */
#define E133_MODEL_STRING_PADDED_LENGTH           250u /* Section 7.2.1 */
#define E133_MANUFACTURER_STRING_PADDED_LENGTH    250u /* Section 7.2.1 */

#define LLRP_FILTERVAL_CLIENT_CONN_INACTIVE 0x0001u
#define LLRP_FILTERVAL_BROKERS_ONLY         0x0002u

/******************************************************************************
 * Table A-1: Broadcast UID Defines
 *****************************************************************************/
#define E133_RPT_ALL_CONTROLLERS    0xFFFCFFFFFFFF
#define E133_RPT_ALL_DEVICES        0xFFFDFFFFFFFF
/* #define E133_RPT_ALL_MID_DEVICES 0xFFFDmmmmFFFF (Specific Manufacturer ID 0xmmmm) */

/******************************************************************************
 * Table A-2: LLRP Constants
 *****************************************************************************/
#define LLRP_MULTICAST_IPV4_ADDRESS_REQUEST  "239.255.250.133"
#define LLRP_MULTICAST_IPV4_ADDRESS_RESPONSE "239.255.250.134"
#define LLRP_MULTICAST_IPV6_ADDRESS_REQUEST  "ff18::85:0:0:85"
#define LLRP_MULTICAST_IPV6_ADDRESS_RESPONSE "ff18::85:0:0:86"
#define LLRP_PORT                            5569
#define LLRP_TIMEOUT_MS                      2000 /* milliseconds */
#define LLRP_TARGET_TIMEOUT_MS               500  /* milliseconds */
#define LLRP_MAX_BACKOFF_MS                  1500 /* milliseconds */
#define LLRP_KNOWN_UID_SIZE                  200
#define LLRP_BROADCAST_CID                   "fbad822c-bd0c-4d4c-bdc8-7eabebc85aff"

/******************************************************************************
 * Table A-4: Vector Defines for LLRP PDU
 *****************************************************************************/
#define VECTOR_LLRP_PROBE_REQUEST 0x00000001 /* Section 5.4.2.1 */
#define VECTOR_LLRP_PROBE_REPLY   0x00000002 /* Section 5.4.2.2 */
#define VECTOR_LLRP_RDM_CMD       0x00000003 /* Section 5.4.2.3 */

/******************************************************************************
 * Table A-5: Vector Defines for LLRP Probe Request PDU
 *****************************************************************************/
#define VECTOR_PROBE_REQUEST_DATA 0x01

/******************************************************************************
 * Table A-6: Vector Defines for LLRP Probe Reply PDU
 *****************************************************************************/
#define VECTOR_PROBE_REPLY_DATA 0x01

/******************************************************************************
 * Table A-7: Vector Defines for Broker PDU
 *****************************************************************************/
#define VECTOR_BROKER_FETCH_CLIENT_LIST      0x0001 /* Section 6.3.1.7 */
#define VECTOR_BROKER_CONNECTED_CLIENT_LIST  0x0002 /* Section 6.3.1.8 */
#define VECTOR_BROKER_CLIENT_ADD             0x0003 /* Section 6.3.1.9 */
#define VECTOR_BROKER_CLIENT_REMOVE          0x0004 /* Section 6.3.1.10 */
#define VECTOR_BROKER_CLIENT_ENTRY_CHANGE    0x0005 /* Section 6.3.1.11 */
#define VECTOR_BROKER_CONNECT                0x0006 /* Section 6.3.1.2 */
#define VECTOR_BROKER_CONNECT_REPLY          0x0007 /* Section 6.3.1.3 */
#define VECTOR_BROKER_CLIENT_ENTRY_UPDATE    0x0008 /* Section 6.3.1.4 */
#define VECTOR_BROKER_REDIRECT_V4            0x0009 /* Section 6.3.1.5 */
#define VECTOR_BROKER_REDIRECT_V6            0x000A /* Section 6.3.1.6 */
#define VECTOR_BROKER_DISCONNECT             0x000B /* Section 6.3.1.15 */
#define VECTOR_BROKER_NULL                   0x000C /* Section 6.3.1.16 */
#define VECTOR_BROKER_REQUEST_DYNAMIC_UIDS   0x000D /* Section 6.3.1.12 */
#define VECTOR_BROKER_ASSIGNED_DYNAMIC_UIDS  0x000E /* Section 6.3.1.13 */
#define VECTOR_BROKER_FETCH_DYNAMIC_UID_LIST 0x000F /* Section 6.3.1.14 */

/******************************************************************************
 * Table A-8: Vector Defines for RPT PDU
 *****************************************************************************/
#define VECTOR_RPT_REQUEST      0x00000001 /* Section 7.5.2 */
#define VECTOR_RPT_STATUS       0x00000002 /* Section 7.5.3 */
#define VECTOR_RPT_NOTIFICATION 0x00000003 /* Section 7.5.4 */

/******************************************************************************
 * Table A-9: Vector Defines for Request PDU
 *****************************************************************************/
#define VECTOR_REQUEST_RDM_CMD 0x01 /* Section 7.5.2 */

/******************************************************************************
 * Table A-10: Vector Defines for RPT Status PDU
 *****************************************************************************/
#define VECTOR_RPT_STATUS_UNKNOWN_RPT_UID       0x0001 /* Section 7.5.3.2 */
#define VECTOR_RPT_STATUS_RDM_TIMEOUT           0x0002 /* Section 7.5.3.3 */
#define VECTOR_RPT_STATUS_RDM_INVALID_RESPONSE  0x0003 /* Section 7.5.3.4 */
#define VECTOR_RPT_STATUS_UNKNOWN_RDM_UID       0x0004 /* Section 7.5.3.5 */
#define VECTOR_RPT_STATUS_UNKNOWN_ENDPOINT      0x0005 /* Section 7.5.3.6 */
#define VECTOR_RPT_STATUS_BROADCAST_COMPLETE    0x0006 /* Section 7.5.3.7 */
#define VECTOR_RPT_STATUS_UNKNOWN_VECTOR        0x0007 /* Section 7.5.3.8 */
#define VECTOR_RPT_STATUS_INVALID_MESSAGE       0x0008 /* Section 7.5.3.9 */
#define VECTOR_RPT_STATUS_INVALID_COMMAND_CLASS 0x0009 /* Section 7.5.3.10 */

/******************************************************************************
 * Table A-11: Vector Defines for Notification PDU
 *****************************************************************************/
#define VECTOR_NOTIFICATION_RDM_CMD 0x01 /* Section 7.5.4 */

/******************************************************************************
 * Table A-12: Vector Defines for RDM Command PDU
 *****************************************************************************/
#define VECTOR_RDM_CMD_RDM_DATA 0xCC /* Section 7.5.5 */

/******************************************************************************
 * Table A-13: Vector Defines for EPT PDU
 *****************************************************************************/
#define VECTOR_EPT_DATA   0x00000001 /* Section 8.3.3 */
#define VECTOR_EPT_STATUS 0x00000002 /* Section 8.3.4 */

/******************************************************************************
 * Table A-14: Vector Defines for EPT Status PDU
 *****************************************************************************/
#define VECTOR_EPT_STATUS_UNKNOWN_CID    0x0001 /* Section 8.3.4.2 */
#define VECTOR_EPT_STATUS_UNKNOWN_VECTOR 0x0002 /* Section 8.3.4.3 */

/******************************************************************************
 * Table A-15: RDM Parameter ID Defines
 *****************************************************************************/
/* Category - E1.33 Management   */
#define E133_COMPONENT_SCOPE  0x7FEF
#define E133_SEARCH_DOMAIN    0x7FE0
#define E133_TCP_COMMS_STATUS 0x7FED
#define E133_BROKER_STATUS    0x7FF0

/******************************************************************************
 * Table A-16: Additional Response NACK Reason Codes
 *****************************************************************************/
#define E133_NR_ACTION_NOT_SUPPORTED       0x000B
#define E133_NR_UNKNOWN_SCOPE              0x0012
#define E133_NR_INVALID_STATIC_CONFIG_TYPE 0x0013
#define E133_NR_INVALID_IPV4_ADDRESS       0x0014
#define E133_NR_INVALID_IPV6_ADDRESS       0x0015
#define E133_NR_INVALID_PORT               0x0016

/******************************************************************************
 * Table A-17: Static Config Type Definitions for COMPONENT_SCOPE Parameter
 * Message
 *****************************************************************************/
#define E133_NO_STATIC_CONFIG   0x00
#define E133_STATIC_CONFIG_IPV4 0x01
#define E133_STATIC_CONFIG_IPV6 0x02

/******************************************************************************
 * Table A-18: Broker State Definitions for BROKER_STATUS Parameter Message
 *****************************************************************************/
#define E133_BROKER_DISABLED 0x00
#define E133_BROKER_ACTIVE   0x01
#define E133_BROKER_STANDBY  0x02

/******************************************************************************
 * Table A-19: Connection Status Codes for Broker Connect
 *****************************************************************************/
#define E133_CONNECT_OK                   0x0000 /* Section 9.1.5 */
#define E133_CONNECT_SCOPE_MISMATCH       0x0002 /* Section 9.1.5 */
#define E133_CONNECT_CAPACITY_EXCEEDED    0x0003 /* Section 9.1.5 */
#define E133_CONNECT_DUPLICATE_UID        0x0004 /* Section 9.1.5 */
#define E133_CONNECT_INVALID_CLIENT_ENTRY 0x0005 /* Section 9.1.5 */
#define E133_CONNECT_INVALID_UID          0x0006 /* Section 9.1.5 */

/******************************************************************************
 * Table A-20: Status Codes for Dynamic UID Mapping
 *****************************************************************************/
#define E133_DYNAMIC_UID_STATUS_OK                 0x0000
#define E133_DYNAMIC_UID_STATUS_INVALID_REQUEST    0x0001
#define E133_DYNAMIC_UID_STATUS_UID_NOT_FOUND      0x0002
#define E133_DYNAMIC_UID_STATUS_DUPLICATE_RID      0x0003
#define E133_DYNAMIC_UID_STATUS_CAPACITY_EXHAUSTED 0x0004

/******************************************************************************
 * Table A-21: Client Protocol Codes
 *****************************************************************************/
#define E133_CLIENT_PROTOCOL_RPT 0x00000005u /* Section 6.3.2 */
#define E133_CLIENT_PROTOCOL_EPT 0x0000000Bu /* Section 6.3.2 */

/******************************************************************************
 * Table A-22: RPT Client Type Codes
 *****************************************************************************/
#define E133_RPT_CLIENT_TYPE_DEVICE     0x0000 /* Section 6.3.2.2 */
#define E133_RPT_CLIENT_TYPE_CONTROLLER 0x0001 /* Section 6.3.2.2 */

/******************************************************************************
 * Table A-23: LLRP Component Type Codes
 *****************************************************************************/
#define LLRP_COMPONENT_TYPE_RPT_DEVICE     0x00
#define LLRP_COMPONENT_TYPE_RPT_CONTROLLER 0x01
#define LLRP_COMPONENT_TYPE_BROKER         0x02
#define LLRP_COMPONENT_TYPE_UNKNOWN        0x03

/******************************************************************************
 * Table A-24: RPT Client Disconnect Reason Codes
 *****************************************************************************/
#define E133_DISCONNECT_SHUTDOWN              0x0000
#define E133_DISCONNECT_CAPACITY_EXHAUSTED    0x0001
#define E133_DISCONNECT_INCORRECT_CLIENT_TYPE 0x0002
#define E133_DISCONNECT_HARDWARE_FAULT        0x0003
#define E133_DISCONNECT_SOFTWARE_FAULT        0x0004
#define E133_DISCONNECT_SOFTWARE_RESET        0x0005
#define E133_DISCONNECT_INCORRECT_SCOPE       0x0006
#define E133_DISCONNECT_RPT_RECONFIGURE       0x0007
#define E133_DISCONNECT_LLRP_RECONFIGURE      0x0008
#define E133_DISCONNECT_USER_RECONFIGURE      0x0009

#endif /* _RDMNET_DEFS_H_ */
