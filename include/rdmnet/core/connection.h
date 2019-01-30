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

/*! \file rdmnet/core/connection.h
 *  \brief RDMnet Connection API definitions
 *
 *  Functions and definitions for the \ref rdmnet_conn "RDMnet Connection API" are contained in this
 *  header.
 *
 *  \author Sam Kearney
 */
#ifndef _RDMNET_CONNECTION_H_
#define _RDMNET_CONNECTION_H_

#include <stddef.h>
#include "lwpa/bool.h"
#include "lwpa/log.h"
#include "lwpa/error.h"
#include "lwpa/inet.h"
#include "lwpa/socket.h"
#include "rdmnet/core/message.h"

/*! \defgroup rdmnet_conn Connection
 *  \ingroup rdmnet_core_lib
 *  \brief Handle a connection between a Client and a %Broker in RDMnet.
 *
 *  In E1.33, the behavior of this module is dictated by the %Broker Protocol
 *  (&sect; 6).
 *
 *  Basic functionality for an RDMnet Client: Initialize the library using rdmnet_init(). Create a
 *  new connection using rdmnet_new_connection(). Connect to a %Broker using rdmnet_connect().
 *  Depending on the value of #RDMNET_USE_TICK_THREAD, may need to call rdmnet_tick() at regular
 *  intervals. Send data over the %Broker connection using rdmnet_send(), and receive data over the
 *  %Broker connection using rdmnet_recv().
 *
 *  @{
 */

/*! An identifier for the type of data contained in an RdmnetData structure. */
typedef enum
{
  kRDMnetNoData,          /*!< This RdmnetData contains no data. */
  kRDMnetDataTypeCode,    /*!< This RdmnetData contains a status code. */
  kRDMnetDataTypeMessage, /*!< This RdmnetData contains a message. */
  kRDMnetDataTypeAddress  /*!< This RdmnetData contains a network address. */
} rdmnet_data_t;

/*! Holds additional data received from API functions. */
typedef struct RdmnetData
{
  /*! The data type contained by this structure. This member can be inspected directly or you can
   *  use the helper macros defined below. */
  rdmnet_data_t type;
  /*! A union containing the different types of possible data. */
  union
  {
    uint32_t code;
    RdmnetMessage msg;
    LwpaSockaddr addr;
  } data;
} RdmnetData;

/*! \brief Determine if an RdmnetData structure contains no data.
 *  \param rdmnetdataptr Pointer to RdmnetData to inspect.
 *  \return true (rdmnetdataptr contains no data) or false (rdmnetdataptr contains a data value
 *          other than no data). */
#define rdmnet_data_is_nodata(rdmnetdataptr) ((rdmnetdataptr)->type == kRDMnetNoData)
/*! \brief Determine if an RdmnetData structure contains a status code.
 *  \param rdmnetdataptr Pointer to RdmnetData to inspect.
 *  \return true (rdmnetdataptr contains a status code) or false (rdmnetdataptr contains a data
 *          value other than a status code). */
#define rdmnet_data_is_code(rdmnetdataptr) ((rdmnetdataptr)->type == kRDMnetDataTypeCode)
/*! \brief Determine if an RdmnetData structure contains a message.
 *  \param rdmnetdataptr Pointer to RdmnetData to inspect.
 *  \return true (rdmnetdataptr contains a message) or false (rdmnetdataptr contains a data value
 *          other than a message). */
#define rdmnet_data_is_msg(rdmnetdataptr) ((rdmnetdataptr)->type == kRDMnetDataTypeMessage)
/*! \brief Determine if an RdmnetData structure contains a network address.
 *  \param rdmnetdataptr Pointer to RdmnetData to inspect.
 *  \return true (rdmnetdataptr contains a network address) or false (rdmnetdataptr contains a data
 *          value other than a network address).
 */
#define rdmnet_data_is_addr(rdmnetdataptr) ((rdmnetdataptr)->type == kRDMnetDataTypeAddress)
/*! \brief Get the status code from an RdmnetData structure.
 *  Use rdmnet_data_is_code() first to check if the status code is valid.
 *  \param rdmnetdataptr Pointer to RdmnetData from which to get the status code.
 *  \return The status code (uint32_t). */
#define rdmnet_data_code(rdmnetdataptr) ((rdmnetdataptr)->data.code)
/*! \brief Get the message from an RdmnetData structure.
 *  Use rdmnet_data_is_msg() first to check if the message is valid.
 *  \param rdmnetdataptr Pointer to RdmnetData from which to get the message.
 *  \return The message (RdmnetMessage *). */
#define rdmnet_data_msg(rdmnetdataptr) (&(rdmnetdataptr)->data.msg)
/*! \brief Get the network address from an RdmnetData structure.
 *  Use rdmnet_data_is_addr() first to check if the network address is valid.
 *  \param rdmnetdataptr Pointer to RdmnetData from which to get the network address.
 *  \return The network address (LwpaSockaddr *). */
#define rdmnet_data_addr(rdmnetdataptr) (&(rdmnetdataptr)->data.addr)

/*! An identifier for an RDMnet connection being polled. Used in rdmnet_poll() and
 *  rdmnet_connect_poll(). */
typedef struct RdmnetPoll
{
  /*! The connection handle. */
  int handle;
  /*! An error code for this connection, returned from the poll function. */
  lwpa_error_t err;
} RdmnetPoll;

#ifdef __cplusplus
extern "C" {
#endif

lwpa_error_t rdmnet_init(const LwpaLogParams *log_params);
void rdmnet_deinit();

int rdmnet_new_connection(const LwpaUuid *local_cid);
lwpa_error_t rdmnet_connect(int handle, const LwpaSockaddr *remote_addr, const ClientConnectMsg *connect_data,
                            RdmnetData *additional_data);
int rdmnet_connect_poll(RdmnetPoll *poll_arr, size_t poll_arr_size, int timeout_ms);
lwpa_error_t rdmnet_set_blocking(int handle, bool blocking);
lwpa_error_t rdmnet_attach_existing_socket(int handle, lwpa_socket_t sock, const LwpaSockaddr *remote_addr);
lwpa_error_t rdmnet_disconnect(int handle, bool send_disconnect_msg, rdmnet_disconnect_reason_t disconnect_reason);
lwpa_error_t rdmnet_destroy_connection(int handle);

int rdmnet_send(int handle, const uint8_t *data, size_t size);

lwpa_error_t rdmnet_start_message(int handle);
int rdmnet_send_partial_message(int handle, const uint8_t *data, size_t size);
lwpa_error_t rdmnet_end_message(int handle);

lwpa_error_t rdmnet_recv(int handle, RdmnetData *data);

int rdmnet_poll(RdmnetPoll *poll_arr, size_t poll_arr_size, int timeout_ms);

void rdmnet_tick();

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _RDMNET_CONNECTION_H_ */
