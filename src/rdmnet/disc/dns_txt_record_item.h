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

#ifndef RDMNET_DISC_DNS_TXT_RECORD_ITEM_H_
#define RDMNET_DISC_DNS_TXT_RECORD_ITEM_H_

#include <stdint.h>

#define MAX_TXT_RECORD_ITEMS_PER_BROKER 5
#define DNS_TXT_RECORD_COMPONENT_MAX_LENGTH 256

typedef struct DnsTxtRecordItemInternal
{
  char    key[DNS_TXT_RECORD_COMPONENT_MAX_LENGTH];
  uint8_t value[DNS_TXT_RECORD_COMPONENT_MAX_LENGTH];
  uint8_t value_len;
} DnsTxtRecordItemInternal;

#endif /* RDMNET_DISC_DNS_TXT_RECORD_ITEM_H_ */
