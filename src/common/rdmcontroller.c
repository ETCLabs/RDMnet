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
#include "rdmnet/rdmcontroller.h"

#include <string.h>
#include "lwpa_pack.h"
#include "estardm.h"

/*************************** Function definitions ****************************/

/*! \brief Create a packed RDM command.
 *  \param[in] cmd_data The data that will be used for this RDM command packet.
 *  \param[out] buffer The buffer into which to pack this RDM command.
 *  \return #LWPA_OK: Command created successfully.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_MSGSIZE: The parameter data was too long.\n
 */
lwpa_error_t rdmctl_create_command(const RdmCommand *cmd_data, RdmBuffer *buffer)
{
  uint8_t *cur_ptr;
  uint8_t rdm_length;

  if (!cmd_data || !buffer)
    return LWPA_INVALID;
  if (cmd_data->datalen > RDM_MAX_PDL)
    return LWPA_MSGSIZE;

  cur_ptr = buffer->data;
  rdm_length = cmd_data->datalen + RDM_HEADER_SIZE;

  *cur_ptr++ = E120_SC_RDM;
  *cur_ptr++ = E120_SC_SUB_MESSAGE;
  *cur_ptr++ = rdm_length;
  pack_16b(cur_ptr, cmd_data->dest_uid.manu);
  cur_ptr += 2;
  pack_32b(cur_ptr, cmd_data->dest_uid.id);
  cur_ptr += 4;
  pack_16b(cur_ptr, cmd_data->src_uid.manu);
  cur_ptr += 2;
  pack_32b(cur_ptr, cmd_data->src_uid.id);
  cur_ptr += 4;
  *cur_ptr++ = cmd_data->transaction_num;
  *cur_ptr++ = cmd_data->port_id;
  *cur_ptr++ = 0;
  pack_16b(cur_ptr, cmd_data->subdevice);
  cur_ptr += 2;
  *cur_ptr++ = cmd_data->command_class;
  pack_16b(cur_ptr, cmd_data->param_id);
  cur_ptr += 2;
  *cur_ptr++ = cmd_data->datalen;
  memcpy(cur_ptr, cmd_data->data, cmd_data->datalen);

  /* pack checksum and set packet length */
  rdm_pack_checksum(buffer->data, rdm_length);
  buffer->datalen = rdm_length + 2;
  return LWPA_OK;
}

/*! \brief Unpack an RDM repsonse.
 *  \param[in] buffer The packed RDM response.
 *  \param[out] resp The RDM response data that was unpacked from buffer.
 *  \return #LWPA_OK: Response unpacked successfully.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_PROTERR: Packed RDM response was invalid.\n
 */
lwpa_error_t rdmctl_unpack_response(const RdmBuffer *buffer, RdmResponse *resp)
{
  if (!buffer || !resp)
    return LWPA_INVALID;
  if (!rdm_validate_msg(buffer))
    return LWPA_PROTERR;

  const uint8_t *cur_ptr = &buffer->data[RDM_OFFSET_DEST_MANUFACTURER];
  resp->dest_uid.manu = upack_16b(cur_ptr);
  cur_ptr += 2;
  resp->dest_uid.id = upack_32b(cur_ptr);
  cur_ptr += 4;
  resp->src_uid.manu = upack_16b(cur_ptr);
  cur_ptr += 2;
  resp->src_uid.id = upack_32b(cur_ptr);
  cur_ptr += 4;
  resp->transaction_num = *cur_ptr++;
  resp->resp_type = *cur_ptr++;
  resp->msg_count = *cur_ptr++;
  resp->subdevice = upack_16b(cur_ptr);
  cur_ptr += 2;
  resp->command_class = *cur_ptr++;
  resp->param_id = upack_16b(cur_ptr);
  cur_ptr += 2;
  resp->datalen = *cur_ptr++;
  memcpy(resp->data, cur_ptr, resp->datalen);
  return LWPA_OK;
}
