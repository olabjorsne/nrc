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


#include "nrc_types.h"
#include "test_port.h"

#include <stdio.h>
#include <Windows.h>

static nrc_port_sema_t      _sema;
static nrc_port_mutex_t     _mutex;
static nrc_port_thread_t    _thread_id;
static u32_t                _delay;
static u32_t                _bit_mask = 0;

static bool_t validate_time(u64_t time_begin, u64_t time_end, u32_t delay)
{
    bool_t ok = FALSE;

    if (delay > nrc_port_timer_get_res_ms()) {
        if ((time_end > time_begin) &&
            ((time_end - time_begin) <= (delay + nrc_port_timer_get_res_ms())) &&
            ((time_end - time_begin) >= delay)) {

            ok = TRUE;
        }
    }
    else {
        if ((time_end >= time_begin) &&
            (time_end - time_begin <= nrc_port_timer_get_res_ms())) {

            ok = TRUE;
        }
    }

    return ok;
}

bool_t test_get_time_startup()
{
    bool_t ok = FALSE;

    u64_t   time1 = nrc_port_timer_get_time_ms();

    if (time1 == 0) {
        ok = TRUE;
    }

    return ok;
}

bool_t test_get_time_duration(u32_t ms)
{
    bool_t  ok = FALSE;
    u64_t   time2;
    u64_t   time1 = nrc_port_timer_get_time_ms();

    Sleep(ms);

    time2 = nrc_port_timer_get_time_ms();

    ok = validate_time(time1, time2, ms);

    return ok;
}

bool_t test_sema_timeout(u32_t ms)
{
    bool_t          ok = FALSE;
    u64_t           time2;
    u64_t           time1;
    nrc_port_sema_t sema;
    s32_t           result;

    result = nrc_port_sema_init(0, &sema);

    if (result == NRC_R_OK) {
        time1 = nrc_port_timer_get_time_ms();

        result = nrc_port_sema_wait(sema, ms);

        time2 = nrc_port_timer_get_time_ms();

        if (result == NRC_R_TIMEOUT) {
            ok = validate_time(time1, time2, ms);
        }
    }

    return ok;

}

static void sema_thread_fcn(void)
{
    s32_t result;

    Sleep(_delay);

    result = nrc_port_sema_signal(_sema);
}

bool_t test_sema_and_thread(u32_t ms, u32_t sema_count)
{
    bool_t  ok = FALSE;
    s32_t   result;
    u64_t   time2;
    u64_t   time1;
    u32_t   i;

    _delay = ms;

    result = nrc_port_sema_init(sema_count, &_sema);

    if (result == NRC_R_OK) {
        result = nrc_port_thread_init(NRC_PORT_THREAD_PRIO_NORMAL, 4096, sema_thread_fcn, &_thread_id);
    }

    if (result == NRC_R_OK) {
        time1 = nrc_port_timer_get_time_ms();
        result = nrc_port_thread_start(_thread_id);
    }

    for (i = 0; (i < sema_count) && (result == NRC_R_OK); i++) {
        result = nrc_port_sema_wait(_sema, 0);
    }

    if (result == NRC_R_OK) {
        result = nrc_port_sema_wait(_sema, 0);
        time2 = nrc_port_timer_get_time_ms();
    }

    if (result == NRC_R_OK) {
        ok = validate_time(time1, time2, ms);
    }

    return ok;
}

static void timeout_fcn(nrc_port_timer_t timer_id, void *tag)
{
    _bit_mask |= (u32_t)tag;

    nrc_port_sema_signal(_sema);
}

bool_t test_timeout(u32_t ms)
{
    bool_t              ok = FALSE;
    nrc_port_timer_t    timer;
    s32_t               result;
    u64_t               time_begin, time_end;

    result = nrc_port_sema_init(0, &_sema);

    if (result == NRC_R_OK) {
        time_begin = nrc_port_timer_get_time_ms();
        result = nrc_port_timer_after(ms, 0, timeout_fcn, &timer);
    }

    if (result == NRC_R_OK) {
        result = nrc_port_sema_wait(_sema, 0);
        time_end = nrc_port_timer_get_time_ms();
    }

    if (result == NRC_R_OK) {
        ok = validate_time(time_begin, time_end, ms);
    }

    return ok;
}

