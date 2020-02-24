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

#include "rdmnet/llrp.h"

/*************************** Function definitions ****************************/

/*!
 * \brief Get a string description of an LLRP component type.
 *
 * LLRP component types describe the type of RDMnet component with which an LLRP target is
 * associated.
 *
 * \param type Type code.
 * \return String, or NULL if type is invalid.
 */
const char* llrp_component_type_to_string(llrp_component_t type)
{
  ETCPAL_UNUSED_ARG(type);
  return NULL;
}

/*!
 * \brief Save the data in a received LLRP RDM command for later use with API functions from a
 *        different context.
 *
 * RDMnet message types delivered to RDMnet callback functions do not own their data; if not
 * responding to a command synchronously, the command must be saved before exiting the callback.
 * See \ref handling_rdm_commands for more information.
 *
 * \param[in] command Command to save.
 * \param[out] saved_command Command with copied and saved data.
 * \return #kEtcPalErrOk: Command saved successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 */
etcpal_error_t llrp_save_rdm_command(const LlrpRdmCommand* command, LlrpSavedRdmCommand* saved_command)
{
  ETCPAL_UNUSED_ARG(command);
  ETCPAL_UNUSED_ARG(saved_command);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Save the data in a received LLRP RDM response for later use from a different context.
 *
 * RDMnet message types delivered to RDMnet callback functions do not own their data; if
 * referencing an RDM response after the callback has returned is desired, the data must be saved.
 *
 * \param[in] response Response to save.
 * \param[out] saved_response Response with copied and saved data.
 * \return #kEtcPalErrOk: Response saved successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 */
etcpal_error_t llrp_save_rdm_response(const LlrpRdmResponse* response, LlrpSavedRdmResponse* saved_response)
{
  ETCPAL_UNUSED_ARG(response);
  ETCPAL_UNUSED_ARG(saved_response);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Copy the data from a saved LLRP RDM response to a different saved LLRP RDM response.
 *
 * \param[in] saved_resp_old Saved response to copy from.
 * \param[out] saved_resp_new Saved response to copy to.
 * \return #kEtcPalErrOk: Response copied successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 */
etcpal_error_t llrp_copy_saved_rdm_response(const LlrpSavedRdmResponse* saved_resp_old,
                                            LlrpSavedRdmResponse* saved_resp_new)
{
  ETCPAL_UNUSED_ARG(saved_resp_old);
  ETCPAL_UNUSED_ARG(saved_resp_new);
  return kEtcPalErrNotImpl;
}
