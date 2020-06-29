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

#include "lwmdns_common.h"

#include "etcpal/pack.h"
#include "rdmnet/defs.h"

const EtcPalIpAddr* kMdnsIpv4Address;
const EtcPalIpAddr* kMdnsIpv6Address;

/******************************************************************************
 * Private Variables
 *****************************************************************************/

static EtcPalIpAddr mdns_ipv4_addr_internal;
static EtcPalIpAddr mdns_ipv6_addr_internal;

/******************************************************************************
 * Function Definitions
 *****************************************************************************/

etcpal_error_t lwmdns_common_module_init(void)
{
  etcpal_string_to_ip(kEtcPalIpTypeV4, E133_MDNS_IPV4_MULTICAST_ADDRESS, &mdns_ipv4_addr_internal);
  etcpal_string_to_ip(kEtcPalIpTypeV6, E133_MDNS_IPV6_MULTICAST_ADDRESS, &mdns_ipv6_addr_internal);
  kMdnsIpv4Address = &mdns_ipv4_addr_internal;
  kMdnsIpv6Address = &mdns_ipv6_addr_internal;
  return kEtcPalErrOk;
}

void lwmdns_common_module_deinit(void)
{
}

const uint8_t* lwmdns_parse_dns_header(const uint8_t* buf, int buf_len, DnsHeader* header)
{
  if (buf_len < DNS_HEADER_BYTES)
    return NULL;

  uint16_t flags = etcpal_unpack_u16b(&buf[DNS_HEADER_OFFSET_FLAGS]);
  header->query = (flags & DNS_FLAGS_REQUEST_RESPONSE_MASK);
  header->truncated = (flags & DNS_FLAGS_TRUNCATED_MASK);

  header->query_count = etcpal_unpack_u16b(&buf[DNS_HEADER_OFFSET_QUESTION_COUNT]);
  header->answer_count = etcpal_unpack_u16b(&buf[DNS_HEADER_OFFSET_ANSWER_COUNT]);
  header->authority_count = etcpal_unpack_u16b(&buf[DNS_HEADER_OFFSET_AUTHORITY_COUNT]);
  header->additional_count = etcpal_unpack_u16b(&buf[DNS_HEADER_OFFSET_ADDITIONAL_COUNT]);
  return buf + DNS_HEADER_BYTES;
}

const uint8_t* lwmdns_parse_domain_name(const uint8_t* buf_begin,
                                        const uint8_t* offset,
                                        int            total_remaining_length,
                                        DnsDomainName* name)
{
  int            remaining_length = total_remaining_length;
  const uint8_t* cur_ptr = offset;

  name->name = NULL;
  name->name_ptr = NULL;

  while (*cur_ptr != 0)
  {
    if (*cur_ptr & DNS_NAME_POINTER_MASK)
    {
      if (remaining_length >= 2)
      {
        name->name_ptr = buf_begin + (etcpal_unpack_u16b(cur_ptr) & 0x3fffu);
        return cur_ptr + 2;
      }
      else
      {
        return NULL;
      }
    }
    else
    {
      uint8_t cur_label_length = *cur_ptr;
      if (remaining_length >= (((int)cur_label_length) + 2))
      {
        if (name->name == NULL)
          name->name = cur_ptr;
        cur_ptr += cur_label_length + 1;
        remaining_length -= (cur_label_length + 1);
      }
      else
      {
        return NULL;
      }
    }
  }
  return cur_ptr + 1;
}

const uint8_t* lwmdns_parse_resource_record(const uint8_t*     buf_begin,
                                            const uint8_t*     offset,
                                            int                total_remaining_length,
                                            DnsResourceRecord* rr)
{
  const uint8_t* cur_ptr = lwmdns_parse_domain_name(buf_begin, offset, total_remaining_length, &rr->name);
  if (!cur_ptr)
    return cur_ptr;

  if (total_remaining_length - (cur_ptr - offset) >= 10)
  {
    rr->record_type = (dns_record_type_t)etcpal_unpack_u16b(cur_ptr);
    cur_ptr += 2;
    uint16_t class = etcpal_unpack_u16b(cur_ptr);
    if ((class & DNS_CLASS_CLASS_MASK) != DNS_CLASS_IN)
      return NULL;
    rr->cache_flush = (class & DNS_CLASS_CACHE_FLUSH_MASK);
    cur_ptr += 2;
    rr->ttl = etcpal_unpack_u32b(cur_ptr);
    cur_ptr += 4;
    rr->data_len = etcpal_unpack_u16b(cur_ptr);
    cur_ptr += 2;
    if (rr->data_len == 0)
    {
      rr->data_ptr = NULL;
      return cur_ptr;
    }
    else if (total_remaining_length - (cur_ptr - offset) >= rr->data_len)
    {
      rr->data_ptr = cur_ptr;
      cur_ptr += rr->data_len;
      return cur_ptr;
    }
    else
    {
      return NULL;
    }
  }
  return NULL;
}

bool txt_record_to_broker_info(const uint8_t* txt_data, uint16_t txt_data_len, DiscoveredBroker* db)
{
  /*
  bool data_changed = false;

  const uint8_t* cur_ptr = txt_data;
  while (cur_ptr - txt_data < txt_data_len)
  {
    uint8_t txt_len = *cur_ptr++;
    if ((cur_ptr + txt_len - txt_data) > txt_data_len)
      return data_changed;

    for (const uint8_t* data = cur_ptr; data < cur_ptr + txt_len; ++data)
    {
      if ((char)*data == '=')
      {
      }
    }
  }
  */
  ETCPAL_UNUSED_ARG(txt_data);
  ETCPAL_UNUSED_ARG(txt_data_len);
  ETCPAL_UNUSED_ARG(db);
  return false;
}
