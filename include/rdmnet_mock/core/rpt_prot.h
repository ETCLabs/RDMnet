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

/* rdmnet_mock/core/rpt_prot.h
 * Mocking the functions of rdmnet/core/rpt_prot.h
 */
#ifndef _RDMNET_MOCK_CORE_RPT_PROT_H_
#define _RDMNET_MOCK_CORE_RPT_PROT_H_

#include "rdmnet/core/rpt_prot.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(size_t, bufsize_rpt_request, const RdmBuffer*);
DECLARE_FAKE_VALUE_FUNC(size_t, bufsize_rpt_status, const RptStatusMsg*);
DECLARE_FAKE_VALUE_FUNC(size_t, bufsize_rpt_notification, const RdmBuffer*, size_t);
DECLARE_FAKE_VALUE_FUNC(size_t, pack_rpt_request, uint8_t*, size_t, const EtcPalUuid*, const RptHeader*,
                        const RdmBuffer*);
DECLARE_FAKE_VALUE_FUNC(size_t, pack_rpt_status, uint8_t*, size_t, const EtcPalUuid*, const RptHeader*,
                        const RptStatusMsg*);
DECLARE_FAKE_VALUE_FUNC(size_t, pack_rpt_notification, uint8_t*, size_t, const EtcPalUuid*, const RptHeader*,
                        const RdmBuffer*, size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, send_rpt_request, rdmnet_conn_t, const EtcPalUuid*, const RptHeader*,
                        const RdmBuffer*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, send_rpt_status, rdmnet_conn_t, const EtcPalUuid*, const RptHeader*,
                        const RptStatusMsg*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, send_rpt_notification, rdmnet_conn_t, const EtcPalUuid*, const RptHeader*,
                        const RdmBuffer*, size_t);

#define RDMNET_CORE_RPT_PROT_DO_FOR_ALL_FAKES(operation) \
  operation(bufsize_rpt_request);                        \
  operation(bufsize_rpt_status);                         \
  operation(bufsize_rpt_notification);                   \
  operation(pack_rpt_request);                           \
  operation(pack_rpt_status);                            \
  operation(pack_rpt_notification);                      \
  operation(send_rpt_request);                           \
  operation(send_rpt_status);                            \
  operation(send_rpt_notification)

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_MOCK_CORE_RPT_PROT_H_ */
