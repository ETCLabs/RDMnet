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

#include "lwmdns_send.h"

#include <string.h>
#include "etcpal/pack.h"
#include "rdmnet/defs.h"
#include "rdmnet/core/mcast.h"
#include "rdmnet/core/opts.h"
#include "lwmdns_common.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#endif

/******************************************************************************
 * Private Macros
 *****************************************************************************/

#define PACK_POINTER_TO(offset, ptr_offset) \
  etcpal_pack_u16b((ptr_offset), (uint16_t)(0xc000 | (offset - mdns_send_buf)))

/******************************************************************************
 * Private Types
 *****************************************************************************/

typedef struct SendSocket
{
  etcpal_socket_t     socket;
  RdmnetMcastNetintId netint_id;
} SendSocket;

/******************************************************************************
 * Private Variables
 *****************************************************************************/

// TODO real number
#define MDNS_SEND_BUF_SIZE 1400
static uint8_t mdns_send_buf[MDNS_SEND_BUF_SIZE];

// clang-format off
static const uint8_t kSubLabelBytes[] = {
    0x04, 0x5f, 0x73, 0x75, 0x62,  // _sub
};
static const uint8_t kRdmnetServiceSuffixBytes[] = {
  0x07, 0x5f, 0x72, 0x64, 0x6d, 0x6e, 0x65, 0x74, // _rdmnet
  0x04, 0x5f, 0x74, 0x63, 0x70, // _tcp
  0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x00 // local
};
// clang-format on

#if RDMNET_DYNAMIC_MEM
static SendSocket* send_sockets;
#else
static SendSocket send_sockets[RDMNET_MAX_MCAST_NETINTS];
#endif
static size_t num_send_sockets;

/******************************************************************************
 * Private function prototypes
 *****************************************************************************/

static void init_send_sockets_array(size_t array_size);
static void send_buf(size_t data_size);

/******************************************************************************
 * Function Definitions
 *****************************************************************************/

etcpal_error_t lwmdns_send_module_init(const RdmnetNetintConfig* netint_config)
{
  if (netint_config && netint_config->netints && netint_config->num_netints > 0)
  {
#if RDMNET_DYNAMIC_MEM
    send_sockets = (SendSocket*)calloc(netint_config->num_netints, sizeof(SendSocket));
    if (!send_sockets)
      return kEtcPalErrNoMem;
    init_send_sockets_array(netint_config->num_netints);
#else
    if (netint_config->num_netints > RDMNET_MAX_MCAST_NETINTS)
      return kEtcPalErrNoMem;
    init_send_sockets_array(RDMNET_MAX_MCAST_NETINTS);
#endif

    for (size_t i = 0; i < netint_config->num_netints; ++i)
    {
      send_sockets[i].netint_id = netint_config->netints[i];
      etcpal_error_t res =
          rc_mcast_get_send_socket(&send_sockets[i].netint_id, E133_MDNS_PORT, &send_sockets[i].socket);
      if (res != kEtcPalErrOk)
      {
        lwmdns_send_module_deinit();
        return res;
      }
      ++num_send_sockets;
    }
  }
  else
  {
    const RdmnetMcastNetintId* mcast_netint_arr;
    size_t                     mcast_netint_arr_size = rc_mcast_get_netint_array(&mcast_netint_arr);

#if RDMNET_DYNAMIC_MEM
    send_sockets = (SendSocket*)calloc(mcast_netint_arr_size, sizeof(SendSocket));
    if (!send_sockets)
      return kEtcPalErrNoMem;
    init_send_sockets_array(mcast_netint_arr_size);
#else
    init_send_sockets_array(RDMNET_MAX_MCAST_NETINTS);
#endif

    for (size_t i = 0; i < mcast_netint_arr_size; ++i)
    {
      send_sockets[i].netint_id = mcast_netint_arr[i];
      etcpal_error_t res =
          rc_mcast_get_send_socket(&send_sockets[i].netint_id, E133_MDNS_PORT, &send_sockets[i].socket);
      if (res != kEtcPalErrOk)
      {
        lwmdns_send_module_deinit();
        return res;
      }
      ++num_send_sockets;
    }
  }

  return kEtcPalErrOk;
}

void lwmdns_send_module_deinit(void)
{
#if RDMNET_DYNAMIC_MEM
  if (send_sockets)
  {
#endif
    for (size_t i = 0; i < num_send_sockets; ++i)
    {
      if (send_sockets[i].socket != ETCPAL_SOCKET_INVALID)
      {
        rc_mcast_release_send_socket(&send_sockets[i].netint_id, E133_MDNS_PORT);
        send_sockets[i].socket = ETCPAL_SOCKET_INVALID;
      }
    }
#if RDMNET_DYNAMIC_MEM
    free(send_sockets);
  }
#endif
}

