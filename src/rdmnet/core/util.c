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

#include "rdmnet/core/util.h"

#include <string.h>

/*! \brief An implementation of the C library function strncpy() which truncates safely.
 *
 *  Always puts a null character at destination[num - 1].
 *  \param[out] destination Pointer to the destination array where the content is to be copied.
 *  \param[in] source C string to be copied.
 *  \param[in] num Maximum number of characters to be copied from source.
 *  \return Destination.
 */
char *rdmnet_safe_strncpy(char *destination, const char *source, size_t num)
{
  if (!destination || num == 0)
    return NULL;

  RDMNET_MSVC_NO_DEP_WRN strncpy(destination, source, num);
  destination[num - 1] = '\0';
  return destination;
}
