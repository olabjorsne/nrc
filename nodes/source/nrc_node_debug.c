#include "nrc_types.h"
#include "nrc_node.h"
#include "nrc_cfg.h"
#include "nrc_os.h"
#include "nrc_log.h"
#include "nrc_node_factory.h"
#include <string.h>

enum nrc_node_debug_state {
    NRC_N_DEBUG_S_INVALID,
    NRC_N_DEBUG_S_INITIALISED,
    NRC_N_DEBUG_S_STARTED
};

struct nrc_node_debug {
    struct nrc_node_hdr         hdr;
    enum nrc_node_debug_state   state;
};

static s32_t nrc_node_debug_init(nrc_node_t self);
static s32_t nrc_node_debug_deinit(nrc_node_t self);
static s32_t nrc_node_debug_start(nrc_node_t self);
static s32_t nrc_node_debug_stop(nrc_node_t self);
static s32_t nrc_node_debug_recv_msg(nrc_node_t self, nrc_msg_t msg);
static s32_t nrc_node_debug_recv_evt(nrc_node_t self, u32_t event_mask);

static s32_t log_msg(const s8_t *name, nrc_msg_t msg);

static struct nrc_node_api _api;

nrc_node_t nrc_factory_create_debug(struct nrc_node_factory_pars *pars)
{
    struct nrc_node_debug *node = NULL;

    if ((pars != NULL) && (strcmp("debug", pars->cfg_type) == 0)) {
        node = (struct nrc_node_debug*)nrc_os_node_alloc(sizeof(struct nrc_node_debug));

        if (node != NULL) {
            memset(node, 0, sizeof(struct nrc_node_debug));
            node->hdr.cfg_id = pars->cfg_id;
            node->hdr.cfg_type = pars->cfg_type;
            node->hdr.cfg_name = pars->cfg_name;

            memset(&_api, 0, sizeof(struct nrc_node_api));
            _api.init = nrc_node_debug_init;
            _api.deinit = nrc_node_debug_deinit;
            _api.start = nrc_node_debug_start;
            _api.stop = nrc_node_debug_stop;
            _api.recv_msg = nrc_node_debug_recv_msg;
            _api.recv_evt = nrc_node_debug_recv_evt;

            pars->api = &_api;

            node->state = NRC_N_DEBUG_S_INVALID;
        }
    }

    return node;
}

static s32_t nrc_node_debug_init(nrc_node_t slf)
{
    struct nrc_node_debug  *self = (struct nrc_node_debug*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if (self != 0) {
        switch (self->state) {
        case NRC_N_DEBUG_S_INVALID:
            //TODO:

            self->state = NRC_N_DEBUG_S_INITIALISED;
            result = NRC_R_OK;
            break;

        default:
            NRC_LOGD("debug", "init: invalid state %d", self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_debug_deinit(nrc_node_t self)
{
    s32_t result = NRC_R_OK;

    return result;
}

static s32_t nrc_node_debug_start(nrc_node_t slf)
{
    struct nrc_node_debug  *self = (struct nrc_node_debug*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if (self != 0) {
        switch (self->state) {
        case NRC_N_DEBUG_S_INITIALISED:

            self->state = NRC_N_DEBUG_S_STARTED;
            result = NRC_R_OK;
            break;

        default:
            NRC_LOGD("debug", "start: invalid state %d", self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_debug_stop(nrc_node_t slf)
{
    struct nrc_node_debug  *self = (struct nrc_node_debug*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if (self != 0) {    
        switch (self->state) {
        case NRC_N_DEBUG_S_STARTED:
            self->state = NRC_N_DEBUG_S_INITIALISED;
            result = NRC_R_OK;
            break;

        default:
            NRC_LOGD("debug", "stop: invalid state %d", self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_debug_recv_msg(nrc_node_t slf, nrc_msg_t msg)
{
    struct nrc_node_debug   *self = (struct nrc_node_debug*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (msg != NULL)) {

        switch (self->state) {
        case NRC_N_DEBUG_S_STARTED:
            result = log_msg(self->hdr.cfg_name, msg);
            break;
        default:
            NRC_LOGD("debug", "recv_msg: invalid state %d", self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    if (msg != NULL) {
        nrc_os_msg_free(msg);
    }

    return result;
}

static s32_t nrc_node_debug_recv_evt(nrc_node_t slf, u32_t event_mask)
{
    struct nrc_node_debug  *self = (struct nrc_node_debug*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if (self != NULL) {
        switch (self->state) {
        case NRC_N_DEBUG_S_STARTED:
            NRC_LOGD("debug", "recv_evt: unexpected event %d", event_mask);
            result = NRC_R_ERROR;
            break;

        default:
            NRC_LOGD("nrc_timer", "recv_evt: invalid state %d", self->state);
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t log_msg(const s8_t *name, nrc_msg_t msg)
{
    s32_t result = result = NRC_R_INVALID_IN_PARAM;
    struct nrc_msg_hdr *hdr = (struct nrc_msg_hdr*)msg;

    if (hdr != NULL) {
        switch (hdr->type) {
        case NRC_MSG_TYPE_STRING:
        {
            struct nrc_msg_str *msg_str = (struct nrc_msg_str*)hdr;
            NRC_LOGI(name, "topic: %s, string: %s", hdr->topic, msg_str->str);
            break;
        }
        case NRC_MSG_TYPE_INT:
        {
            struct nrc_msg_int *msg_int = (struct nrc_msg_int*)hdr;
            NRC_LOGI(name, "topic: %s, integer: %d", hdr->topic, msg_int->value);
            break;
        }
        case NRC_MSG_TYPE_DATA_AVAILABLE:
        {
            struct nrc_msg_data_available *msg_da = (struct nrc_msg_data_available*)hdr;
            NRC_LOGI(name, "topic: %s, read fcn: 0x%X", hdr->topic, msg_da->read);
            break;
        }
        default:
        {
            NRC_LOGI(name, "topic: %s", hdr->topic);
            break;
        }
        }
    }

    return result;
}