bool_t test_timeout_after_and_cancel(void)
{
    bool_t ok = FALSE;
    s32_t result;

    nrc_port_timer_t timer1;
    nrc_port_timer_t timer2;
    nrc_port_timer_t timer4;
    nrc_port_timer_t timer8;
    nrc_port_timer_t timer16;
    nrc_port_timer_t timer32;
    nrc_port_timer_t timer64;

    _bit_mask = 0;

    result = nrc_port_sema_init(0, &_sema);

    if (result == NRC_R_OK) {
        result = nrc_port_timer_after(2050, (void*)1, timeout_fcn, &timer1);
    }

    if (result == NRC_R_OK) {
        result = nrc_port_timer_after(2000, (void*)2, timeout_fcn, &timer2);
    }
    if (result == NRC_R_OK) {
        result = nrc_port_timer_after(1000, (void*)4, timeout_fcn, &timer4);
    }
    if (result == NRC_R_OK) {
        result = nrc_port_timer_after(3000, (void*)8, timeout_fcn, &timer8);
    }
    if (result == NRC_R_OK) {
        result = nrc_port_timer_after(1100, (void*)16, timeout_fcn, &timer16);
    }
    if (result == NRC_R_OK) {
        result = nrc_port_timer_after(4000, (void*)32, timeout_fcn, &timer32);
    }
    if (result == NRC_R_OK) {
        result = nrc_port_timer_after(5000, (void*)64, timeout_fcn, &timer64);
    }

    if (result == NRC_R_OK) {
        result = nrc_port_timer_cancel(timer64);
    }
    if (result == NRC_R_OK) {
        result = nrc_port_sema_wait(_sema, 0);
    }
    if (result == NRC_R_OK) {
        if (_bit_mask != 4)
        {
            result = NRC_R_ERROR;
        }
    }
    if (result == NRC_R_OK) {
        nrc_port_timer_cancel(timer16);
    }
    if (result == NRC_R_OK) {
        result = nrc_port_sema_wait(_sema, 0);
    }
    if (result == NRC_R_OK) {
        if (_bit_mask != (4 + 2))
        {
            result = NRC_R_ERROR;
        }
    }
    if (result == NRC_R_OK) {
        nrc_port_timer_cancel(timer1);
    }
    if (result == NRC_R_OK) {
        result = nrc_port_sema_wait(_sema, 0);
    }
    if (result == NRC_R_OK) {
        if (_bit_mask != (4 + 2 + 8))
        {
            result = NRC_R_ERROR;
        }
    }
    if (result == NRC_R_OK) {
        result = nrc_port_sema_wait(_sema, 0);
    }
    if (result == NRC_R_OK) {
        if (_bit_mask != (4 + 2 + 8 + 32))
        {
            result = NRC_R_ERROR;
        }
    }
    if (result == NRC_R_OK) {
        result = nrc_port_sema_wait(_sema, 5000);
        if (result == NRC_R_TIMEOUT) {
            result = NRC_R_OK;
        }
    }
    if (result == NRC_R_OK) {
        if (_bit_mask != (4 + 2 + 8 + 32))
        {
            result = NRC_R_ERROR;
        }
        else {
            ok = TRUE;
        }
    }

    return ok;
}

static void mutex_thread_fcn(void)
{
    nrc_port_mutex_lock(_mutex, 0);
    _bit_mask = 1;
    nrc_port_mutex_unlock(_mutex);
}

