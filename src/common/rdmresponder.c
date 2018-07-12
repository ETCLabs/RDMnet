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
#include "rdmnet/rdmresponder.h"

#include <string.h>
#include "lwpa_int.h"
#include "lwpa_pack.h"
#include "estardm.h"

/*************************** Function definitions ****************************/

/*! \brief Unpack an RDM command.
 *  \param[in] buffer The packed RDM command.
 *  \param[out] cmd The RDM command data that was unpacked from buffer.
 *  \return #LWPA_OK: Command unpacked successfully.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_PROTERR: Packed RDM command was invalid.\n
 */
lwpa_error_t rdmresp_unpack_command(const RdmBuffer *buffer, RdmCommand *cmd)
{
  const uint8_t *cur_ptr;

  if (!buffer || !cmd)
    return LWPA_INVALID;
  if (!rdm_validate_msg(buffer))
    return LWPA_PROTERR;

  cur_ptr = &buffer->data[RDM_OFFSET_DEST_MANUFACTURER];
  cmd->dest_uid.manu = upack_16b(cur_ptr);
  cur_ptr += 2;
  cmd->dest_uid.id = upack_32b(cur_ptr);
  cur_ptr += 4;
  cmd->src_uid.manu = upack_16b(cur_ptr);
  cur_ptr += 2;
  cmd->src_uid.id = upack_32b(cur_ptr);
  cur_ptr += 4;
  cmd->transaction_num = *cur_ptr++;
  cmd->port_id = *cur_ptr++;
  cur_ptr++; /* Message Count field is ignored */
  cmd->subdevice = upack_16b(cur_ptr);
  cur_ptr += 2;
  cmd->command_class = *cur_ptr++;
  cmd->param_id = upack_16b(cur_ptr);
  cur_ptr += 2;
  cmd->datalen = *cur_ptr++;
  memcpy(cmd->data, cur_ptr, cmd->datalen);
  return LWPA_OK;
}

/*! \brief Create a packed RDM response.
 *  \param[in] resp_data The data that will be used for this RDM response
 *                       packet.
 *  \param[out] buffer The buffer into which to pack this RDM response.
 *  \return #LWPA_OK: Response created successfully.\n
 *          #LWPA_INVALID: Invalid argument provided.\n
 *          #LWPA_MSGSIZE: The parameter data was too long.\n
 */
lwpa_error_t rdmresp_create_response(const RdmResponse *resp_data, RdmBuffer *buffer)
{
  uint8_t *cur_ptr;
  uint8_t rdm_length;

  /* Check for invalid parameters */
  if (!resp_data || !buffer)
    return LWPA_INVALID;
  if (resp_data->datalen > RDM_MAX_PDL)
    return LWPA_MSGSIZE;

  cur_ptr = buffer->data;
  rdm_length = resp_data->datalen + RDM_HEADER_SIZE;

  /* Pack the header and data into the buffer */
  *cur_ptr++ = E120_SC_RDM;
  *cur_ptr++ = E120_SC_SUB_MESSAGE;
  *cur_ptr++ = rdm_length;
  pack_16b(cur_ptr, resp_data->dest_uid.manu);
  cur_ptr += 2;
  pack_32b(cur_ptr, resp_data->dest_uid.id);
  cur_ptr += 4;
  pack_16b(cur_ptr, resp_data->src_uid.manu);
  cur_ptr += 2;
  pack_32b(cur_ptr, resp_data->src_uid.id);
  cur_ptr += 4;
  *cur_ptr++ = resp_data->transaction_num;
  *cur_ptr++ = resp_data->resp_type;
  *cur_ptr++ = resp_data->msg_count;
  pack_16b(cur_ptr, resp_data->subdevice);
  cur_ptr += 2;
  *cur_ptr++ = resp_data->command_class;
  pack_16b(cur_ptr, resp_data->param_id);
  cur_ptr += 2;
  *cur_ptr++ = resp_data->datalen;
  memcpy(cur_ptr, resp_data->data, resp_data->datalen);

  /* pack checksum and set packet length */
  rdm_pack_checksum(buffer->data, rdm_length);
  buffer->datalen = rdm_length + 2;
  return LWPA_OK;
}
