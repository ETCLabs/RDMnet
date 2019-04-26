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

/*! \file rdmnet/version.h
 *  \brief Provides the current version of the RDMnet library and executables.
 *  \author Sam Kearney
 */
#ifndef _RDMNET_VERSION_H_
#define _RDMNET_VERSION_H_

/* clang-format off */

/*! \defgroup rdmnet_core_lib RDMnet Core Library
 *  \brief Implementation of the core functions of RDMnet.
 *
 *  This includes discovery, connections, and message packing and unpacking.
 *
 *  @{
 */

/*! \name RDMnet version numbers
 *  @{
 */
#define RDMNET_VERSION_MAJOR 0 /*!< The major version. */
#define RDMNET_VERSION_MINOR 2 /*!< The minor version. */
#define RDMNET_VERSION_PATCH 0 /*!< The patch version. */
#define RDMNET_VERSION_BUILD 6 /*!< The build number. */
/*!@}*/

/*! \name RDMnet version strings
 *  @{
 */
#define RDMNET_VERSION_STRING "0.2.0.6"
#define RDMNET_VERSION_DATESTR "2019-04-26"
#define RDMNET_VERSION_COPYRIGHT "Copyright 2019 ETC Inc."
#define RDMNET_VERSION_PRODUCTNAME "RDMnet"
/*!@}*/

/*!@}*/

#endif /* _RDMNET_VERSION_H_ */
