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

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "etcpal/pack.h"
#include "etcpal/uuid.h"
#include "rdmnet/defs.h"
#include "rdmnet/disc/common.h"
#include "rdmnet/core/util.h"

/******************************************************************************
 * Private Types
 *****************************************************************************/

typedef struct TxtRecordItemRef
{
  const uint8_t* key;
  uint8_t        key_len;
  const uint8_t* value;
  uint8_t        value_len;
} TxtRecordItemRef;

typedef struct DomainNameLabel
{
  uint8_t        length;
  const uint8_t* label;
} DomainNameLabel;

#define DOMAIN_NAME_LABEL_INIT \
  {                            \
    0, NULL                    \
  }

/******************************************************************************
 * Private Constants
 *****************************************************************************/

#define DNS_SD_SERVICE_TYPE_MAX_LEN 20
#define DNS_LABEL_MAX_LEN 63

typedef uint32_t txt_keys_found_mask_t;

#define TXT_KEY_E133SCOPE_FOUND_MASK 0x00000001u
#define TXT_KEY_E133VERS_FOUND_MASK 0x00000002u
#define TXT_KEY_CID_FOUND_MASK 0x00000004u
#define TXT_KEY_UID_FOUND_MASK 0x00000008u
#define TXT_KEY_MODEL_FOUND_MASK 0x00000010u
#define TXT_KEY_MANUF_FOUND_MASK 0x00000020u

#define ALL_TXT_KEYS_FOUND_MASK 0x0000003fu
#define ALL_TXT_KEYS_FOUND(mask_val) (((mask_val)&ALL_TXT_KEYS_FOUND_MASK) == ALL_TXT_KEYS_FOUND_MASK)

/******************************************************************************
 * Global Variables
 *****************************************************************************/

const EtcPalIpAddr* kMdnsIpv4Address;
const EtcPalIpAddr* kMdnsIpv6Address;

/******************************************************************************
 * Private Variables
 *****************************************************************************/

static EtcPalIpAddr mdns_ipv4_addr_internal;
static EtcPalIpAddr mdns_ipv6_addr_internal;

/******************************************************************************
 * Private function prototypes
 *****************************************************************************/

static bool parse_txt_vers(const TxtRecordItemRef* item);
static bool parse_txt_item(const TxtRecordItemRef* item, DiscoveredBroker* db, txt_keys_found_mask_t* found_mask);
static bool parse_e133_scope_item(const TxtRecordItemRef* item,
                                  DiscoveredBroker*       db,
                                  txt_keys_found_mask_t*  found_mask);
static bool parse_e133_vers_item(const TxtRecordItemRef* item, DiscoveredBroker* db, txt_keys_found_mask_t* found_mask);
static bool parse_cid_item(const TxtRecordItemRef* item, DiscoveredBroker* db, txt_keys_found_mask_t* found_mask);
static bool parse_uid_item(const TxtRecordItemRef* item, DiscoveredBroker* db, txt_keys_found_mask_t* found_mask);
static bool parse_model_item(const TxtRecordItemRef* item, DiscoveredBroker* db, txt_keys_found_mask_t* found_mask);
static bool parse_manufacturer_item(const TxtRecordItemRef* item,
                                    DiscoveredBroker*       db,
                                    txt_keys_found_mask_t*  found_mask);
static int  binary_atoi(const uint8_t* ascii_val, uint8_t ascii_val_len);

static bool get_domain_name_label(const uint8_t* buf_begin, const uint8_t* name_begin, DomainNameLabel* label);
static bool is_rdmnet_service_type_and_domain(const uint8_t* buf_begin, DomainNameLabel* last_non_service_label);

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

