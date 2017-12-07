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

#include "nrc_timer.h"
#include "nrc_port.h"
#include "nrc_node.h"
#include "nrc_os.h"
#include "nrc_log.h"
#include <assert.h>

enum nrc_timer_state {
    NRC_TIMER_S_INVALID,
    NRC_TIMER_S_INITIALISED
};

struct nrc_timer {
    enum nrc_timer_state  state;
};

static void timeout_fcn(nrc_port_timer_t timer_id, void* tag);

static struct nrc_timer _timer = { NRC_TIMER_S_INVALID };

s32_t nrc_timer_init(void)
{
    s32_t result = NRC_PORT_RES_OK;

    if (_timer.state == NRC_TIMER_S_INVALID) {

        result = nrc_port_init();

        if (result == NRC_PORT_RES_OK) {
            _timer.state = NRC_TIMER_S_INITIALISED;
        }
    }

    return result;
}

s32_t nrc_timer_after(u32_t timeout, struct nrc_timer_info *info)
{
    s32_t result = NRC_PORT_RES_INVALID_IN_PARAM;

    if (info != 0) {
        result = nrc_port_timer_after(timeout, info, timeout_fcn, &info->timer_id);
    }

    return result;
}

s32_t nrc_timer_cancel(nrc_timer_id_t timer_id)
{
    return nrc_port_timer_cancel(timer_id);
}

static void timeout_fcn(nrc_port_timer_t timer_id, void* info)
{
    s32_t                   result;
    struct nrc_timer_info    *timer_info = (struct nrc_timer_info*)info;

    if ((timer_info != 0) &&
        (timer_id == timer_info->timer_id)) {

        result = nrc_os_set_evt(timer_info->node_id, timer_info->evt, timer_info->prio);
    }
    else {
        NRC_LOGD("nrc_timer", "timeout_fcn: invalid parameters timer_info ptr %d, timer_id %d", timer_info, timer_info->timer_id);
    }
}
