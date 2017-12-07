#include "nrc_types.h"
#include "nrc_node.h"
#include "nrc_cfg.h"
#include "nrc_os.h"
#include <string.h>
#include "nrc_timer.h"
#include "nrc_log.h"

#define NRC_N_INJECT_EVT_TIMEOUT    (1)

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
    nrc_node_id_t               id;

    s8_t                        *topic;
    enum nrc_node_inject_types  payload_type;
    s8_t                        *payload_string;
    u32_t                       period_ms;

    s8_t                        prio;
    struct nrc_timer_info       timer_info;
};

static s32_t nrc_node_inject_init(struct nrc_node_hdr *self, nrc_node_id_t id);
static s32_t nrc_node_inject_teardown(struct nrc_node_hdr *self);
static s32_t nrc_node_inject_start(struct nrc_node_hdr *self);
static s32_t nrc_node_inject_stop(struct nrc_node_hdr *self);
static s32_t nrc_node_inject_recv_msg(struct nrc_node_hdr *self, struct nrc_msg_hdr *msg);
static s32_t nrc_node_inject_recv_evt(struct nrc_node_hdr *self, u32_t event_mask);

static struct nrc_node_api _api;

struct nrc_node_hdr* nrc_factory_create_inject(
    const s8_t          *cfg_type,
    const s8_t          *cfg_id,
    const s8_t          *cfg_name,
    struct nrc_node_api **ptr_api)
{
    struct nrc_node_inject *node = (struct nrc_node_inject*)nrc_os_node_alloc(sizeof(struct nrc_node_inject));

    if (node != 0) {
        node->hdr.cfg_id = cfg_id;
        node->hdr.cfg_type = cfg_type;
        node->hdr.cfg_name = cfg_name;

        node->topic = 0;
        node->payload_type = NRC_N_INJECT_TYPE_NONE;
        node->payload_string = 0;
        node->period_ms = 0;

        if (ptr_api != 0) {
            memset(&_api, 0, sizeof(struct nrc_node_api));

            node->state = NRC_N_INJECT_S_INVALID;

            _api.init = nrc_node_inject_init;
            _api.teardown = nrc_node_inject_teardown;
            _api.start = nrc_node_inject_start;
            _api.stop = nrc_node_inject_stop;
            _api.recv_msg = nrc_node_inject_recv_msg;
            _api.recv_evt = nrc_node_inject_recv_evt;

            *ptr_api = &_api;
        }
    }

    return (struct nrc_node_hdr*)node;
}

static s32_t nrc_node_inject_init(struct nrc_node_hdr *hdr, nrc_node_id_t id)
{
    struct nrc_node_inject  *self = (struct nrc_node_inject*)hdr;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if (self != 0) {
        result = NRC_R_OK;

        if (self->state == NRC_N_INJECT_S_INVALID) {
            self->id = id;

            result = nrc_timer_init();

            if (result == NRC_R_OK) {
                //TODO: read configuration from nrc_cfg
                self->period_ms = 10000;
                self->payload_type = NRC_N_INJECT_TYPE_TIME;
                self->prio = 16;

                self->state = NRC_N_INJECT_S_INITIALISED;
            }
        }
    }

    return result;
}

static s32_t nrc_node_inject_teardown(struct nrc_node_hdr *self)
{
    s32_t result = NRC_R_NOT_SUPPORTED;

    return result;
}

static s32_t nrc_node_inject_start(struct nrc_node_hdr *hdr)
{
    struct nrc_node_inject  *self = (struct nrc_node_inject*)hdr;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if (self != 0) {
        result = NRC_R_OK;

        if (self->state = NRC_N_INJECT_S_INITIALISED) {
            self->timer_info.node_id = self->id;
            self->timer_info.evt = NRC_N_INJECT_EVT_TIMEOUT;
            self->timer_info.prio = self->prio;

            result = nrc_timer_after(self->period_ms, &self->timer_info);

            if (result == NRC_R_OK) {
                self->state = NRC_N_INJECT_S_STARTED;
            }
        }
    }

    return result;
}

static s32_t nrc_node_inject_stop(struct nrc_node_hdr *hdr)
{
    struct nrc_node_inject  *self = (struct nrc_node_inject*)hdr;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if (self != 0) {    
        switch (self->state) {
        case NRC_N_INJECT_S_INITIALISED:
            result = NRC_R_OK;
            break;

        case NRC_N_INJECT_S_STARTED:
            nrc_timer_cancel(self->timer_info.timer_id);

            result = NRC_R_OK;
            break;

        default:
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_inject_recv_msg(struct nrc_node_hdr *self, struct nrc_msg_hdr *msg)
{
    s32_t result = NRC_R_ERROR;

    NRC_LOGD("nrc_timer", "Unexpected msg");

    return result;
}

static s32_t nrc_node_inject_recv_evt(struct nrc_node_hdr *hdr, u32_t event_mask)
{
    struct nrc_node_inject  *self = (struct nrc_node_inject*)hdr;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if (self != 0) {
        switch (self->state) {
        case NRC_N_INJECT_S_STARTED:
            // Allocate and send message

            result = NRC_R_OK;
            break;

        default:
            NRC_LOGD("nrc_timer", "recv_evt: invalid state %d", self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

