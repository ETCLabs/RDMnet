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

/*! \file rdmnet/core/util.h
 *  \brief Utilities used throughout the RDMnet library.
 *  \author Sam Kearney
 */
#ifndef _RDMNET_CORE_UTIL_H_
#define _RDMNET_CORE_UTIL_H_

#include <stddef.h>

/* Suppress deprecated function warnings on Windows/MSVC. This is mostly used in situations where
 * Microsoft warns us that a function like strncpy() could be unsafe, but we want to be portable
 * and have made sure that we're using it in a safe way (e.g. by manually inserting null
 * terminators). */
#ifdef _MSC_VER

#define RDMNET_MSVC_NO_DEP_WRN __pragma(warning(suppress : 4996))

#define RDMNET_MSVC_BEGIN_NO_DEP_WARNINGS() __pragma(warning(push)) __pragma(warning(disable : 4996))
#define RDMNET_MSVC_END_NO_DEP_WARNINGS() __pragma(warning(pop))

#else /* _MSC_VER */

#define RDMNET_MSVC_NO_DEP_WRN
#define RDMNET_MSVC_BEGIN_NO_DEP_WARNINGS()
#define RDMNET_MSVC_END_NO_DEP_WARNINGS()

#endif /* _MSC_VER */

#ifdef __cplusplus
extern "C" {
#endif

char* rdmnet_safe_strncpy(char* destination, const char* source, size_t num);

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_CORE_UTIL_H_ */
