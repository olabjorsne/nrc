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

#define NRC_PORT_TIMER_TYPE (0xAAAAAAAA)

enum nrc_port_state {
    NRC_PORT_S_INVALID = 0,
    NRC_PORT_S_INITIALISED
};

struct nrc_port_timer {
    struct nrc_port_timer   *next;
    u32_t                   type;
    s32_t                   tag;
    u32_t                   timeout_ms;
    nrc_port_timeout_fcn_t  fcn;
};

struct nrc_port {
    enum nrc_port_state     state;

    nrc_port_mutex_t        irq_mutex;

    nrc_port_mutex_t        timer_mutex;
    struct nrc_port_timer   *timer_list;
};

static struct nrc_port _port = { NRC_PORT_S_INVALID, 0, 0, 0};

s32_t nrc_port_init(void)
{
    s32_t result = NRC_PORT_RES_OK;

    assert(_port.state == NRC_PORT_S_INVALID);

    result = nrc_port_mutex_init(&(_port.irq_mutex));

    result = nrc_port_mutex_init(&(_port.timer_mutex));

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

static VOID CALLBACK timer_proc(
    _In_ HWND     hwnd,
    _In_ UINT     uMsg,
    _In_ UINT_PTR idEvent,
    _In_ DWORD    dwTime)
{
    s32_t result;
    struct nrc_port_timer *tmr = 0;
    struct nrc_port_timer *tmr_pre = 0;
    struct nrc_port_timer *triggered = 0;

    result = nrc_port_mutex_lock(_port.timer_mutex, 0);

    tmr = _port.timer_list;
    tmr_pre = 0;

    while (tmr != 0) {
        tmr->timeout_ms = (tmr->timeout_ms > NRC_PORT_TIMER_RES_MS) ? (tmr->timeout_ms - NRC_PORT_TIMER_RES_MS) : 0;

        if (tmr->timeout_ms == 0) {
            //Remove timer from list
            if (tmr_pre == 0) {
                _port.timer_list = tmr->next;
            }
            else {
                tmr_pre->next = tmr->next;
            }
            //Add timer to trigger list
            tmr->next = triggered;
            triggered = tmr->next;
            
            //Update for next iteration
            if (tmr_pre == 0) {
                tmr = _port.timer_list;
            }
            else {
                tmr = tmr_pre->next;
            }
        }
        else {
            tmr_pre = tmr;
            tmr = tmr->next;
        }
    }
    if (_port.timer_list != 0) {
        UINT_PTR wres = SetTimer(NULL, 0, (UINT)NRC_PORT_TIMER_RES_MS, timer_proc);
    }
    result = nrc_port_mutex_unlock(_port.timer_mutex);

    while (triggered != 0)
    {
        tmr = triggered;
        triggered = tmr->next;

        tmr->fcn((nrc_port_timer_t)tmr, tmr->tag);
        nrc_port_heap_fast_free(tmr);
    }
}

s32_t nrc_port_timer_after(u32_t timeout_ms, s32_t tag, nrc_port_timeout_fcn_t fcn, nrc_port_timer_t *timer_id)
{
    s32_t       result = NRC_PORT_RES_INVALID_IN_PARAM;
    UINT_PTR    wres;

    if (timer_id != 0) {

        struct nrc_port_timer *timer = (struct nrc_port_timer*)nrc_port_heap_fast_alloc(sizeof(struct nrc_port_timer));

        if (timer != 0) {
            timer->fcn = fcn;
            timer->tag = tag;
            timer->timeout_ms = timeout_ms;

            result = nrc_port_mutex_lock(_port.timer_mutex, 0);

            if (_port.timer_list == 0) {
                _port.timer_list = timer;

                wres = SetTimer(NULL, 0, (UINT)NRC_PORT_TIMER_RES_MS, timer_proc);
            }
            else {
                timer->next = _port.timer_list;
                _port.timer_list = timer;

                timer->timeout_ms += NRC_PORT_TIMER_RES_MS;
            }

            *timer_id = (nrc_port_timer_t)timer;

            result = nrc_port_mutex_unlock(_port.timer_mutex);

            result = NRC_PORT_RES_OK;
        }
        else {
            result = NRC_PORT_RES_OUT_OF_MEM;
        }
    }

    return result;
}

s32_t nrc_port_timer_cancel(nrc_port_timer_t timer_id)
{
    s32_t result = NRC_PORT_RES_INVALID_IN_PARAM;

    if (timer_id != 0) {

        struct nrc_port_timer *timer = (struct nrc_port_timer*)timer_id;

        result = NRC_PORT_RES_OK;

        result = nrc_port_mutex_lock(_port.timer_mutex, 0);
        if (_port.timer_list != 0) {
            if (timer == _port.timer_list) {
                _port.timer_list = timer->next;
                nrc_port_heap_fast_free(timer);
            }
            else {
                struct nrc_port_timer *tmr = _port.timer_list;

                while ((tmr->next != 0) && (tmr->next != timer)) {
                    tmr = tmr->next;
                }
                if ((tmr->next != 0) && (tmr->next == timer)) {
                    tmr->next = tmr->next->next;
                    nrc_port_heap_fast_free(timer);
                }
            }
        }
        result = nrc_port_mutex_unlock(_port.timer_mutex);
    }

    return result;
}

s32_t nrc_port_irq_disable(void)
{
    assert(_port.state == NRC_PORT_S_INITIALISED);

    return nrc_port_mutex_lock(_port.irq_mutex, 0);
}

s32_t nrc_port_irq_enable(void)
{
    assert(_port.state == NRC_PORT_S_INITIALISED);

    return nrc_port_mutex_unlock(_port.irq_mutex);
}