const uint8_t* lwmdns_parse_resource_record(const uint8_t*     buf_begin,
                                            const uint8_t*     rr_ptr,
                                            int                total_remaining_length,
                                            DnsResourceRecord* rr)
{
  const uint8_t* cur_ptr = lwmdns_parse_domain_name(buf_begin, rr_ptr, total_remaining_length);
  if (!cur_ptr)
    return cur_ptr;

  if (total_remaining_length - (cur_ptr - rr_ptr) >= 10)
  {
    rr->name = rr_ptr;
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
    else if (total_remaining_length - (cur_ptr - rr_ptr) >= rr->data_len)
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

const uint8_t* lwmdns_parse_domain_name(const uint8_t* buf_begin, const uint8_t* offset, int total_remaining_length)
{
  int            remaining_length = total_remaining_length;
  const uint8_t* cur_ptr = offset;

  while (*cur_ptr != 0)
  {
    if (*cur_ptr & DNS_NAME_POINTER_MASK)
    {
      if (remaining_length >= 2)
      {
        uint16_t pointer_offset = (etcpal_unpack_u16b(cur_ptr) & 0x3fffu);
        if (buf_begin + pointer_offset < offset)
          return cur_ptr + 2;
        else
          return NULL;
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

uint8_t lwmdns_copy_domain_name(const uint8_t* buf_begin, const uint8_t* name_ptr, uint8_t* buf)
{
  size_t          size_copied = 0;
  DomainNameLabel label = DOMAIN_NAME_LABEL_INIT;
  if (!get_domain_name_label(buf_begin, name_ptr, &label))
    return 0;

  do
  {
    if (size_copied + label.length + 2 > DNS_FQDN_MAX_LENGTH)
      return 0;

    memcpy(&buf[size_copied], label.label - 1, label.length + 1);
    size_copied += label.length + 1;
  } while (get_domain_name_label(buf_begin, NULL, &label));

  buf[size_copied] = 0;
  return (uint8_t)(size_copied + 1);
}

uint8_t lwmdns_domain_name_length(const uint8_t* buf_begin, const uint8_t* name_ptr)
{
  size_t          length = 0;
  DomainNameLabel label = DOMAIN_NAME_LABEL_INIT;
  if (!get_domain_name_label(buf_begin, name_ptr, &label))
    return 0;

  do
  {
    if (length + label.length + 2 > DNS_FQDN_MAX_LENGTH)
      return 0;
    length += label.length + 1;
  } while (get_domain_name_label(buf_begin, NULL, &label));
  return (uint8_t)(length + 1);
}

bool lwmdns_domain_names_equal(const uint8_t* buf_begin_a,
                               const uint8_t* name_a,
                               const uint8_t* buf_begin_b,
                               const uint8_t* name_b)
{
  DomainNameLabel label_a = DOMAIN_NAME_LABEL_INIT;
  DomainNameLabel label_b = DOMAIN_NAME_LABEL_INIT;

  bool a_result = get_domain_name_label(buf_begin_a, name_a, &label_a);
  bool b_result = get_domain_name_label(buf_begin_b, name_b, &label_b);
  while (a_result && b_result)
  {
    if (label_a.length != label_b.length || memcmp(label_a.label, label_b.label, label_a.length) != 0)
      return false;
    a_result = get_domain_name_label(buf_begin_a, NULL, &label_a);
    b_result = get_domain_name_label(buf_begin_b, NULL, &label_b);
  }
  return a_result == b_result;
}

bool lwmdns_domain_name_matches_service_instance(const uint8_t* buf_begin,
                                                 const uint8_t* name_ptr,
                                                 const char*    service_instance_name)
{
  if (!buf_begin || !name_ptr || !service_instance_name)
    return false;

  size_t service_name_len = strlen(service_instance_name);
  if (service_name_len > DNS_LABEL_MAX_LEN)
    return false;

  DomainNameLabel label = DOMAIN_NAME_LABEL_INIT;
  // Compare the service instance name
  if (!get_domain_name_label(buf_begin, name_ptr, &label) || label.length != (uint8_t)service_name_len ||
      memcmp(label.label, service_instance_name, service_name_len) != 0)
  {
    return false;
  }

  return is_rdmnet_service_type_and_domain(buf_begin, &label);
}

bool lwmdns_domain_name_matches_service_subtype(const uint8_t* buf_begin, const uint8_t* name_ptr, const char* scope)
{
  if (!buf_begin || !name_ptr || !scope)
    return false;

  size_t scope_len = strlen(scope);
  if (scope_len > DNS_LABEL_MAX_LEN - 1)
    return false;

  DomainNameLabel label = DOMAIN_NAME_LABEL_INIT;
  // Compare the scope - it should be prefixed with an underscore in the domain name
  if (!get_domain_name_label(buf_begin, name_ptr, &label) || label.length != ((uint8_t)scope_len) + 1 ||
      label.label[0] != (uint8_t)'_' || memcmp(&label.label[1], scope, scope_len) != 0)
  {
    return false;
  }

  // Compare the subtype separator (_sub)
  if (!get_domain_name_label(buf_begin, NULL, &label) || (label.length != (sizeof("_sub") - 1)) ||
      memcmp(label.label, "_sub", sizeof("_sub") - 1) != 0)
  {
    return false;
  }

  return is_rdmnet_service_type_and_domain(buf_begin, &label);
}

bool lwmdns_domain_label_to_string(const uint8_t* buf_begin, const uint8_t* label, char* str_buf)
{
  if (!buf_begin || !label || !str_buf)
    return false;

  DomainNameLabel label_data = DOMAIN_NAME_LABEL_INIT;
  if (get_domain_name_label(buf_begin, label, &label_data))
  {
    memcpy(str_buf, label_data.label, label_data.length);
    str_buf[label_data.length] = '\0';
    return true;
  }
  return false;
}

txt_record_parse_result_t lwmdns_txt_record_to_broker_info(const uint8_t*    txt_data,
                                                           uint16_t          txt_data_len,
                                                           DiscoveredBroker* db)
{
  txt_keys_found_mask_t keys_found = 0;
  bool                  data_changed = false;

  const uint8_t* cur_ptr = txt_data;
  bool           parsed_txt_vers = false;
  while (cur_ptr - txt_data < txt_data_len)
  {
    uint8_t txt_len = *cur_ptr++;
    if ((cur_ptr + txt_len - txt_data) > txt_data_len)
      break;

    TxtRecordItemRef item;
    item.key = cur_ptr;
    bool found_equals = false;

    for (const uint8_t* data = cur_ptr; data < cur_ptr + txt_len; ++data)
    {
      if ((char)*data == '=')
      {
        found_equals = true;
        item.key_len = (uint8_t)(data - cur_ptr);
        item.value = data + 1;
        item.value_len = txt_len - (item.key_len + 1);
        if (item.key_len == 0 || item.key_len + 1 > txt_len)
          break;

        if (parsed_txt_vers)
        {
          if (parse_txt_item(&item, db, &keys_found))
            data_changed = true;
          break;
        }
        else
        {
          // The TxtVers key must be the first key in the TXT record.
          if (parse_txt_vers(&item))
          {
            parsed_txt_vers = true;
            break;
          }
          else
          {
            return kTxtRecordParseError;
          }
        }
      }
    }
    // An additional TXT item with no '=' (key only)
    if (!found_equals && txt_len > 0)
    {
      item.key_len = txt_len;
      item.value = NULL;
      item.value_len = 0;
      if (parse_txt_item(&item, db, &keys_found))
        data_changed = true;
    }
    cur_ptr += txt_len;
  }

  if (ALL_TXT_KEYS_FOUND(keys_found))
  {
    return (data_changed ? kTxtRecordParseOkDataChanged : kTxtRecordParseOkNoDataChanged);
  }
  else
  {
    return kTxtRecordParseError;
  }
}

bool parse_txt_vers(const TxtRecordItemRef* item)
{
  if (item->key_len != sizeof(E133_TXT_VERS_KEY) - 1)
    return false;

  if (memcmp(item->key, E133_TXT_VERS_KEY, sizeof(E133_TXT_VERS_KEY) - 1) != 0)
    return false;

  if (binary_atoi(item->value, item->value_len) != E133_DNSSD_TXTVERS)
    return false;

  return true;
}

bool parse_txt_item(const TxtRecordItemRef* item, DiscoveredBroker* db, txt_keys_found_mask_t* found_mask)
{
  // E133Scope
  if (item->key_len == sizeof(E133_TXT_SCOPE_KEY) - 1 &&
      memcmp(item->key, E133_TXT_SCOPE_KEY, sizeof(E133_TXT_SCOPE_KEY) - 1) == 0)
  {
    return parse_e133_scope_item(item, db, found_mask);
  }
  // E133Vers
  else if (item->key_len == sizeof(E133_TXT_E133VERS_KEY) - 1 &&
           memcmp(item->key, E133_TXT_E133VERS_KEY, sizeof(E133_TXT_E133VERS_KEY) - 1) == 0)
  {
    return parse_e133_vers_item(item, db, found_mask);
  }
  // CID
  else if (item->key_len == sizeof(E133_TXT_CID_KEY) - 1 &&
           memcmp(item->key, E133_TXT_CID_KEY, sizeof(E133_TXT_CID_KEY) - 1) == 0)
  {
    return parse_cid_item(item, db, found_mask);
  }
  // UID
  else if (item->key_len == sizeof(E133_TXT_UID_KEY) - 1 &&
           memcmp(item->key, E133_TXT_UID_KEY, sizeof(E133_TXT_UID_KEY) - 1) == 0)
  {
    return parse_uid_item(item, db, found_mask);
  }
  // Model
  else if (item->key_len == sizeof(E133_TXT_MODEL_KEY) - 1 &&
           memcmp(item->key, E133_TXT_MODEL_KEY, sizeof(E133_TXT_MODEL_KEY) - 1) == 0)
  {
    return parse_model_item(item, db, found_mask);
  }
  // Manufacturer
  else if (item->key_len == sizeof(E133_TXT_MANUFACTURER_KEY) - 1 &&
           memcmp(item->key, E133_TXT_MANUFACTURER_KEY, sizeof(E133_TXT_MANUFACTURER_KEY) - 1) == 0)
  {
    return parse_manufacturer_item(item, db, found_mask);
  }
  // Additional/unknown
  else
  {
    return discovered_broker_add_binary_txt_record_item(db, item->key, item->key_len, item->value, item->value_len);
  }
}

bool parse_e133_scope_item(const TxtRecordItemRef* item, DiscoveredBroker* db, txt_keys_found_mask_t* found_mask)
{
  if (item->value_len > 0 && item->value_len <= E133_SCOPE_STRING_PADDED_LENGTH - 1)
  {
    *found_mask |= TXT_KEY_E133SCOPE_FOUND_MASK;
    if (strlen(db->scope) != item->value_len || memcmp(db->scope, item->value, item->value_len) != 0)
    {
      memcpy(db->scope, item->value, item->value_len);
      db->scope[item->value_len] = '\0';
      return true;
    }
  }
  return false;
}

bool parse_e133_vers_item(const TxtRecordItemRef* item, DiscoveredBroker* db, txt_keys_found_mask_t* found_mask)
{
  int e133_vers = binary_atoi(item->value, item->value_len);
  if (e133_vers != 0)
  {
    *found_mask |= TXT_KEY_E133VERS_FOUND_MASK;
    if (e133_vers != db->e133_version)
    {
      db->e133_version = e133_vers;
      return true;
    }
  }
  return false;
}

bool parse_cid_item(const TxtRecordItemRef* item, DiscoveredBroker* db, txt_keys_found_mask_t* found_mask)
{
  if (item->value_len >= 32 && item->value_len < ETCPAL_UUID_STRING_BYTES)
  {
    char cid_str[ETCPAL_UUID_STRING_BYTES];
    memcpy(cid_str, item->value, item->value_len);
    cid_str[item->value_len] = '\0';

    EtcPalUuid cid;
    if (etcpal_string_to_uuid(cid_str, &cid))
    {
      *found_mask |= TXT_KEY_CID_FOUND_MASK;
      if (ETCPAL_UUID_CMP(&cid, &db->cid) != 0)
      {
        db->cid = cid;
        return true;
      }
    }
  }
  return false;
}

bool parse_uid_item(const TxtRecordItemRef* item, DiscoveredBroker* db, txt_keys_found_mask_t* found_mask)
{
  if (item->value_len >= 12 && item->value_len < RDM_UID_STRING_BYTES)
  {
    char uid_str[RDM_UID_STRING_BYTES];
    memcpy(uid_str, item->value, item->value_len);
    uid_str[item->value_len] = '\0';

    RdmUid uid;
    if (rdm_string_to_uid(uid_str, &uid))
    {
      *found_mask |= TXT_KEY_UID_FOUND_MASK;
      if (!RDM_UID_EQUAL(&uid, &db->uid))
      {
        db->uid = uid;
        return true;
      }
    }
  }
  return false;
}

bool parse_model_item(const TxtRecordItemRef* item, DiscoveredBroker* db, txt_keys_found_mask_t* found_mask)
{
  if (item->value_len > 0 && item->value_len < E133_MODEL_STRING_PADDED_LENGTH)
  {
    *found_mask |= TXT_KEY_MODEL_FOUND_MASK;
    if (strlen(db->model) != item->value_len || memcmp(db->model, item->value, item->value_len) != 0)
    {
      memcpy(db->model, item->value, item->value_len);
      db->model[item->value_len] = '\0';
      return true;
    }
  }
  return false;
}

bool parse_manufacturer_item(const TxtRecordItemRef* item, DiscoveredBroker* db, txt_keys_found_mask_t* found_mask)
{
  if (item->value_len > 0 && item->value_len < E133_MANUFACTURER_STRING_PADDED_LENGTH)
  {
    *found_mask |= TXT_KEY_MANUF_FOUND_MASK;
    if (strlen(db->manufacturer) != item->value_len || memcmp(db->manufacturer, item->value, item->value_len) != 0)
    {
      memcpy(db->manufacturer, item->value, item->value_len);
      db->manufacturer[item->value_len] = '\0';
      return true;
    }
  }
  return false;
}

// Some simplifications over a normal atoi() for our purposes:
// The number is assumed to start at the beginning of the string
// We give up immediately if the number is over 9 places
int binary_atoi(const uint8_t* ascii_val, uint8_t ascii_val_len)
{
  int res = 0;

  if (ascii_val_len > 9)
    return 0;

  int place_multiplier = 1;
  for (const uint8_t* ptr = ascii_val; ptr < ascii_val + ascii_val_len; ++ptr)
  {
    if (isdigit(*ptr))
    {
      res += ((*ptr - 0x30) * place_multiplier);
      place_multiplier *= 10;
    }
    else
    {
      return res;
    }
  }
  return res;
}

bool get_domain_name_label(const uint8_t* buf_begin, const uint8_t* name_ptr, DomainNameLabel* label)
{
  const uint8_t* next_length_offset = NULL;
  if (!label->label)
    next_length_offset = name_ptr;
  else
    next_length_offset = label->label + label->length;

  if (!next_length_offset)
    return false;

  if (*next_length_offset & DNS_NAME_POINTER_MASK)
    next_length_offset = buf_begin + (etcpal_unpack_u16b(next_length_offset) & 0x3fff);
  if (*next_length_offset == 0 || *next_length_offset > 63)
    return false;
  label->length = *next_length_offset;
  label->label = next_length_offset + 1;
  return true;
}

bool is_rdmnet_service_type_and_domain(const uint8_t* buf_begin, DomainNameLabel* last_non_service_label)
{
  DomainNameLabel label = *last_non_service_label;

  // Compare the service type (e.g. _rdmnet)
  if (!get_domain_name_label(buf_begin, NULL, &label) || (label.length != (sizeof("_rdmnet") - 1)) ||
      memcmp(label.label, "_rdmnet", sizeof("_rdmnet") - 1) != 0)
  {
    return false;
  }

  // Compare the service protocol (e.g. _tcp)
  if (!get_domain_name_label(buf_begin, NULL, &label) || (label.length != (sizeof("_tcp") - 1)) ||
      memcmp(label.label, "_tcp", sizeof("_tcp") - 1) != 0)
  {
    return false;
  }

  // Compare the search domain (e.g. local)
  if (!get_domain_name_label(buf_begin, NULL, &label) || (label.length != (sizeof("local") - 1)) ||
      memcmp(label.label, "local", sizeof("local") - 1) != 0)
  {
    return false;
  }

  if (get_domain_name_label(buf_begin, NULL, &label))
    return false;

  return true;
}
