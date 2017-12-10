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


#ifndef _NRC_PORT_H_
#define _NRC_PORT_H_

#include "nrc_types.h"

#define NRC_PORT_RES_OK                     (0)
#define NRC_PORT_RES_ERROR                  (-1)
#define NRC_PORT_RES_TIMEOUT                (-2)
#define NRC_PORT_RES_NOT_SUPPORTED          (-3)
#define NRC_PORT_RES_INVALID_IN_PARAM       (-4)
#define NRC_PORT_RES_NOT_FOUND              (-5)
#define NRC_PORT_RES_OUT_OF_MEM             (-6)
#define NRC_PORT_RES_INVALID_STATE          (-7)
#define NRC_PORT_RES_RESOURCE_UNAVAILABLE   (-8)

#ifdef __cplusplus
extern "C" {
#endif

enum nrc_port_thread_prio {
    NRC_PORT_THREAD_PRIO_CRITICAL = 1,  // For time critical drivers
    NRC_PORT_THREAD_PRIO_HIGH,          // For kernel
    NRC_PORT_THREAD_PRIO_NORMAL,        // For application
    NRC_PORT_THREAD_PRIO_LOW            // For background tasks
};

typedef void* nrc_port_thread_t;
typedef void* nrc_port_sema_t;
typedef void* nrc_port_mutex_t;
typedef void* nrc_port_timer_t;

typedef void(*nrc_port_thread_fcn_t)(void);
typedef void(*nrc_port_timeout_fcn_t)(nrc_port_timer_t timer_id, void *tag);


s32_t nrc_port_init(void);

/**
 * Memory and Heap
 */
u8_t* nrc_port_heap_alloc(u32_t size);
void nrc_port_heap_free(void *buf);

u8_t* nrc_port_heap_fast_alloc(u32_t size);
void nrc_port_heap_fast_free(void *buf);

/**
 * Thread
 */
s32_t nrc_port_thread_init(
    enum nrc_port_thread_prio   priority,
    u32_t                       stack_size,
    nrc_port_thread_fcn_t       thread_fcn,
    nrc_port_thread_t           *thread_id);

s32_t nrc_port_thread_start(nrc_port_thread_t thread_id);

/**
 * Mutex
 */
s32_t nrc_port_mutex_init(nrc_port_mutex_t *mutex);
s32_t nrc_port_mutex_lock(nrc_port_mutex_t mutex, u32_t timeout);
s32_t nrc_port_mutex_unlock(nrc_port_mutex_t mutex);

/**
 * Semaphore
 */
s32_t nrc_port_sema_init(u32_t count, nrc_port_sema_t *sema);
s32_t nrc_port_sema_signal(nrc_port_sema_t sema);
s32_t nrc_port_sema_wait(nrc_port_sema_t sema, u32_t timeout);

/**
 * Timer
 */
s32_t nrc_port_timer_after(u32_t timeout_ms, void *tag, nrc_port_timeout_fcn_t timeout_fcn, nrc_port_timer_t *timer);
s32_t nrc_port_timer_cancel(nrc_port_timer_t timer);
u64_t nrc_port_timer_get_time_ms(void);
u32_t nrc_port_timer_get_res_ms(void);

/**
 * IRQ Disable/Enable
 * 
 * Only for interrupts
 */
s32_t nrc_port_irq_disable(void);
s32_t nrc_port_irq_enable(void);

/**
 * Used by log macros
 */
s32_t nrc_port_vprintf(const char * format, va_list argptr);
u32_t nrc_port_timestamp_in_ms(void);

s32_t nrc_port_get_config(u8_t **config, u32_t *size);

#ifdef __cplusplus
}
#endif

#endif