bool_t test_mutex(void)
{
    bool_t ok = FALSE;
    s32_t       result;

    _bit_mask = 0;

    result = nrc_port_sema_init(0, &_sema);

    if (result == NRC_R_OK) {
        result = nrc_port_mutex_init(&_mutex);
    }

    if (result == NRC_R_OK) {
        result = nrc_port_mutex_lock(_mutex, 0);
    }

    if (result == NRC_R_OK) {
        result = nrc_port_thread_init(NRC_PORT_THREAD_PRIO_NORMAL, 4096, mutex_thread_fcn, &_thread_id);
    }

    if (result == NRC_R_OK) {
        result = nrc_port_thread_start(_thread_id);
    }

    if (result == NRC_R_OK) {
        result = nrc_port_sema_wait(_sema, 100);
        if (result == NRC_R_TIMEOUT) {
            result = NRC_R_OK;
        }
    }

    if (result == NRC_R_OK) {
        result = nrc_port_mutex_lock(_mutex, 10);
    }

    if (result == NRC_R_OK) {
        if (_bit_mask != 0) {
            result = NRC_R_ERROR;
        }
    }

    if (result == NRC_R_OK) {
        result = nrc_port_mutex_unlock(_mutex);
    }

    if (result == NRC_R_OK) {
        result = nrc_port_mutex_unlock(_mutex);
    }

    if (result == NRC_R_OK) {
        result = nrc_port_sema_wait(_sema, 10);
        if (result == NRC_R_TIMEOUT) {
            result = NRC_R_OK;
        }
    }

    if (result == NRC_R_OK) {
        if (_bit_mask == 1) {
            ok = TRUE;
        }
    }
    if (result == NRC_R_OK) {

    }

    return ok;
}

bool_t test_all(void)
{
    bool_t ok = FALSE;

    printf("test_get_time_startup: ");
    ok = test_get_time_startup();
    printf("%d\n", ok);

    printf("test_get_time_duration(1): ");
    ok = test_get_time_duration(1);
    printf("%d\n", ok);

    printf("test_get_time_duration(10): ");
    ok = test_get_time_duration(10);
    printf("%d\n", ok);

    printf("test_get_time_duration(500): ");
    ok = test_get_time_duration(500);
    printf("%d\n", ok);

    printf("test_get_time_duration(5000): ");
    ok = test_get_time_duration(5000);
    printf("%d\n", ok);

    printf("test_sema_timeout(1): ");
    ok = test_sema_timeout(1);
    printf("%d\n", ok);

    printf("test_sema_timeout(10): ");
    ok = test_sema_timeout(10);
    printf("%d\n", ok);

    printf("test_sema_timeout(500): ");
    ok = test_sema_timeout(500);
    printf("%d\n", ok);

    printf("test_sema_timeout(5000): ");
    ok = test_sema_timeout(5000);
    printf("%d\n", ok);

    printf("test_sema_and_thread(0, 0): ");
    ok = test_sema_and_thread(0, 0);
    printf("%d\n", ok);

    printf("test_sema_and_thread(1, 1): ");
    ok = test_sema_and_thread(1, 1);
    printf("%d\n", ok);

    printf("test_sema_and_thread(10, 10): ");
    ok = test_sema_and_thread(10, 10);
    printf("%d\n", ok);

    printf("test_sema_and_thread(500, 500): ");
    ok = test_sema_and_thread(500, 500);
    printf("%d\n", ok);

    printf("test_sema_and_thread(5000, 5000): ");
    ok = test_sema_and_thread(5000, 5000);
    printf("%d\n", ok);

    printf("test_timeout(1): ");
    ok = test_timeout(1);
    printf("%d\n", ok);

    printf("test_timeout(10): ");
    ok = test_timeout(10);
    printf("%d\n", ok);

    printf("test_timeout(1000): ");
    ok = test_timeout(1000);
    printf("%d\n", ok);

    printf("test_timeout(10000): ");
    ok = test_timeout(10000);
    printf("%d\n", ok);

    printf("test_timeout(10): ");
    ok = test_timeout(10);
    printf("%d\n", ok);

    printf("test_mutex(): ");
    ok = test_mutex();
    printf("%d\n", ok);



    return ok;
}
