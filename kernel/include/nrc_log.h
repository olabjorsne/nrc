/**
* Copyright 2017 Tomas Frisberg & Ola Bjorsne
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http ://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#ifndef _NRC_LOG_H_
#define _NRC_LOG_H_

#include <stdint.h>
#include <stdarg.h>
#include "nrc_types.h"
#include "nrc_port.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NRC_LOG_NONE,
    NRC_LOG_ERROR, 
    NRC_LOG_WARNING,
    NRC_LOG_INFO,
    NRC_LOG_DEBUG,
    NRC_LOG_VERBOSE
} nrc_log_level;

#ifndef NRC_LOG_LEVEL
    #define NRC_LOG_LEVEL NRC_LOG_DEBUG 
#endif

void nrc_log_write(nrc_log_level level, const char* tag, const char* format, ...);

#define NRF_LOGE(tag, ...)  if (NRC_LOG_LEVEL >= NRC_LOG_ERROR)		{ nrc_log_write(NRC_LOG_ERROR,	 tag, __VA_ARGS__); }
#define NRC_LOGW(tag, ...)  if (NRC_LOG_LEVEL >= NRC_LOG_WARNING)	{ nrc_log_write(NRC_LOG_WARNING, tag, __VA_ARGS__); }
#define NRC_LOGI(tag, ...)  if (NRC_LOG_LEVEL >= NRC_LOG_INFO)		{ nrc_log_write(NRC_LOG_INFO,	 tag, __VA_ARGS__); }
#define NRC_LOGD(tag, ...)  if (NRC_LOG_LEVEL >= NRC_LOG_DEBUG)		{ nrc_log_write(NRC_LOG_DEBUG,   tag, __VA_ARGS__); }
#define NRC_LOGV(tag, ...)  if (NRC_LOG_LEVEL >= NRC_LOG_VERBOSE)	{ nrc_log_write(NRC_LOG_VERBOSE, tag, __VA_ARGS__); }

// TODO
//void nrc_log_level_set(const char* tag, nrc_log_level level);

#ifdef __cplusplus
}
#endif


#endif /* __NRC_LOG_H__ */
