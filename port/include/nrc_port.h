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

#ifdef __cplusplus
extern "C" {
#endif

#define NRC_PORT_ASSERT(x)                                      \
	do {                                                        \
		if (!(x)) {                                             \
            nrc_port_error(__FILE__, __LINE__);                 \
		}                                                       \
	} while ((0))


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

typedef void(*nrc_port_error_handler_t)(s8_t *file, u32_t line);


/** 
 * @brief Initialises the nrc_port component
 *
 * Must be called before any other function of the component is called.
 *
 * @return NRC_R_OK if successful.
 */
s32_t nrc_port_init(void);

/** 
 * @brief Registers node types supported by the port to node factory
 *
 * Must be called before nodes can be created using nrc_factory_create() function
 *
 * @return None
 */
void nrc_port_register_nodes(void);

/** 
 * @brief Allocate buffer from heap
 *
 * Mainly for memory allocated/freed at start/end of program execution.
 *
 * @param size Number of bytes to allocate
 *
 * @return Allocated buffer. NULL if out of memory.
 */
u8_t* nrc_port_heap_alloc(u32_t size);

/** 
 * @brief Free buffer from heap
 *
 * Free buffer previously allocated with nrc_port_heap_alloc.
 *
 * @param buf Buffer to free
 *
 * @return void
 */
void nrc_port_heap_free(void *buf);

/** 
 * @brief Allocate  fast buffer from heap
 *
 * Mainly for memory allocated/freed many times during program execution.
 * Faster and lower risk of fragementation but with risk of allocating more
 * memory than requested. Typically used for messages.
 *
 * @param size Number of bytes to allocate
 *
 * @return Allocated buffer. NULL if out of memory.
 */
u8_t* nrc_port_heap_fast_alloc(u32_t size);

/** 
 * @brief Free fast buffer to heap
 *
 * Free buffer previously allocated with nrc_port_heap_fast_alloc.
 *
 * @param buf Buffer to free
 *
 * @return Void
 */
void nrc_port_heap_fast_free(void *buf);

/** 
 * @brief Creates and initializes a thread
 *
 * Thread in suspended state until nrc_port_thread_start is called.
 *
 * @param priority Priority of the thread (lower number is higher prio)
 * @param stack_size Size of the thread stack in bytes.
 * @param thread_fcn Function that will be called when thread is started.
 * @param thread_id Output parameter where the thread identifier is stored.
 *
 * @return NRC_R_OK if call is successful.
 */
s32_t nrc_port_thread_init(
    enum nrc_port_thread_prio   priority,
    u32_t                       stack_size,
    nrc_port_thread_fcn_t       thread_fcn,
    nrc_port_thread_t           *thread_id);

/** 
 * @brief Start thread
 *
 * Start a thread previously created with nrc_port_thread_init.
 *
 * @param thread_id Thread identifier
 *
 * @return NRC_R_OK is successful
 */
s32_t nrc_port_thread_start(nrc_port_thread_t thread_id);

/** 
 * @brief Creates and initializes a mutex
 *
 * @param mutex Output parameter where the mutex identifier is stored.
 *
 * @return NRC_R_OK if call is successful.
 */
s32_t nrc_port_mutex_init(nrc_port_mutex_t *mutex);

/** 
 * @brief Locks the mutex
 *
 * Locks the mutex until the nrc_port_mutex_unlock is called.
 *
 * @param mutex Mutex to lock
 * @param timeout Maximum time in ms to wait for mutex to be released.
 * If set to 0, the call will not timeout and it is blocked until mutex is released.
 *
 * @return NRC_R_OK is successful. If timeout, NRC_R_TIMEOUT is returned.
 */
s32_t nrc_port_mutex_lock(nrc_port_mutex_t mutex, u32_t timeout);

/** 
 * @brief Unlocks the mutex
 *
 * Unlocks the mutex previously locked with nrc_port_mutex_lock.
 *
 * @param mutex Mutex to unlock
 *
 * @return NRC_R_OK is successful
 */
s32_t nrc_port_mutex_unlock(nrc_port_mutex_t mutex);

/**
 * @brief Creates and initializes a semaphore
 *
 * @param count Start count value for semaphore
 * @param sema Output parameter where the semaphore identifier is stored
 *
 * @return NRC_R_OK is successful
 */
s32_t nrc_port_sema_init(u32_t count, nrc_port_sema_t *sema);

/**
 * @brief Signals the semaphore
 *
 * @param sema Semaphore to signal
 *
 * @return NRC_R_OK is successful
 */
s32_t nrc_port_sema_signal(nrc_port_sema_t sema);

/**
 * @brief Wait for semaphore
 *
 * Waits for semaphore until count is > 0 or until timeout has elapsed.
 * A timeout of 0 causes the call to wait indefinately until count is increased.
 *
 * @param sema Semaphore to wait for
 * @param timeout Timeout in ms until call returns. If 0 wait for indefinately.
 *
 * @return NRC_R_OK is successful or NRC_R_TIMEOUT if timeout elapses.
 */
s32_t nrc_port_sema_wait(nrc_port_sema_t sema, u32_t timeout);

/** 
 * @brief Start one-shot timer
 *
 * @param timeout_ms Time in ms before timer elapses
 * @param tag User defined variable that is returned in the callback
 * @param timeout_fcn Callback function called when the timer elapses
 * @param timer Out parameter used to return the timer handle
 *
 * @return NRC_R_OK is successful
 */
s32_t nrc_port_timer_after(u32_t timeout_ms, void *tag, nrc_port_timeout_fcn_t timeout_fcn, nrc_port_timer_t *timer);

/**
 * @brief Cancel timer
 *
 * Cancels a timer previously started with nrc_port_timer_after.
 *
 * @param timer Timer handle
 *
 * @return NRC_R_OK is successful
 */
s32_t nrc_port_timer_cancel(nrc_port_timer_t timer);

/**
 * @brief Get timer resolution
 *
 * @return Timer resolution in ms
 */
u32_t nrc_port_timer_get_res_ms(void);

/**
 * @brief Get system time
 *
 * Total time since power on
 *
 * @return Time in ms
 */
u64_t nrc_port_timer_get_time_ms(void);

/**
 * @brief Disable HW interrupts
 *
 * @return NRC_R_OK if successful
 */
s32_t nrc_port_irq_disable(void);

/**
 * @brief Enable HW interrupts
 
 * @return NRC_R_OK if successful
 */
s32_t nrc_port_irq_enable(void);

/**
* @brief Print to debug port
*
* @param format String to print
* @param argptr Argument list to format string
*
* @return If successful, number of characters written is returned.
* If failure, negative number is returned.
*/
s32_t nrc_port_vprintf(const char * format, va_list argptr);

/**
* @brief Register error handler
*
* @param error_handler Callback function that is called when the nrc_port_error is called.
*
* @return void
*/
s32_t nrc_port_register_error_handler(nrc_port_error_handler_t error_handler);

/**
 * @brief Call to exit
 *
 * @param file Pointer to file name
 * @param line Line of error/assert
 *
 * @return void
 **/
void nrc_port_error(s8_t *file, u32_t line);

#ifdef __cplusplus
}
#endif

#endif