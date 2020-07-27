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

#ifndef LWMDNS_COMMON_H_
#define LWMDNS_COMMON_H_

#include <stdint.h>
#include "etcpal/inet.h"
#include "rdmnet/disc/discovered_broker.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DNS_DOMAIN_NAME_MAX_LENGTH 254

#define DNS_HEADER_BYTES 12
#define DNS_HEADER_OFFSET_FLAGS 2
#define DNS_HEADER_OFFSET_QUESTION_COUNT 4
#define DNS_HEADER_OFFSET_ANSWER_COUNT 6
#define DNS_HEADER_OFFSET_AUTHORITY_COUNT 8
#define DNS_HEADER_OFFSET_ADDITIONAL_COUNT 10

#define DNS_FLAGS_REQUEST_RESPONSE_MASK 0x8000u
#define DNS_FLAGS_TRUNCATED_MASK 0x0200u

typedef enum
{
  kDnsRecordTypeA = 1,
  kDnsRecordTypeNS = 2,
  kDnsRecordTypeCNAME = 5,
  kDnsRecordTypeSOA = 6,
  kDnsRecordTypePTR = 12,
  kDnsRecordTypeTXT = 16,
  kDnsRecordTypeAAAA = 28,
  kDnsRecordTypeSRV = 33,
  kDnsRecordTypeOPT = 41,
  kDnsRecordTypeANY = 255
} dns_record_type_t;

#define DNS_CLASS_IN 0x0001u
#define DNS_CLASS_CLASS_MASK 0x7fffu
#define DNS_CLASS_CACHE_FLUSH_MASK 0x8000u

#define DNS_NAME_POINTER_MASK 0xc0u

typedef struct DnsHeader
{
  bool     query;
  bool     truncated;
  uint16_t query_count;
  uint16_t answer_count;
  uint16_t authority_count;
  uint16_t additional_count;
} DnsHeader;

/*
typedef struct DnsQuestion
{
  const char*       name;
  dns_record_type_t type;
} DnsQuestion;
*/

typedef struct DnsDomainName
{
  const uint8_t* name;
  const uint8_t* name_ptr;
} DnsDomainName;

typedef struct DnsResourceRecord
{
  DnsDomainName     name;
  dns_record_type_t record_type;
  bool              cache_flush;
  uint32_t          ttl;
  uint16_t          data_len;
  const uint8_t*    data_ptr;
} DnsResourceRecord;

/*
typedef struct DnsPtrRecord
{
  DnsRRHeader header;
  const char* ptr_domain_name;
} DnsPtrRecord;

typedef struct DnsARecord
{
  DnsRRHeader  header;
  EtcPalIpAddr ip;
} DnsARecord;

typedef struct DnsSrvRecord
{
  DnsRRHeader header;
} DnsSrvRecord;

typedef struct DnsTxtRecord
{
  DnsRRHeader header;
} DnxTxtRecord;
*/

typedef enum
{
  kTxtRecordParseOkDataChanged,
  kTxtRecordParseOkNoDataChanged,
  kTxtRecordParseError
} txt_record_parse_result_t;

extern const EtcPalIpAddr* kMdnsIpv4Address;
extern const EtcPalIpAddr* kMdnsIpv6Address;

etcpal_error_t lwmdns_common_module_init(void);
void           lwmdns_common_module_deinit(void);

const uint8_t* lwmdns_parse_dns_header(const uint8_t* buf, int buf_len, DnsHeader* header);
const uint8_t* lwmdns_parse_resource_record(const uint8_t*     buf_begin,
                                            const uint8_t*     offset,
                                            int                total_remaining_length,
                                            DnsResourceRecord* rr);

const uint8_t* lwmdns_parse_domain_name(const uint8_t* buf_begin,
                                        const uint8_t* offset,
                                        int            total_remaining_length,
                                        DnsDomainName* name);
bool           lwmdns_domain_name_matches_service(const DnsDomainName* name,
                                                  const char*          service_instance_name,
                                                  const char*          service_type,
                                                  const char*          domain);
void           lwmdns_convert_domain_name_to_string(const DnsDomainName* name, char* str_buf);

txt_record_parse_result_t lwmdns_txt_record_to_broker_info(const uint8_t*    txt_data,
                                                           uint16_t          txt_data_len,
                                                           DiscoveredBroker* db);

#ifdef __cplusplus
}
#endif

#endif /* LWMDNS_COMMON_H_ */
