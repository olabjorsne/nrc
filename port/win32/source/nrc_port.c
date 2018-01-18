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
#include <stdio.h>
#include <stdarg.h>

#include "nrc_log.h"

#define NRC_PORT_TIMER_TYPE         (0xAAAAAAAA)
#define NRC_PORT_TIMER_RES_MS       (32)
#define NRC_PORT_MAX_CONFIG_SIZE    (10000)

enum nrc_port_state {
    NRC_PORT_S_INVALID = 0,
    NRC_PORT_S_INITIALISED
};

struct nrc_port_timer {
    struct nrc_port_timer   *next;
    u32_t                   type;
    void                    *tag;
    u64_t                   time;
    nrc_port_timeout_fcn_t  fcn;
};

struct nrc_port {
    enum nrc_port_state     state;

    nrc_port_mutex_t        irq_mutex;

    nrc_port_mutex_t        timer_mutex;
    nrc_port_thread_t       timer_thread_id;
    u64_t                   time_start;
    struct nrc_port_timer   *timer_list;
    s8_t                    flows_config_buffer[NRC_PORT_MAX_CONFIG_SIZE];
    u32_t                   flows_config_size;
};

static void timer_thread_fcn(void);

s32_t nrc_port_thread_start(nrc_port_thread_t thread_id);

static struct nrc_port _port = {0};
static const s8_t *TAG = "main";

s32_t nrc_port_init(void)
{
    s32_t result = NRC_R_OK;

    if (_port.state == NRC_PORT_S_INVALID) {

        result = nrc_port_mutex_init(&(_port.irq_mutex));

        result = nrc_port_mutex_init(&(_port.timer_mutex));

        result = nrc_port_thread_init(NRC_PORT_THREAD_PRIO_CRITICAL, 4096, timer_thread_fcn, &(_port.timer_thread_id));

        result = nrc_port_thread_start(_port.timer_thread_id);

        _port.time_start = GetTickCount64();

        if (result == NRC_R_OK) {
            _port.state = NRC_PORT_S_INITIALISED;
        }
    }

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
    s32_t   result = NRC_R_OK;

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
        result = NRC_R_ERROR;
        *thread_id = 0;
    }

	return result;
}
s32_t nrc_port_thread_start(nrc_port_thread_t thread_id)
{
    s16_t   result = NRC_R_OK;
    DWORD   win_result;
    
    win_result = ResumeThread((HANDLE)thread_id);

    if (win_result == -1) {
        result = NRC_R_ERROR;
    }

	return result;
}

s32_t nrc_port_mutex_init(nrc_port_mutex_t *mutex)
{
    s32_t   result = NRC_R_OK;
    HANDLE  handle;
    
    assert(mutex != NULL);

    handle = CreateMutex(NULL, FALSE, NULL);

    if (handle != NULL) {
        *mutex = (nrc_port_mutex_t)handle;
    }
    else {
        result = NRC_R_ERROR;
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
        result = NRC_R_OK;
    }
    else if (win_result == WAIT_TIMEOUT) {
        result = NRC_R_TIMEOUT;
    }
    else {
        result = NRC_R_ERROR;
    }

    return result;
}
s32_t nrc_port_mutex_unlock(nrc_port_mutex_t mutex)
{
    s32_t   result = NRC_R_OK;
    BOOL    ok;

    ok = ReleaseMutex((HANDLE)mutex);

    if (!ok) {
        result = NRC_R_ERROR;
    }

    return result;
}

s32_t nrc_port_sema_init(u32_t count, nrc_port_sema_t *sema)
{
    s32_t       result = NRC_R_OK;
    HANDLE      handle;

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
        result = NRC_R_ERROR;
        *sema = 0;
    }

	return result;
}
s32_t nrc_port_sema_signal(nrc_port_sema_t sema)
{
    s32_t   result = NRC_R_OK;
    BOOL    ok;
    LONG    cnt;
    
    ok = ReleaseSemaphore((HANDLE)sema, 1, &cnt);

    if (!ok) {
        result = NRC_R_ERROR;
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
        result = NRC_R_OK;
    }
    else if (win_result == WAIT_TIMEOUT) {
        result = NRC_R_TIMEOUT;
    }
    else {
        result = NRC_R_ERROR;
    }

	return result;
}

