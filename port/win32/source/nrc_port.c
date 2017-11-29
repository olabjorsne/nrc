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


#include "nrc_port.h"
#include <stdlib.h>
#include <Windows.h>
#include <assert.h>

enum nrc_port_state {
    NRC_PORT_S_INVALID = 0,
    NRC_PORT_S_INITIALISED
};

struct nrc_port {
    enum nrc_port_state state;
    nrc_port_mutex_t    irq_mutex;
};

static struct nrc_port port = { NRC_PORT_S_INVALID, 0 };

s32_t nrc_port_init(void)
{
    s32_t result = NRC_PORT_RES_OK;

    assert(port.state == NRC_PORT_S_INVALID);

    result = nrc_port_mutex_init(&(port.irq_mutex));

    return result;
}

u8_t* nrc_port_heap_alloc(u32_t size)
{
    return (u8_t*)malloc(size);
}
void nrc_port_heap_free(void *buf)
{
    free(buf);
}

u8_t* nrc_port_heap_fast_alloc(u32_t size)
{
    return (u8_t*)malloc(size);
}
void nrc_port_heap_fast_free(void *buf)
{
    free(buf);
}

static DWORD WINAPI win32_thread_fcn(LPVOID lpParam)
{
    nrc_port_thread_fcn_t fcn = (nrc_port_thread_fcn_t)lpParam;

    fcn();

    return 0;
}

s32_t nrc_port_thread_init(
    enum nrc_port_thread_prio   priority,
    u32_t                       stack_size,
    nrc_port_thread_fcn_t       thread_fcn,
    nrc_port_thread_t           *thread_id)
{
    HANDLE  handle;
    s32_t   result = NRC_PORT_RES_OK;

    assert(thread_id != NULL);

    if (stack_size < 8 * 1024) {
        stack_size = 8 * 1024;
    }

    handle = CreateThread(
        NULL,                   // default security attributes
        stack_size,             // stack size  
        win32_thread_fcn,       // thread function name
        thread_fcn,             // argument to thread function 
        CREATE_SUSPENDED,       // creation flags 
        NULL);             // thread identifier 

    if (handle != NULL) {
        *thread_id = (nrc_port_thread_t)handle;
    }
    else {
        result = NRC_PORT_RES_ERROR;
        *thread_id = 0;
    }

	return result;
}
s32_t nrc_port_thread_start(nrc_port_thread_t thread_id)
{
    s16_t   result = NRC_PORT_RES_OK;
    DWORD   win_result;
    
    win_result = ResumeThread((HANDLE)thread_id);

    if (win_result == -1) {
        result = NRC_PORT_RES_ERROR;
    }

	return result;
}

/*
s32_t nrc_port_queue_init(u32_t size, nrc_port_queue_t *queue)
{
	return 0;
}
s32_t nrc_port_queue_put(nrc_port_queue_t queue, void *item)
{
	return 0;
}
void* nrc_port_queue_get(nrc_port_queue_t queue, u32_t timeout)
{
	return 0;
}
*/

s32_t nrc_port_mutex_init(nrc_port_mutex_t *mutex)
{
    s32_t   result = NRC_PORT_RES_OK;
    HANDLE  handle;
    
    assert(mutex != NULL);

    handle = CreateMutex(NULL, FALSE, NULL);

    if (handle != NULL) {
        *mutex = (nrc_port_mutex_t)handle;
    }
    else {
        result = NRC_PORT_RES_ERROR;
        *mutex = 0;
    }

	return result;
}
s32_t nrc_port_mutex_lock(nrc_port_mutex_t mutex, u32_t timeout)
{
    s32_t   result;
    DWORD   win_result;

    if (timeout == 0) {
        timeout = INFINITE;
    }

    win_result = WaitForSingleObject((HANDLE)mutex, timeout);

    if (win_result == WAIT_OBJECT_0) {
        result = NRC_PORT_RES_OK;
    }
    else if (win_result == WAIT_TIMEOUT) {
        result = NRC_PORT_RES_TIMEOUT;
    }
    else {
        result = NRC_PORT_RES_ERROR;
    }

    return result;
}
s32_t nrc_port_mutex_unlock(nrc_port_mutex_t mutex)
{
    s32_t   result = NRC_PORT_RES_OK;
    BOOL    ok;

    ok = ReleaseMutex((HANDLE)mutex);

    if (!ok) {
        result = NRC_PORT_RES_ERROR;
    }

    return result;
}

s32_t nrc_port_sema_init(u32_t count, nrc_port_sema_t *sema)
{
    s32_t   result = NRC_PORT_RES_OK;
    HANDLE  handle;

    assert(sema != NULL);
    
    handle = CreateSemaphore(
        NULL,
        count,
        LONG_MAX,
        NULL);

    if (handle != NULL) {
        *sema = (nrc_port_sema_t)handle;
    }
    else {
        result = NRC_PORT_RES_ERROR;
        *sema = 0;
    }

	return result;
}
s32_t nrc_port_sema_signal(nrc_port_sema_t sema)
{
    s32_t   result = NRC_PORT_RES_OK;
    BOOL    ok;
    LONG    cnt;
    
    ok = ReleaseSemaphore((HANDLE)sema, 1, &cnt);

    if (!ok) {
        result = NRC_PORT_RES_ERROR;
    }

	return result;
}
s32_t nrc_port_sema_wait(nrc_port_sema_t sema, u32_t timeout)
{
    s32_t   result;
    DWORD   win_result;

    if (timeout == 0) {
        timeout = INFINITE;
    }
    
    win_result = WaitForSingleObject((HANDLE)sema, timeout);

    if(win_result == WAIT_OBJECT_0) {
        result = NRC_PORT_RES_OK;
    }
    else if (win_result == WAIT_TIMEOUT) {
        result = NRC_PORT_RES_TIMEOUT;
    }
    else {
        result = NRC_PORT_RES_ERROR;
    }

	return result;
}

s32_t nrc_port_irq_disable(void)
{
    assert(port.state == NRC_PORT_S_INITIALISED);

    return nrc_port_mutex_lock(port.irq_mutex, 0);
}

s32_t nrc_port_irq_enable(void)
{
    assert(port.state == NRC_PORT_S_INITIALISED);

    return nrc_port_mutex_unlock(port.irq_mutex);
}