etcpal_error_t lwmdns_send_query(const RdmnetScopeMonitorRef* ref)
{
  // Start with a zeroed header
  uint8_t* cur_ptr = mdns_send_buf;
  memset(cur_ptr, 0, DNS_HEADER_BYTES);
  cur_ptr += DNS_HEADER_BYTES;

  // Pack the PTR query
  uint8_t* service_sub_offset = cur_ptr;

  uint8_t scope_len = (uint8_t)strlen(ref->scope);
  *cur_ptr++ = scope_len + 1;
  *cur_ptr++ = (uint8_t)'_';
  memcpy(cur_ptr, ref->scope, scope_len);
  cur_ptr += scope_len;

  uint8_t* service_offset = cur_ptr;

  memcpy(cur_ptr, kRdmnetServiceSuffixBytes, sizeof(kRdmnetServiceSuffixBytes));
  cur_ptr += sizeof(kRdmnetServiceSuffixBytes);

  etcpal_pack_u16b(cur_ptr, (uint16_t)kDnsRecordTypePTR);
  cur_ptr += 2;
  uint16_t class_val = DNS_CLASS_IN;
  if (!ref->platform_data.sent_first_query)
    class_val |= 0x8000u;
  etcpal_pack_u16b(cur_ptr, class_val);
  cur_ptr += 2;
  // Update the question count
  etcpal_pack_u16b(&mdns_send_buf[DNS_HEADER_OFFSET_QUESTION_COUNT], 1u);

  uint8_t* answers_offset = cur_ptr;

  uint16_t num_answers = 0;
  for (DiscoveredBroker* db = ref->broker_list; db; db = db->next)
  {
    uint8_t name_size = (uint8_t)strlen(db->service_instance_name);
    if (cur_ptr + name_size + 3 >= mdns_send_buf + MDNS_SEND_BUF_SIZE)
    {
      etcpal_pack_u16b(&mdns_send_buf[DNS_HEADER_OFFSET_ANSWER_COUNT], num_answers);
      etcpal_pack_u16b(&mdns_send_buf[DNS_HEADER_OFFSET_FLAGS], DNS_FLAGS_TRUNCATED_MASK);
      send_buf(cur_ptr - mdns_send_buf);
      cur_ptr = answers_offset;
      num_answers = 0;
      etcpal_pack_u16b(&mdns_send_buf[DNS_HEADER_OFFSET_FLAGS], 0u);
    }

    PACK_POINTER_TO(service_sub_offset, cur_ptr);
    cur_ptr += 2;
    etcpal_pack_u16b(cur_ptr, kDnsRecordTypePTR);
    cur_ptr += 2;
    etcpal_pack_u16b(cur_ptr, DNS_CLASS_IN);
    cur_ptr += 2;
    etcpal_pack_u16b(cur_ptr, (uint16_t)(etcpal_timer_remaining(&db->platform_data.ttl_timer) / 1000));
    cur_ptr += 2;
    etcpal_pack_u16b(cur_ptr, name_size + 3);
    cur_ptr += 2;
    *cur_ptr++ = name_size;
    memcpy(cur_ptr, db->service_instance_name, name_size);
    cur_ptr += name_size;
    PACK_POINTER_TO(service_offset, cur_ptr);
    cur_ptr += 2;
    ++num_answers;
  }

  send_buf(cur_ptr - mdns_send_buf);
  return kEtcPalErrOk;
}

static void init_send_sockets_array(size_t array_size)
{
  for (SendSocket* send_socket = send_sockets; send_socket < send_sockets + array_size; ++send_socket)
  {
    send_socket->socket = ETCPAL_SOCKET_INVALID;
  }
}

static void send_buf(size_t data_size)
{
  for (SendSocket* send_socket = send_sockets; send_socket < send_sockets + num_send_sockets; ++send_socket)
  {
    EtcPalSockAddr send_addr;
    send_addr.port = E133_MDNS_PORT;
    if (send_socket->netint_id.ip_type == kEtcPalIpTypeV4)
    {
      send_addr.ip = *kMdnsIpv4Address;
      etcpal_sendto(send_socket->socket, mdns_send_buf, data_size, 0, &send_addr);
    }
    else
    {
      send_addr.ip = *kMdnsIpv6Address;
      etcpal_sendto(send_socket->socket, mdns_send_buf, data_size, 0, &send_addr);
    }
  }
}
