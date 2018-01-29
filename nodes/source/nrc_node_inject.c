#include "nrc_types.h"
#include "nrc_node.h"
#include "nrc_cfg.h"
#include "nrc_os.h"
#include "nrc_timer.h"
#include "nrc_log.h"
#include "nrc_node_factory.h"
#include <string.h>
#include <assert.h>

#define NRC_N_INJECT_EVT_TIMEOUT    (1)
#define NRC_N_INJECT_MAX_WIRES      (4)

enum nrc_node_inject_types {
    NRC_N_INJECT_TYPE_NONE,
    NRC_N_INJECT_TYPE_STR,
    NRC_N_INJECT_TYPE_TIME
};

enum nrc_node_inject_state {
    NRC_N_INJECT_S_INVALID,
    NRC_N_INJECT_S_INITIALISED,
    NRC_N_INJECT_S_STARTED
};

struct nrc_node_inject {
    struct nrc_node_hdr         hdr;
    enum nrc_node_inject_state  state;
    const s8_t                  *topic;
    nrc_node_t                  wire[NRC_N_INJECT_MAX_WIRES];

    enum nrc_node_inject_types  payload_type;
    s8_t                        *payload_string;

    u32_t                       period_ms;
    s8_t                        prio;
    struct nrc_timer_pars       timer_pars;
};

static nrc_node_t nrc_node_inject_create(struct nrc_node_factory_pars *pars);
static s32_t nrc_node_inject_init(nrc_node_t self);
static s32_t nrc_node_inject_deinit(nrc_node_t self);
static s32_t nrc_node_inject_start(nrc_node_t self);
static s32_t nrc_node_inject_stop(nrc_node_t self);
static s32_t nrc_node_inject_recv_msg(nrc_node_t self, nrc_msg_t msg);
static s32_t nrc_node_inject_recv_evt(nrc_node_t self, u32_t event_mask);

const static s8_t* _tag = "inject";
static struct nrc_node_api _api;


void nrc_node_inject_register(void)
{
    s32_t status = nrc_factory_register("inject", nrc_node_inject_create);
    if (!OK(status)) {
        NRC_LOGE(_tag, "Registration to factory failed")
    }
}

nrc_node_t nrc_node_inject_create(struct nrc_node_factory_pars *pars)
{
    struct nrc_node_inject *node = NULL;

    if ((pars != NULL) && (strcmp("inject", pars->cfg_type) == 0)) {
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

            node->state = NRC_N_INJECT_S_INVALID;
        }
    }

    return node;
}

static s32_t nrc_node_inject_init(nrc_node_t slf)
{
    struct nrc_node_inject  *self = (struct nrc_node_inject*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if (self != 0) {
        switch (self->state) {
        case NRC_N_INJECT_S_INVALID:
            result = nrc_timer_init();

            if (result == NRC_R_OK) {
                s32_t period_s = 0;
                result = nrc_cfg_get_int(self->hdr.cfg_id, "repeat", &period_s);
                assert(OK(result));
                self->period_ms = period_s * 1000;
                result = nrc_cfg_get_str(self->hdr.cfg_id, "topic", &self->topic);
                assert(OK(result));
                result = nrc_cfg_get_str(self->hdr.cfg_id, "payload", &self->payload_string);
                assert(OK(result));

                self->payload_type = NRC_N_INJECT_TYPE_TIME;
                self->prio = 16;
                
                for (s32_t i = 0; i < NRC_N_INJECT_MAX_WIRES && OK(result); i++) {
                    s8_t* wire = NULL;
                    result = nrc_cfg_get_str_from_array(self->hdr.cfg_id, "wires", i, &wire);
                    if (OK(result)) {
                        self->wire[i] = nrc_os_node_get(wire);
                    }
                }

                self->state = NRC_N_INJECT_S_INITIALISED;

                result = NRC_R_OK;

                NRC_LOGD("inject", "initialised");
            }
            break;

        default:
            NRC_LOGD("inject", "init: invalid state %d", self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_inject_deinit(nrc_node_t self)
{
    s32_t result = NRC_R_OK;

    return result;
}

static s32_t nrc_node_inject_start(nrc_node_t slf)
{
    struct nrc_node_inject  *self = (struct nrc_node_inject*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if (self != 0) {
        switch (self->state) {
        case NRC_N_INJECT_S_INITIALISED:
            self->timer_pars.node = self;
            self->timer_pars.evt = NRC_N_INJECT_EVT_TIMEOUT;
            self->timer_pars.prio = self->prio;

            if (self->period_ms) {
                result = nrc_timer_after(self->period_ms, &self->timer_pars);
            }
            else {
                result = NRC_R_OK;
            }

            if (result == NRC_R_OK) {
                self->state = NRC_N_INJECT_S_STARTED;

                NRC_LOGD("inject", "started");
            }
            break;

        default:
            NRC_LOGD("inject", "start: invalid state %d", self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_inject_stop(nrc_node_t slf)
{
    struct nrc_node_inject  *self = (struct nrc_node_inject*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if (self != 0) {    
        switch (self->state) {
        case NRC_N_INJECT_S_STARTED:
            nrc_timer_cancel(self->timer_pars.timer);

            self->state = NRC_N_INJECT_S_INITIALISED;
            result = NRC_R_OK;

            NRC_LOGD("inject", "stopped");
            break;

        default:
            NRC_LOGD("inject", "stop: invalid state %d", self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_inject_recv_msg(nrc_node_t slf, nrc_msg_t msg)
{
    s32_t result = NRC_R_ERROR;

    NRC_LOGD("inject", "Unexpected msg");

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

    if (self != NULL) {
        switch (self->state) {
        case NRC_N_INJECT_S_STARTED:
            if (event_mask == self->timer_pars.evt) {
                if (self->period_ms) {
                    result = nrc_timer_after(self->period_ms, &self->timer_pars);
                }
                // Allocate and send message
                NRC_LOGI(self->hdr.cfg_name, "recv_evt: timeout", event_mask);

                hdr = (struct nrc_msg_hdr*)nrc_os_msg_alloc(sizeof(struct nrc_msg_hdr));
                if (hdr != NULL) {
                    hdr->topic = self->topic;
                    hdr->type = NRC_MSG_TYPE_EMPTY;
                    result = nrc_os_send_msg_to(self->wire[0], hdr, 16);
                }

                result = NRC_R_OK;
            }
            else {
                NRC_LOGI(self->hdr.cfg_name, "recv_evt: invalid evt %d", event_mask);
            }
            break;

        default:
            NRC_LOGD("nrc_timer", "recv_evt: invalid state %d", self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

