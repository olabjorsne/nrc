#include "nrc_types.h"
#include "nrc_node.h"
#include "nrc_cfg.h"
#include "nrc_os.h"
#include "nrc_log.h"
#include "nrc_factory.h"
#include "nrc_assert.h"
#include <string.h>

#define NRC_N_DEBUG_TYPE    (0x927365A1)

enum nrc_node_debug_state {
    NRC_N_DEBUG_S_INVALID = 0,
    NRC_N_DEBUG_S_CREATED,
    NRC_N_DEBUG_S_INITIALISED,
    NRC_N_DEBUG_S_STARTED,
    NRC_N_DEBUG_S_ERROR
};

struct nrc_node_debug {
    struct nrc_node_hdr         hdr;
    enum nrc_node_debug_state   state;
    u32_t                       type;
};

static nrc_node_t nrc_node_debug_create(struct nrc_node_factory_pars *pars);
static s32_t nrc_node_debug_init(nrc_node_t self);
static s32_t nrc_node_debug_deinit(nrc_node_t self);
static s32_t nrc_node_debug_start(nrc_node_t self);
static s32_t nrc_node_debug_stop(nrc_node_t self);
static s32_t nrc_node_debug_recv_msg(nrc_node_t self, nrc_msg_t msg);
static s32_t nrc_node_debug_recv_evt(nrc_node_t self, u32_t event_mask);

static s32_t log_msg(struct nrc_node_debug *self, nrc_msg_t msg);

const static s8_t*          _tag = "debug";
static struct nrc_node_api  _api;

void nrc_node_debug_register(void)
{
    s32_t status = nrc_factory_register_node_type("nrc-debug", nrc_node_debug_create);
    if (!OK(status)) {
        NRC_LOGE(_tag, "register: error %d", status);
    }
}

static nrc_node_t nrc_node_debug_create(struct nrc_node_factory_pars *pars)
{
    struct nrc_node_debug *node = NULL;

    if ((pars != NULL) && (strcmp("nrc-debug", pars->cfg_type) == 0)) {
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

            node->type = NRC_N_DEBUG_TYPE;
            node->state = NRC_N_DEBUG_S_CREATED;
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

static s32_t nrc_node_debug_init(nrc_node_t slf)
{
    struct nrc_node_debug  *self = (struct nrc_node_debug*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_DEBUG_TYPE)){
        switch (self->state) {
        case NRC_N_DEBUG_S_CREATED:
            // Nothing to init

            self->state = NRC_N_DEBUG_S_INITIALISED;
            result = NRC_R_OK;
            break;

        default:
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_debug_deinit(nrc_node_t slf)
{
    struct nrc_node_debug  *self = (struct nrc_node_debug*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_DEBUG_TYPE)) {
        switch (self->state) {
        case NRC_N_DEBUG_S_INITIALISED:
        case NRC_N_DEBUG_S_ERROR:
            // Nothing to free

            self->state = NRC_N_DEBUG_S_CREATED;
            result = NRC_R_OK;
            break;

        default:
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t nrc_node_debug_start(nrc_node_t slf)
{
    struct nrc_node_debug  *self = (struct nrc_node_debug*)slf;
    s32_t                   result = NRC_R_INVALID_IN_PARAM;

    if ((self != NULL) && (self->type == NRC_N_DEBUG_TYPE)) {
        switch (self->state) {
        case NRC_N_DEBUG_S_INITIALISED:

            self->state = NRC_N_DEBUG_S_STARTED;
            result = NRC_R_OK;
            break;

        default:
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

    if ((self != NULL) && (self->type == NRC_N_DEBUG_TYPE)) {
        switch (self->state) {
        case NRC_N_DEBUG_S_STARTED:
            self->state = NRC_N_DEBUG_S_INITIALISED;
            result = NRC_R_OK;
            break;

        default:
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

    if ((self != NULL) && (self->type == NRC_N_DEBUG_TYPE) && (msg != NULL)) {

        switch (self->state) {
        case NRC_N_DEBUG_S_STARTED:
            result = log_msg(self, msg);
            break;

        default:
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

    if ((self != NULL) && (self->type == NRC_N_DEBUG_TYPE)) {
        switch (self->state) {
        case NRC_N_DEBUG_S_STARTED:
            NRC_LOGW(_tag, "recv_evt(%s): unexpected evt %d", self->hdr.cfg_id, event_mask);
            result = NRC_R_OK;
            break;

        default:
            result = NRC_R_INVALID_STATE;
            break;
        }
    }

    return result;
}

static s32_t log_msg(struct nrc_node_debug *self, nrc_msg_t msg)
{
    s32_t               result = result = NRC_R_INVALID_IN_PARAM;
    struct nrc_msg_hdr  *hdr = (struct nrc_msg_hdr*)msg;

    NRC_ASSERT(self != NULL);
    NRC_ASSERT(self->type == NRC_N_DEBUG_TYPE);

    // TODO: Messages shall be sent to host

    while (hdr != NULL) {
        switch (hdr->type) {
        case NRC_MSG_TYPE_STRING:
        {
            struct nrc_msg_str *msg_str = (struct nrc_msg_str*)hdr;
            NRC_LOGI(_tag, "%s(%s): topic %s, string: %s", self->hdr.cfg_name, self->hdr.cfg_id, hdr->topic, msg_str->str);
            break;
        }
        case NRC_MSG_TYPE_INT:
        {
            struct nrc_msg_int *msg_int = (struct nrc_msg_int*)hdr;
            NRC_LOGI(_tag, "%s(%s): topic %s, int: %d", self->hdr.cfg_name, self->hdr.cfg_id, hdr->topic, msg_int->value);
            break;
        }
        case NRC_MSG_TYPE_DATA_AVAILABLE:
        {
            struct nrc_msg_data_available *msg_da = (struct nrc_msg_data_available*)hdr;
            NRC_LOGI(_tag, "%s(%s): topic %s, node 0x%X, read fcn: 0x%X", self->hdr.cfg_name, self->hdr.cfg_id, hdr->topic, msg_da->node, msg_da->read);
            break;
        }
        default:
        {
            NRC_LOGI(_tag, "%s(%s): topic %s", self->hdr.cfg_name, self->hdr.cfg_id, hdr->topic);
            break;
        }
        }
        hdr = hdr->next;
    }

    return result;
}
