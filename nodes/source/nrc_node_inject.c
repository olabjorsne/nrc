#include "nrc_types.h"
#include "nrc_node.h"
#include "nrc_cfg.h"
#include "nrc_os.h"
#include "nrc_timer.h"
#include "nrc_log.h"
#include "nrc_factory.h"
#include <string.h>
#include "nrc_assert.h"

#define NRC_N_INJECT_TYPE           (0xABF23A12)
#define NRC_N_INJECT_EVT_TIMEOUT    (1)

enum nrc_node_inject_types {
    NRC_N_INJECT_TYPE_NONE,
    NRC_N_INJECT_TYPE_STR,
    NRC_N_INJECT_TYPE_TIME
};

enum nrc_node_inject_state {
    NRC_N_INJECT_S_INVALID,
    NRC_N_INJECT_S_CREATED,
    NRC_N_INJECT_S_INITIALISED,
    NRC_N_INJECT_S_STARTED,
    NRC_N_INJECT_S_ERROR
};

struct nrc_node_inject {
    struct nrc_node_hdr         hdr;
    const s8_t                  *topic;

    enum nrc_node_inject_state  state;
    s8_t                        prio;

    //enum nrc_node_inject_types  payload_type;  // TODO: Do we need user configurable types?
    //s8_t                        *payload_string;

    u32_t                       timeout_ms;
    bool_t                      repeat;
    struct nrc_timer_pars       timer_pars;

    u32_t                       type;
};

static nrc_node_t nrc_node_inject_create(struct nrc_node_factory_pars *pars);
static s32_t nrc_node_inject_init(nrc_node_t self);
static s32_t nrc_node_inject_deinit(nrc_node_t self);
static s32_t nrc_node_inject_start(nrc_node_t self);
static s32_t nrc_node_inject_stop(nrc_node_t self);
static s32_t nrc_node_inject_recv_msg(nrc_node_t self, nrc_msg_t msg);
static s32_t nrc_node_inject_recv_evt(nrc_node_t self, u32_t event_mask);

const static s8_t*          _tag = "inject";
static struct nrc_node_api  _api;

void nrc_node_inject_register(void)
{
    s32_t status = nrc_factory_register_node_type("nrc-inject", nrc_node_inject_create);
    if (!OK(status)) {
        NRC_LOGE(_tag, "register: error %d", status);
    }
}

nrc_node_t nrc_node_inject_create(struct nrc_node_factory_pars *pars)
{
    struct nrc_node_inject *node = NULL;

    if ((pars != NULL) && (strcmp("nrc-inject", pars->cfg_type) == 0)) {
        node = (struct nrc_node_inject*)nrc_os_node_alloc(sizeof(struct nrc_node_inject));

        if (node != NULL) {
            memset(node, 0, sizeof(struct nrc_node_inject));
            node->hdr.cfg_id = pars->cfg_id;
            node->hdr.cfg_type = pars->cfg_type;
            node->hdr.cfg_name = pars->cfg_name;

            memset(&_api, 0, sizeof(struct nrc_node_api));
            _api.init = nrc_node_inject_init;
            _api.deinit = nrc_node_inject_deinit;
            _api.start = nrc_node_inject_start;
            _api.stop = nrc_node_inject_stop;
            _api.recv_msg = nrc_node_inject_recv_msg;
            _api.recv_evt = nrc_node_inject_recv_evt;

            pars->api = &_api;

            node->type = NRC_N_INJECT_TYPE;
            node->state = NRC_N_INJECT_S_CREATED;
        }
        else {
            NRC_LOGE(_tag, "create(%s): out of mem", pars->cfg_id);
        }
    }
    else {
        NRC_LOGE(_tag, "create: invalid in params");
    }

    return node;
}

static s32_t nrc_node_inject_init(nrc_node_t slf)
{
    struct nrc_node_inject  *self = (struct nrc_node_inject*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_INJECT_TYPE)) {
        switch (self->state) {
        case NRC_N_INJECT_S_CREATED:
            result = nrc_timer_init();

            if (OK(result)) {
                s32_t timeout_ms = 0;
                result = nrc_cfg_get_int(self->hdr.cfg_id, "timeout", &timeout_ms);
                if (OK(result)) {
                    if (timeout_ms >= 0) {
                        self->timeout_ms = timeout_ms;
                    }
                    else {
                        result = NRC_R_INVALID_CFG;
                    }
                }
            }
            if (OK(result)) {
                s8_t *str;
                result = nrc_cfg_get_str(self->hdr.cfg_id, "repeat", &str);

                if (OK(result)) {
                    if (strcmp(str, "false") == 0) {
                        self->repeat = FALSE;
                    }
                    else {
                        self->repeat = TRUE;
                    }
                }
            }
            if (OK(result)) {
                result = nrc_cfg_get_str(self->hdr.cfg_id, "topic", &self->topic);
            }
            if (OK(result)) {
                // Get node priority
                s32_t prio;
                result = nrc_cfg_get_int(self->hdr.cfg_id, "priority", &prio);
                if (OK(result)) {
                    if ((prio >= S8_MIN_VALUE) && (prio <= S8_MAX_VALUE)) {
                        self->prio = (s8_t)prio;
                    }
                    else {
                        result = NRC_R_INVALID_CFG;
                    }
                }
            }

            if (OK(result)) {
                self->state = NRC_N_INJECT_S_INITIALISED;

                NRC_LOGI(_tag, "init(%s): ok", self->hdr.cfg_id);
            }
            else {
                self->state = NRC_N_INJECT_S_ERROR;

                NRC_LOGE(_tag, "init(%s): failed %d", self->hdr.cfg_id, result);
            }
            break;

        default:
            NRC_LOGE(_tag, "init(%s): invalid state %d", self->hdr.cfg_id, self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "init: invalid in parameter");
    }

    return result;
}

