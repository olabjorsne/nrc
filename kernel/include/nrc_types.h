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


#ifndef _NRC_TYPES_H_
#define _NRC_TYPES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NRC_R_OK                 (0)
#define NRC_R_ERROR              (-1)
#define NRC_R_TIMEOUT            (-2)
#define NRC_R_NOT_SUPPORTED      (-3)
#define NRC_R_INVALID_IN_PARAM   (-4)
#define NRC_R_NOT_FOUND          (-5)
#define NRC_R_OUT_OF_MEM         (-6)
#define NRC_R_INVALID_STATE      (-7)

#ifndef FALSE
#define FALSE   (0)
#endif
#ifndef TRUE
#define TRUE    (1)
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#define S8_MAX_VALUE  (0x7F)
#define S32_MAX_VALUE (0x7FFFFFFF)
#define U32_MAX_VALUE (0xFFFFFFFF)

typedef signed char         s8_t;
typedef signed short        s16_t;
typedef signed int          s32_t;
typedef signed long long    s64_t;

typedef unsigned char       u8_t;
typedef unsigned short      u16_t;
typedef unsigned int        u32_t;
typedef unsigned long long  u64_t;

typedef u32_t               bool_t;

#ifdef __cplusplus
}
#endif

#endif /* _NRC_TYPES_H_ */