static void timer_thread_fcn(void)
{
    s32_t result;
    struct nrc_port_timer *tmr = 0;
    struct nrc_port_timer *tmr_pre = 0;
    struct nrc_port_timer *triggered = 0;

    while (_port.state == NRC_PORT_S_INITIALISED) {
        Sleep(NRC_PORT_TIMER_RES_MS);

        result = nrc_port_mutex_lock(_port.timer_mutex, 0);

        tmr = _port.timer_list;
        tmr_pre = 0;

        while (tmr != 0) {

            if (tmr->time <= nrc_port_timer_get_time_ms()) {

                //Remove timer from list
                if (tmr_pre == 0) {
                    _port.timer_list = tmr->next;
                }
                else {
                    tmr_pre->next = tmr->next;
                }
                //Add timer to trigger list
                tmr->next = triggered;
                triggered = tmr;

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

        result = nrc_port_mutex_unlock(_port.timer_mutex);

        while (triggered != 0)
        {
            tmr = triggered;
            triggered = tmr->next;

            tmr->fcn((nrc_port_timer_t)tmr, tmr->tag);
            nrc_port_heap_fast_free(tmr);
        }
    }
}

s32_t nrc_port_timer_after(u32_t timeout_ms, void *tag, nrc_port_timeout_fcn_t fcn, nrc_port_timer_t *timer_id)
{
    s32_t result = NRC_R_INVALID_IN_PARAM;

    if (timer_id != 0) {

        struct nrc_port_timer *timer = (struct nrc_port_timer*)nrc_port_heap_fast_alloc(sizeof(struct nrc_port_timer));

        if (timer != 0) {
            timeout_ms = (timeout_ms < 10) ? 10 : timeout_ms;

            timer->fcn = fcn;
            timer->tag = tag;
            timer->time = timeout_ms + nrc_port_timer_get_time_ms(); //Time when to trigger

            result = nrc_port_mutex_lock(_port.timer_mutex, 0);

            timer->next = _port.timer_list;
            _port.timer_list = timer;

            *timer_id = (nrc_port_timer_t)timer;

            result = nrc_port_mutex_unlock(_port.timer_mutex);

            result = NRC_R_OK;
        }
        else {
            result = NRC_R_OUT_OF_MEM;
        }
    }

    return result;
}

s32_t nrc_port_timer_cancel(nrc_port_timer_t timer_id)
{
    s32_t result = NRC_R_INVALID_IN_PARAM;

    if (timer_id != 0) {

        struct nrc_port_timer *timer = (struct nrc_port_timer*)timer_id;

        result = NRC_R_OK;

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

u32_t nrc_port_timer_get_res_ms(void)
{
    return NRC_PORT_TIMER_RES_MS;
}

u64_t nrc_port_timer_get_time_ms(void)
{

    u64_t now = GetTickCount64();

    return (now - _port.time_start);
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

u32_t nrc_port_timestamp_in_ms(void)
{
    s32_t timestamp = 0;
    if (_port.state == NRC_PORT_S_INITIALISED) {
        timestamp = (u32_t)(GetTickCount64() - _port.time_start);
    }
    else {
        timestamp = 0;
    }
    return timestamp;
}

s32_t nrc_port_vprintf(const char *format, va_list argptr)
{
    return vprintf(format, argptr);
}

extern s8_t *flows_file;
s32_t nrc_port_get_config(u8_t **config, u32_t *size)
{
    s32_t status = NRC_R_ERROR;
    FILE *stream;
    errno_t err;
    
    *config = NULL;
    *size = 0;

    if (flows_file) {
        err = fopen_s(&stream, flows_file, "r");
        if (err == 0) {
            status = NRC_R_OK;
        }
        else {
            NRC_LOGE("nrc_port", "Flows file : %s could not be opened");
            status = NRC_R_NOT_FOUND;
        }

        if (status == NRC_R_OK) {
            s32_t i = 0;
            s8_t data = 0;
            do 
            {
                data = fgetc(stream);
                if (data != EOF) {
                    _port.flows_config_buffer[i++] = data;
                }
                else {
                    _port.flows_config_size = i;
                }

            } while (data != EOF);
        }

        if (stream) {
            err = fclose(stream);
        }
        *config = _port.flows_config_buffer;
        *size = _port.flows_config_size;

        NRC_LOGI(TAG, "Config file : \t%s", flows_file);
        NRC_LOGI(TAG, "Config size : \t%d bytes", _port.flows_config_size);
    }
    else {
        NRC_LOGI(TAG, "No config file defined");
    }
    return status;
}