static s32_t nrc_node_inject_deinit(nrc_node_t slf)
{
    struct nrc_node_inject  *self = (struct nrc_node_inject*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_INJECT_TYPE)) {
        switch (self->state) {
        case NRC_N_INJECT_S_INITIALISED:
        case NRC_N_INJECT_S_ERROR:
            // Nothing to free

            self->state = NRC_N_INJECT_S_CREATED;
            result = NRC_R_OK;

            NRC_LOGI(_tag, "deinit(%s): ok", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "deinit(%s): invalid state %d", self->hdr.cfg_id, self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "deinit: invalid in params");
    }

    return result;
}

static s32_t nrc_node_inject_start(nrc_node_t slf)
{
    struct nrc_node_inject  *self = (struct nrc_node_inject*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_INJECT_TYPE)) {
        switch (self->state) {
        case NRC_N_INJECT_S_INITIALISED:
            self->timer_pars.node = self;
            self->timer_pars.evt = NRC_N_INJECT_EVT_TIMEOUT;
            self->timer_pars.prio = self->prio;

            if (self->timeout_ms >= 0) {
                result = nrc_timer_after(self->timeout_ms, &self->timer_pars);
            }
            else {
                result = NRC_R_OK;
            }

            if (OK(result)) {
                self->state = NRC_N_INJECT_S_STARTED;

                NRC_LOGI(_tag, "start(%s): ok", self->hdr.cfg_id);
            }
            else {
                self->state = NRC_N_INJECT_S_ERROR;

                NRC_LOGE(_tag, "start(%s): error %d", self->hdr.cfg_id, result);
            }
            break;

        default:
            NRC_LOGE(_tag, "start(%s): invalid state %d", self->hdr.cfg_id, self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "start: invalid in params");
    }

    return result;
}

static s32_t nrc_node_inject_stop(nrc_node_t slf)
{
    struct nrc_node_inject  *self = (struct nrc_node_inject*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_INJECT_TYPE)) {
        switch (self->state) {
        case NRC_N_INJECT_S_STARTED:
            result = nrc_timer_cancel(self->timer_pars.timer);

            self->state = NRC_N_INJECT_S_INITIALISED;
            result = NRC_R_OK;

            NRC_LOGI(_tag, "stop(%s): ok", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "stop(%s): invalid state %d", self->hdr.cfg_id, self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "start: invalid in params");
    }

    return result;
}

static s32_t nrc_node_inject_recv_msg(nrc_node_t slf, nrc_msg_t msg)
{
    struct nrc_node_inject  *self = (struct nrc_node_inject*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_INJECT_TYPE) && (msg != NULL)) {

        switch (self->state) {
        case NRC_N_INJECT_S_STARTED:
             NRC_LOGW(_tag, "recv_msg(%s): unexpected msg %d", self->hdr.cfg_id);
            break;

        default:
            NRC_LOGE(_tag, "recv_msg(%s): invalid state %d", self->hdr.cfg_id, self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "recv_msg: invalid in params");
    }

    if (msg != NULL) {
        nrc_os_msg_free(msg);
    }

    return result;
}

static s32_t nrc_node_inject_recv_evt(nrc_node_t slf, u32_t event_mask)
{
    struct nrc_node_inject  *self = (struct nrc_node_inject*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;
    struct nrc_msg_hdr      *hdr = NULL;

    if ((self != NULL) && (self->type == NRC_N_INJECT_TYPE)) {
        switch (self->state) {
        case NRC_N_INJECT_S_STARTED:
            if ((event_mask & self->timer_pars.evt) != 0) {
                if ((self->timeout_ms >= 0) && (self->repeat == TRUE)) {
                    result = nrc_timer_after(self->timeout_ms, &self->timer_pars);
                }

                hdr = (struct nrc_msg_hdr*)nrc_os_msg_alloc(sizeof(struct nrc_msg_hdr));
                if (hdr != NULL) {
                    hdr->topic = self->topic;
                    hdr->type = NRC_MSG_TYPE_EMPTY;

                    result = nrc_os_send_msg_from(self, hdr, self->prio);
                }
                else {
                    result = NRC_R_OUT_OF_MEM;
                }

                NRC_LOGV(_tag, "recv_evt(%s): result %d", event_mask);
            }
            else {
                NRC_LOGE(_tag, "recv_evt(%s): invalid evt %d", self->hdr.cfg_id, event_mask);
            }
            break;

        default:
            NRC_LOGE(_tag, "recv_evt(%s): invalid state %d", self->hdr.cfg_id, self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }
    else {
        NRC_LOGE(_tag, "recv_evt: invalid in params");
    }

    return result;
}

