#include "nrc_types.h"
#include "test_port.h"

#include <stdio.h>
#include <Windows.h>

static nrc_port_sema_t      _sema;
static nrc_port_thread_t    _thread_id;
static u32_t                _delay;

static bool_t test_get_time_startup()
{
    bool_t ok = FALSE;

    u64_t   time1 = nrc_port_timer_get_time_ms();

    if (time1 == 0) {
        ok = TRUE;
    }

    return ok;
}

static bool_t test_get_time_duration(u32_t ms)
{
    bool_t  ok = FALSE;
    u64_t   time2;
    u64_t   time1 = nrc_port_timer_get_time_ms();

    Sleep(ms);

    time2 = nrc_port_timer_get_time_ms();

    if (ms > nrc_port_timer_get_res_ms()) {
        if ((time2 > time1) &&
            ((time2 - time1) <= (ms + nrc_port_timer_get_res_ms())) &&
            ((time2 - time1) >= ms)) {

            ok = TRUE;
        }
    }
    else {
        if ((time2 >= time1) &&
            (time2 - time1 <= nrc_port_timer_get_res_ms())) {
            ok = TRUE;
        }
    }

    return ok;
}

static bool_t test_sema_timeout(u32_t ms)
{
    bool_t          ok = FALSE;
    u64_t           time2;
    u64_t           time1;
    nrc_port_sema_t sema;
    s32_t           result;

    result = nrc_port_sema_init(0, &sema);

    if (result == NRC_PORT_RES_OK) {
        time1 = nrc_port_timer_get_time_ms();

        result = nrc_port_sema_wait(sema, ms);

        time2 = nrc_port_timer_get_time_ms();

        if (result == NRC_PORT_RES_TIMEOUT) {

            if (ms > nrc_port_timer_get_res_ms()) {
                if ((time2 > time1) &&
                    ((time2 - time1) <= (ms + nrc_port_timer_get_res_ms())) &&
                    ((time2 - time1) >= ms)) {

                    ok = TRUE;
                }
            }
            else {
                if ((time2 >= time1) &&
                    (time2 - time1 <= nrc_port_timer_get_res_ms())) {
                    ok = TRUE;
                }
            }
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

bool_t test_sema_and_thread(u32_t ms)
{
    bool_t  ok = FALSE;
    s32_t   result;
    u64_t   time2;
    u64_t   time1;

    _delay = ms;

    result = nrc_port_sema_init(0, &_sema);

    if (result == NRC_PORT_RES_OK) {
        result = nrc_port_thread_init(NRC_PORT_THREAD_PRIO_NORMAL, 4096, sema_thread_fcn, &_thread_id);
    }

    if (result == NRC_PORT_RES_OK) {
        time1 = nrc_port_timer_get_time_ms();
        result = nrc_port_thread_start(_thread_id);
    }

    if (result == NRC_PORT_RES_OK) {
        result = nrc_port_sema_wait(_sema, 0);
        time2 = nrc_port_timer_get_time_ms();
    }

    if (result == NRC_PORT_RES_OK) {
        if (ms > nrc_port_timer_get_res_ms()) {
            if ((time2 > time1) &&
                ((time2 - time1) <= (ms + nrc_port_timer_get_res_ms())) &&
                ((time2 - time1) >= ms)) {

                ok = TRUE;
            }
        }
        else {
            if ((time2 >= time1) &&
                (time2 - time1 <= nrc_port_timer_get_res_ms())) {
                ok = TRUE;
            }
        }
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

    printf("test_sema_and_thread(0): ");
    ok = test_sema_and_thread(0);
    printf("%d\n", ok);

    printf("test_sema_and_thread(1): ");
    ok = test_sema_and_thread(1);
    printf("%d\n", ok);

    printf("test_sema_and_thread(10): ");
    ok = test_sema_and_thread(10);
    printf("%d\n", ok);

    printf("test_sema_and_thread(500): ");
    ok = test_sema_and_thread(500);
    printf("%d\n", ok);

    printf("test_sema_and_thread(5000): ");
    ok = test_sema_and_thread(5000);
    printf("%d\n", ok);



    return ok;
}
