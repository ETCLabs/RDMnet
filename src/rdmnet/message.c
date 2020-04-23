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

#include "rdmnet/message.h"

#include "rdmnet/private/opts.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#endif

/*************************** Function definitions ****************************/

/*!
 * \brief Save the data in a received RDM command for later use with API functions from a different
 *        context.
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
etcpal_error_t rdmnet_save_rdm_command(const RdmnetRdmCommand* command, RdmnetSavedRdmCommand* saved_command)
{
  ETCPAL_UNUSED_ARG(command);
  ETCPAL_UNUSED_ARG(saved_command);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Save the data in a received RDM response for later use from a different context.
 *
 * RDMnet message types delivered to RDMnet callback functions do not own their data; if
 * referencing an RDM response after the callback has returned is desired, the data must be saved.
 *
 * The length of data contained in an RDMnet RDM response is technically unbounded (ACK_OVERFLOW
 * responses are recombined by the library), so this function does heap allocation and gives the
 * resulting RdmnetSavedRdmResponse struct an owned rdm_data pointer.
 *
 * \param[in] response Response to save.
 * \param[out] saved_response Response with copied and saved data.
 * \return #kEtcPalErrOk: Response saved successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotImpl: RDMnet was compiled with #RDMNET_DYNAMIC_MEM set to 0.
 * \return #kEtcPalErrNoMem: Couldn't allocate memory for response data.
 */
