/******************************************************************************
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
 ******************************************************************************
 * This file is a part of RDMnet. For more information, go to:
 * https://github.com/ETCLabs/RDMnet
 *****************************************************************************/

#ifndef DNS_MESSAGE_H_
#define DNS_MESSAGE_H_

#include <stdint.h>
#include "etcpal/inet.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DNS_DOMAIN_NAME_MAX_LENGTH 254

typedef enum
{
  kDnsRecordTypeA = 1,
  kDnsRecordTypeNS = 2,
  kDnsRecordTypeCNAME = 5,
  kDnsRecordTypeSOA = 6,
  kDnsRecordTypePTR = 12,
  kDnsRecordTypeAAAA = 28,
  kDnsRecordTypeSRV = 33,
  kDnsRecordTypeOPT = 41,
} dns_record_type_t;

typedef struct DnsHeader
{
  uint16_t message_id;
  uint16_t control;
  uint16_t question_count;
  uint16_t response_count;
  uint16_t authority_count;
  uint16_t additional_count;
} DnsHeader;

typedef struct DnsQuestion
{
  char name[DNS_DOMAIN_NAME_MAX_LENGTH];
  dns_record_type_t type;
} DnsQuestion;

typedef struct DnsRRHeader
{
  char name[DNS_DOMAIN_NAME_MAX_LENGTH];
  dns_record_type_t record_type;
  uint16_t dns_class;
  uint32_t ttl;
  uint16_t length;
} DnsRRHeader;

typedef struct DnsPtrRecord
{
  DnsRRHeader header;
  char ptr_domain_name[DNS_DOMAIN_NAME_MAX_LENGTH];
} DnsPtrRecord;

typedef struct DnsARecord
{
  DnsRRHeader header;
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

bool pack_query(uint8_t* buf, size_t buflen, const DnsHeader* header, const DnsQuestion* question,
                const DnsPtrRecord* answers, size_t num_answers);

#ifdef __cplusplus
}
#endif

#endif /* DNS_MESSAGE_H_ */