etcpal_error_t rdmnet_save_rdm_response(const RdmnetRdmResponse* response, RdmnetSavedRdmResponse* saved_response)
{
  ETCPAL_UNUSED_ARG(response);
  ETCPAL_UNUSED_ARG(saved_response);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Append more data to a SavedRdmResponse's parameter data.
 *
 * This is useful after having previously saved an RdmnetRdmResponse with the more_coming flag set
 * to true.
 *
 * \param[in] new_response The new response that has just been received.
 * \param[in,out] previously_saved_response The previously saved response to which to append data.
 * \return #kEtcPalErrOk: Data appended successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotImpl: RDMnet was compiled with #RDMNET_DYNAMIC_MEM set to 0.
 * \return #kEtcPalErrNoMem: Couldn't allocate memory for new response data.
 */
etcpal_error_t rdmnet_append_to_saved_rdm_response(const RdmnetRdmResponse* new_response,
                                                   RdmnetSavedRdmResponse* previously_saved_response)
{
  ETCPAL_UNUSED_ARG(new_response);
  ETCPAL_UNUSED_ARG(previously_saved_response);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Save the data in a received RPT status message for later use from a different context.
 *
 * RDMnet message types delivered to RDMnet callback functions do not own their data; if
 * referencing an RPT status message after the callback has returned is desired, the data must be
 * saved. This only applies to the optional RPT status string that accompanies a status message; an
 * application that doesn't care about the status string need not use this function.
 *
 * This function does heap allocation to copy the status string and gives the resulting
 * RdmnetSavedRptStatus an owned status_string pointer.
 *
 * \param[in] status Status message to save.
 * \param[out] saved_status Status with copied and saved data.
 * \return #kEtcPalErrOk: Status message saved successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotImpl: RDMnet was compiled with #RDMNET_DYNAMIC_MEM set to 0.
 * \return #kEtcPalErrNoMem: Couldn't allocate memory for status string.
 */
etcpal_error_t rdmnet_save_rpt_status(const RdmnetRptStatus* status, RdmnetSavedRptStatus* saved_status)
{
  ETCPAL_UNUSED_ARG(status);
  ETCPAL_UNUSED_ARG(saved_status);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Copy the data from a saved RDM response to a different saved RDM response.
 *
 * The length of data contained in an RDMnet RDM response is technically unbounded (ACK_OVERFLOW
 * responses are recombined by the library), so this function does heap allocation and gives the
 * resulting RdmnetSavedRdmResponse struct an owned rdm_data pointer.
 *
 * \param[in] saved_resp_old Saved response to copy from.
 * \param[out] saved_resp_new Saved response to copy to.
 * \return #kEtcPalErrOk: Response copied successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotImpl: RDMnet was compiled with #RDMNET_DYNAMIC_MEM set to 0.
 * \return #kEtcPalErrNoMem: Couldn't allocate memory for response data.
 */
etcpal_error_t rdmnet_copy_saved_rdm_response(const RdmnetSavedRdmResponse* saved_resp_old,
                                              RdmnetSavedRdmResponse* saved_resp_new)
{
  ETCPAL_UNUSED_ARG(saved_resp_old);
  ETCPAL_UNUSED_ARG(saved_resp_new);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Copy the data from a saved RPT status message to a different saved RPT status message.
 *
 * This function does heap allocation to copy the status string and gives the resulting
 * RdmnetSavedRptStatus an owned status_string pointer.
 *
 * \param[in] saved_status_old Saved status message to copy from.
 * \param[out] saved_status_new Saved status message to copy to.
 * \return #kEtcPalErrOk: Status message copied successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotImpl: RDMnet was compiled with #RDMNET_DYNAMIC_MEM set to 0.
 * \return #kEtcPalErrNoMem: Couldn't allocate memory for status string.
 */
etcpal_error_t rdmnet_copy_saved_rpt_status(const RdmnetSavedRptStatus* saved_status_old,
                                            RdmnetSavedRptStatus* saved_status_new)
{
  ETCPAL_UNUSED_ARG(saved_status_old);
  ETCPAL_UNUSED_ARG(saved_status_new);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Free the memory owned by a saved RDM response.
 *
 * Frees the owned data members in the RdmnetSavedRdmResponse struct. Make sure to do this before
 * the struct goes out of scope.
 *
 * \param[in] saved_response Response for which to free resources.
 * \return #kEtcPalErrOk: Response freed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotImpl: RDMnet was compiled with #RDMNET_DYNAMIC_MEM set to 0.
 */
etcpal_error_t rdmnet_free_saved_rdm_response(RdmnetSavedRdmResponse* saved_response)
{
  ETCPAL_UNUSED_ARG(saved_response);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Free the memory owned by a saved RPT status message.
 *
 * Frees the owned data members in the RdmnetSavedRptStatus struct. Make sure to do this before the
 * struct goes out of scope.
 *
 * \param[in] saved_status Status message for which to free resources.
 * \return #kEtcPalErrOk: Status message freed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotImpl: RDMnet was compiled with #RDMNET_DYNAMIC_MEM set to 0.
 */
etcpal_error_t rdmnet_free_saved_rpt_status(RdmnetSavedRptStatus* saved_status)
{
  ETCPAL_UNUSED_ARG(saved_status);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Save the data in a received EPT data message for later use from a different context.
 *
 * RDMnet message types delivered to RDMnet callback functions do not own their data; if
 * referencing an EPT data message after the callback has returned is desired, the data must be
 * saved.
 *
 * This function does heap allocation and gives the resulting RdmnetSavedEptData struct an owned
 * data pointer.
 *
 * \param[in] data Data message to save.
 * \param[out] saved_data Message structure with copied and saved data.
 * \return #kEtcPalErrOk: Data saved successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotImpl: RDMnet was compiled with #RDMNET_DYNAMIC_MEM set to 0.
 * \return #kEtcPalErrNoMem: Couldn't allocate memory for data.
 */
etcpal_error_t rdmnet_save_ept_data(const RdmnetEptData* data, RdmnetSavedEptData* saved_data)
{
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(saved_data);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Save the data in a received EPT status message for later use from a different context.
 *
 * RDMnet message types delivered to RDMnet callback functions do not own their data; if
 * referencing an EPT status message after the callback has returned is desired, the data must be
 * saved. This only applies to the optional EPT status string that accompanies a status message; an
 * application that doesn't care about the status string need not use this function.
 *
 * This function does heap allocation to copy the status string and gives the resulting
 * RdmnetSavedEptStatus an owned status_string pointer.
 *
 * \param[in] status Status message to save.
 * \param[out] saved_status Status with copied and saved data.
 * \return #kEtcPalErrOk: Status message saved successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotImpl: RDMnet was compiled with #RDMNET_DYNAMIC_MEM set to 0.
 * \return #kEtcPalErrNoMem: Couldn't allocate memory for status string.
 */
etcpal_error_t rdmnet_save_ept_status(const RdmnetEptStatus* status, RdmnetSavedEptStatus* saved_status)
{
  ETCPAL_UNUSED_ARG(status);
  ETCPAL_UNUSED_ARG(saved_status);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Copy the data from a saved EPT data message to a different saved EPT data message.
 *
 * This function does heap allocation and gives the resulting RdmnetSavedEptData struct an owned
 * data pointer.
 *
 * \param[in] saved_data_old Saved data message to copy from.
 * \param[out] saved_data_new Saved data message to copy to.
 * \return #kEtcPalErrOk: Data copied successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotImpl: RDMnet was compiled with #RDMNET_DYNAMIC_MEM set to 0.
 * \return #kEtcPalErrNoMem: Couldn't allocate memory for data.
 */
etcpal_error_t rdmnet_copy_saved_ept_data(const RdmnetSavedEptData* saved_data_old, RdmnetSavedEptData* saved_data_new)
{
  ETCPAL_UNUSED_ARG(saved_data_old);
  ETCPAL_UNUSED_ARG(saved_data_new);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Copy the data from a saved EPT status message to a different saved EPT status message.
 *
 * This function does heap allocation to copy the status string and gives the resulting
 * RdmnetSavedEptStatus an owned status_string pointer.
 *
 * \param[in] saved_status_old Saved status message to copy from.
 * \param[out] saved_status_new Saved status message to copy to.
 * \return #kEtcPalErrOk: Status message copied successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotImpl: RDMnet was compiled with #RDMNET_DYNAMIC_MEM set to 0.
 * \return #kEtcPalErrNoMem: Couldn't allocate memory for status string.
 */
etcpal_error_t rdmnet_copy_saved_ept_status(const RdmnetSavedEptStatus* saved_status_old,
                                            RdmnetSavedEptStatus* saved_status_new)
{
  ETCPAL_UNUSED_ARG(saved_status_old);
  ETCPAL_UNUSED_ARG(saved_status_new);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Free the memory owned by a saved EPT data message.
 *
 * Frees the owned data members in the RdmnetSavedEptData struct. Make sure to do this before the
 * struct goes out of scope.
 *
 * \param[in] saved_data Data message for which to free resources.
 * \return #kEtcPalErrOk: Response freed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotImpl: RDMnet was compiled with #RDMNET_DYNAMIC_MEM set to 0.
 */
etcpal_error_t rdmnet_free_saved_ept_data(RdmnetSavedEptData* saved_data)
{
  ETCPAL_UNUSED_ARG(saved_data);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Free the memory owned by a saved EPT status message.
 *
 * Frees the owned data members in the RdmnetSavedEptStatus struct. Make sure to do this before the
 * struct goes out of scope.
 *
 * \param[in] saved_status Status message for which to free resources.
 * \return #kEtcPalErrOk: Status message freed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNotImpl: RDMnet was compiled with #RDMNET_DYNAMIC_MEM set to 0.
 */
etcpal_error_t rdmnet_free_saved_ept_status(RdmnetSavedEptStatus* saved_status)
{
  ETCPAL_UNUSED_ARG(saved_status);
  return kEtcPalErrNotImpl;
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
etcpal_error_t rdmnet_save_llrp_rdm_command(const LlrpRdmCommand* command, LlrpSavedRdmCommand* saved_command)
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
etcpal_error_t rdmnet_save_llrp_rdm_response(const LlrpRdmResponse* response, LlrpSavedRdmResponse* saved_response)
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
etcpal_error_t rdmnet_copy_saved_llrp_rdm_response(const LlrpSavedRdmResponse* saved_resp_old,
                                                   LlrpSavedRdmResponse* saved_resp_new)
{
  ETCPAL_UNUSED_ARG(saved_resp_old);
  ETCPAL_UNUSED_ARG(saved_resp_new);
  return kEtcPalErrNotImpl;
}
