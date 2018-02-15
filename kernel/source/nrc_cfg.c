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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "nrc_types.h"
#include "nrc_port.h"
#include "nrc_cfg.h"
#include "nrc_log.h"
#include "nrc_factory.h"
#include "jsmn.h"

// TODO - MAX_TOKENS should not be hard coded
#define MAX_TOKENS  1024

enum nrc_cfg_state {
    NRC_CONFIG_S_INVALID = 0,
    NRC_CONFIG_S_INITIALISED
};

enum nrc_cfg_type
{
    NRC_CFG_E_STRING = 0,
    NRC_CFG_E_STRING_ARRAY,
    NRC_CFG_E_INT,
    NRC_CFG_E_INT_ARRAY
};

struct nrc_param
{
    struct nrc_param*           next;
    u32_t                       token_id;
    enum nrc_cfg_type           type;   // Remove - only strings will be stored in list
    s8_t*                       name;   // Use the name in the json confing to save memory    
    void*                       value;
};

struct nrc_param_array_str
{
    s32_t   size;
    s8_t*   value[];
};

struct nrc_param_array_int
{
    s32_t   size;
    s32_t   value[];
};

struct nrc_cfg_node
{
    struct nrc_cfg_node*        next;
    u32_t                       token_id;
    s8_t*                       type;
    s8_t*                       id;
    s8_t*                       name;
    struct nrc_param*           params; // List of parsed parameters
};

struct nrc_cfg_t
{
    const s8_t*                     json_data;           // json formatted flow configuration
    struct nrc_cfg_node*            nodes;               // list of all nodes
    jsmn_parser                     json_parser;
    jsmntok_t                       json_tokens[1000];
    u32_t                           n_tokens;
    u32_t                           config_size;     
};

static const s8_t *_tag = "config";
static struct nrc_cfg_t *_config = NULL;

static s32_t parse_nodes(nrc_cfg_t *config);
static s32_t parse_node(nrc_cfg_t *config, jsmntok_t t[], s32_t i);
static s32_t next_key(jsmntok_t t[], s32_t i);
static s32_t get_next_object(jsmntok_t t[], s32_t i, s32_t end);
static s32_t keycmp(nrc_cfg_t* config, jsmntok_t t, const s8_t* key);

static s32_t get_value_len(nrc_cfg_t* config, s32_t i);
static s32_t get_value(nrc_cfg_t* config, s32_t i, s8_t buf[]);
static const s8_t* get_value_ref(nrc_cfg_t* config, s32_t i);

static struct nrc_cfg_node* node_alloc(void);
static void node_add(nrc_cfg_t *config, struct nrc_cfg_node* node);
static struct nrc_cfg_node* get_node_by_id(nrc_cfg_t* config, const s8_t *cfg_id);
static s32_t get_node_cfg_value_by_name(nrc_cfg_t* config, struct nrc_cfg_node* node, const s8_t *name, const s8_t **value, s32_t* value_len);
static struct nrc_param* get_node_cfg_from_list(nrc_cfg_t* config, struct nrc_cfg_node *node, const s8_t *cfg_param_name);
static struct nrc_param* add_node_str_cfg(nrc_cfg_t* config, struct nrc_cfg_node* node, const s8_t *name, s8_t *value, s32_t value_len);


s32_t nrc_cfg_init(void)
{
    return NRC_R_OK;
}

nrc_cfg_t* nrc_cfg_create(const u8_t *p_config, u32_t config_size)
{
    s32_t status = NRC_R_ERROR;
    nrc_cfg_t *config = NULL;

    if (p_config == NULL || config_size == 0) {
        return NULL;
    }
    
    config = (nrc_cfg_t *)nrc_port_heap_alloc(sizeof(nrc_cfg_t));
    memset(config, 0, sizeof(nrc_cfg_t));

    config->json_data = p_config;
    config->config_size = config_size;

    jsmn_init(&config->json_parser);
    config->n_tokens = jsmn_parse(&config->json_parser, config->json_data, config->config_size, config->json_tokens, MAX_TOKENS);

    if (config->n_tokens > 0) {
        s32_t status = parse_nodes(config);
        if (status == NRC_R_OK) {
            NRC_LOGD(_tag, "Configuration parsed without errors");
        }
        else {
            NRC_LOGE(_tag, "Failed to parse nodes");
        }
    }
    else {
        status = NRC_R_ERROR;
        NRC_LOGE(_tag, "Configuration parsing error");
        if (config->n_tokens == JSMN_ERROR_NOMEM) {
            NRC_LOGE(_tag, "Not enough tokens were provided ");
        }
        else if (config->n_tokens == JSMN_ERROR_INVAL) {
            NRC_LOGE(_tag, "Invalid character inside JSON string");
        }
        else if (config->n_tokens == JSMN_ERROR_PART) {
            NRC_LOGE(_tag, "The string is not a full JSON packet, more bytes expected");
        }
    }

    if (!OK(status)) {
        //nrc_cfg_destroy(config); //TODO
    }

    return config;
}

s32_t nrc_cfg_set_active(nrc_cfg_t* current_config)
{
    _config = current_config;
    return NRC_R_OK;
}

static s32_t next_key(jsmntok_t t[], s32_t i)
{
    return i + 1 + t[i].size;
}

static s32_t get_value_len(nrc_cfg_t* config, s32_t i)
{
    jsmntok_t* t = config->json_tokens;
    return t[i + 1].end - t[i + 1].start;
}

static const s8_t* get_value_ref(nrc_cfg_t* config, s32_t i)
{
    jsmntok_t* t = config->json_tokens;
    return &config->json_data[t[i + 1].start];
}

static s32_t get_value(nrc_cfg_t* config, s32_t i, s8_t buf[])
{
    jsmntok_t* t = config->json_tokens;
    memcpy(buf, config->json_data + t[i + 1].start, t[i + 1].end - t[i + 1].start);
    return  t[i + 1].end - t[i + 1].start;
}

static s32_t get_array_size(nrc_cfg_t* config, s32_t i)
{
    s32_t status = NRC_R_ERROR;
    jsmntok_t* t = config->json_tokens;
    s32_t end = t[i].end;
    s32_t size = 0;

    if ((t[i + 1].type == JSMN_ARRAY) && (t[i + 2].type == JSMN_ARRAY)) {
        end = t[i + 2].end;

        while (t[i + 3 + size].end < end && t[i + 3 + size].type == JSMN_STRING) {
            size++;
        }
    }
    return size;
}

static s32_t keycmp(nrc_cfg_t* config, jsmntok_t t, const s8_t* key)
{
    if (memcmp(key, &config->json_data[t.start], t.end - t.start) == 0) {
        return NRC_R_OK;
    }
    else {
        return NRC_R_ERROR;
    }
}

static struct nrc_cfg_node* node_alloc()
{
    struct nrc_cfg_node* node = (struct nrc_cfg_node*)nrc_port_heap_alloc(sizeof(struct nrc_cfg_node));
    if (node) {
        memset(node, 0, sizeof(struct nrc_cfg_node));
    }
    return node;
}

static void node_add(nrc_cfg_t *config, struct nrc_cfg_node* node)
{
    if (config->nodes == NULL) {
        config->nodes = node;
    }
    else {
        struct nrc_cfg_node* node_list = config->nodes;
        while (node_list->next != NULL) {
            node_list = node_list->next;
        }
        node_list->next = node;
    }
}

static s32_t parse_node(nrc_cfg_t *config, jsmntok_t t[], s32_t i)
{
    s32_t node_token = i;
    s32_t type_token = 0;
    s32_t id_token = 0;
    s32_t name_token = 0;
    s32_t end = t[i].end;
    
    i++;
    while ((t[i].end < end) && (t[i].type != JSMN_UNDEFINED)) {
        if (keycmp(config, t[i], "type") == NRC_R_OK) {
            type_token = i;
        }
        else if (keycmp(config, t[i], "id") == NRC_R_OK) {
            id_token = i;
        }
        else if (keycmp(config, t[i], "name") == NRC_R_OK) {
            name_token = i;
        }
        i = next_key(t, i);
    }

    if (type_token && id_token) {
        struct nrc_cfg_node* node = node_alloc();
        if (node) {
            s32_t len = 0;

            len = get_value_len(config, type_token);
            node->type = (s8_t*)nrc_port_heap_alloc(len+1);
            get_value(config, type_token, node->type);
            node->type[len] = 0;

            len = get_value_len(config, id_token);
            node->id = (s8_t*)nrc_port_heap_alloc(len + 1);
            get_value(config, id_token, node->id);
            node->id[len] = 0;

            if (name_token) {
                len = get_value_len(config, name_token);
                node->name = (s8_t*)nrc_port_heap_alloc(len + 1);
                get_value(config, name_token, node->name);
                node->name[len] = 0;
            }
            else {
                node->name = NULL;
            }
            node->token_id = node_token;

            node_add(config, node);
        }
    }

    return i;
}

static s32_t get_next_object(jsmntok_t t[], s32_t i, s32_t end)
{
    while (t[i].type != JSMN_OBJECT && t[i].type != JSMN_UNDEFINED && t[i].end < end) {
        i++;
    }
    return i;
}

static s32_t parse_nodes(nrc_cfg_t *config)
{
    s32_t status = NRC_R_OK;

    if (config->json_tokens[0].type != JSMN_ARRAY) {
        return NRC_R_INVALID_IN_PARAM;
    }

    s32_t end = config->json_tokens[0].end;
    jsmntok_t* t = config->json_tokens;
    s32_t next = 1;
    do
    {
        next = parse_node(config, t, next);
        next = get_next_object(t, next, end);
    } while (t[next].type != JSMN_UNDEFINED);

    return status;
}

s32_t nrc_cfg_deinit(void) 
{
    //if (config->json_tokens != NULL) {
    //    nrc_port_heap_free(config->json_tokens);
    //}
    memset(&_config, 0, sizeof(_config));
    return NRC_R_OK;
}

s32_t nrc_cfg_get_node(u32_t index, const s8_t **cfg_type, const s8_t **cfg_id, const s8_t **cfg_name)
{
    s32_t status = NRC_R_ERROR;
    struct nrc_cfg_node* node = _config->nodes;
    for (u32_t i = 0; i < index && node != NULL; i++) {
        node = node->next;
    }
    if (node != NULL) {
        status = NRC_R_OK;
        *cfg_type = node->type;
        *cfg_id = node->id;
        *cfg_name = node->name;
    }
    return status;
}

static struct nrc_cfg_node* get_node_by_id(nrc_cfg_t* config, const s8_t *cfg_id)
{
    assert(config);

    struct nrc_cfg_node* node = config->nodes;
    while (node != NULL) {
        if (strcmp(node->id, cfg_id) == 0) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

static s32_t get_node_cfg_value_by_name(nrc_cfg_t* config, struct nrc_cfg_node* node, const s8_t *name, const s8_t **value, s32_t* value_len)
{
    s32_t status = NRC_R_ERROR;
    jsmntok_t* t = config->json_tokens; 
    s32_t end = t[node->token_id].end;
    s32_t i = node->token_id;

    assert(node);
    assert(name);
    i++;
    while ((t[i].end < end) && (t[i].type != JSMN_UNDEFINED)) {
        if (keycmp(config, t[i], name) == NRC_R_OK) {
            *value = get_value_ref(config, i);
            *value_len = get_value_len(config, i);
            status = NRC_R_OK;
            break;
        }
        i = next_key(t, i);
    }
    return status;
}


static s32_t get_node_cfg_array_str_by_name(nrc_cfg_t* config, struct nrc_cfg_node* node, const s8_t *name, struct nrc_param_array_str** array_str)
{
    s32_t status = NRC_R_ERROR;
    jsmntok_t* t = config->json_tokens;
    s32_t end = t[node->token_id].end;
    s32_t i = node->token_id;
    *array_str = NULL;

    assert(node);
    assert(name);
    i++;
    while ((t[i].end < end) && (t[i].type != JSMN_UNDEFINED)) {
        if (keycmp(config, t[i], name) == NRC_R_OK) {
            s32_t size = get_array_size(config, i);
            if (size > 0) {
                struct nrc_param_array_str* new_array_str;
                new_array_str = (struct nrc_param_array_str*)nrc_port_heap_alloc(sizeof(struct nrc_param_array_str) + sizeof(s8_t*)*size);
                assert(new_array_str);
                new_array_str->size = size;
                for (int k = 0; k < size; k++) {
                    s32_t value_token = i + 3 + k; // todo - fix magic
                    s32_t str_len = t[value_token].end - t[value_token].start;
                    new_array_str->value[k] = nrc_port_heap_alloc(str_len + 1);
                    assert(new_array_str->value[k]);
                    memcpy(new_array_str->value[k], &config->json_data[t[value_token].start], str_len);
                    new_array_str->value[k][str_len] = '\0';
                }
                *array_str = new_array_str;
                status = NRC_R_OK;
            }
            break;
        }
        i = next_key(t, i);
    }
    return status;
}


static struct nrc_param* get_node_cfg_from_list(nrc_cfg_t* config, struct nrc_cfg_node *node, const s8_t *name)
{
    s32_t status = NRC_R_ERROR;

    assert(config);
    assert(node);

    struct nrc_param* param = node->params;

    while (param != NULL) {
        if (strcmp(param->name, name) == 0) {
            break;
        }
        param = param->next;
    }    
    return param;
}

static struct nrc_param* add_node_str_cfg(nrc_cfg_t* config, struct nrc_cfg_node* node, const s8_t *name, s8_t *value, s32_t value_len)
{
    assert(config);
    assert(node);

    struct nrc_param* new_param = (struct nrc_param*)nrc_port_heap_alloc(sizeof(struct nrc_param));
    new_param->type = NRC_CFG_E_STRING;
    new_param->name = (s8_t*)nrc_port_heap_alloc((u32_t)strlen(name) + 1);
    memcpy(new_param->name, name, strlen(name) + 1);
    new_param->value = (s8_t*)nrc_port_heap_alloc(value_len + 1);
    memcpy(new_param->value, value, value_len);
    ((s8_t*)new_param->value)[value_len] = '\0';
    new_param->next = NULL;

    if (node->params == NULL) {
        node->params = new_param;
    }
    else {
        struct nrc_param* end_param = node->params;
        while (end_param->next != NULL) {
            end_param = end_param->next;
        }
        end_param->next = new_param;
    }

    return new_param;
}

static struct nrc_param* add_node_int_cfg(nrc_cfg_t* config, struct nrc_cfg_node* node, const s8_t *name, s8_t *value, s32_t value_len)
{
    assert(config);
    assert(node); 

    struct nrc_param* new_param = (struct nrc_param*)nrc_port_heap_alloc(sizeof(struct nrc_param));
    new_param->type = NRC_CFG_E_INT;
    new_param->name = (s8_t*)nrc_port_heap_alloc((u32_t)strlen(name) + 1);
    memcpy(new_param->name, name, strlen(name) + 1);
    new_param->value = (s8_t*)nrc_port_heap_alloc(sizeof(s32_t));

    s8_t* value_string = (s8_t*)nrc_port_heap_alloc(value_len + 1);
    memcpy(value_string, value, value_len);
    value_string[value_len] = '\0';
    *(s32_t*)new_param->value = atoi(value_string);
    nrc_port_heap_free(value_string);
    
    new_param->next = NULL;
    
    if (node->params == NULL) {
        node->params = new_param;
    }
    else {
        struct nrc_param* end_param = node->params;
        while (end_param->next != NULL) {
            end_param = end_param->next;
        }
        end_param->next = new_param;
    }
    
    return new_param;
}


static struct nrc_param* add_node_str_array_cfg(nrc_cfg_t* config, struct nrc_cfg_node* node, const  s8_t *name, struct nrc_param_array_str* array_str)
{
    assert(config);
    assert(node);

    struct nrc_param* new_param = (struct nrc_param*)nrc_port_heap_alloc(sizeof(struct nrc_param));
    new_param->type = NRC_CFG_E_STRING_ARRAY;
    new_param->name = (s8_t*)nrc_port_heap_alloc((u32_t)strlen(name) + 1);
    memcpy(new_param->name, name, strlen(name) + 1);
    new_param->value = array_str;

    new_param->next = NULL;

    if (node->params == NULL) {
        node->params = new_param;
    }
    else {
        struct nrc_param* end_param = node->params;
        while (end_param->next != NULL) {
            end_param = end_param->next;
        }
        end_param->next = new_param;
    }

    return new_param;
}

s32_t nrc_cfg_get_str(const s8_t *cfg_id, const s8_t *cfg_param_name, const s8_t **str)
{
    s32_t status = NRC_R_NOT_FOUND;
    s8_t *value = NULL;
    s32_t value_len = 0;
    struct nrc_cfg_node* node = NULL;
    struct nrc_param*  param = NULL;
   
    assert(_config);

    node = get_node_by_id(_config, cfg_id);
    if (node == NULL) {
        return status;
    }

    param = get_node_cfg_from_list(_config, node, cfg_param_name);
    if (param == NULL) {
        status = get_node_cfg_value_by_name(_config, node, cfg_param_name, &value, &value_len);
        if (OK(status)) {
            param = add_node_str_cfg(_config, node, cfg_param_name, value, value_len);
            assert(param);
        }
    }

    if (param) {
        *str = (s8_t*)param->value;
        status = NRC_R_OK;
    }
    return status; 
}

s32_t nrc_cfg_get_int(const s8_t *cfg_id, const s8_t *cfg_param_name, s32_t *int_value)
{
    s32_t status = NRC_R_NOT_FOUND;
    s8_t *value = NULL;
    s32_t value_len = 0;
    struct nrc_cfg_node* node = NULL;
    struct nrc_param* param = NULL;

    assert(_config);

    node = get_node_by_id(_config, cfg_id);
    if (node == NULL) {
        return status;
    }

    param = get_node_cfg_from_list(_config, node, cfg_param_name);
    if (param == NULL) {
        status = get_node_cfg_value_by_name(_config, node, cfg_param_name, &value, &value_len);
        if (OK(status)) {
            param = add_node_int_cfg(_config, node, cfg_param_name, value, value_len);
            assert(param);
        }
    }

    if (param) {
        *int_value = *(s32_t*)param->value;
        status = NRC_R_OK;
    }
    return status;
}
s32_t nrc_cfg_get_str_from_array(const s8_t *cfg_id, const s8_t *cfg_arr_name, u8_t index, const s8_t **str)
{ 
    s32_t status = NRC_R_NOT_FOUND;
    s8_t *value = NULL;
    s32_t value_len = 0;
    struct nrc_cfg_node* node = NULL;
    struct nrc_param*  param = NULL;

    assert(_config);

    node = get_node_by_id(_config, cfg_id);
    if (node == NULL) {
        return status;
    }

    param = get_node_cfg_from_list(_config, node, cfg_arr_name);
    if (param == NULL) {
        struct nrc_param_array_str* array_str = NULL;
        status = get_node_cfg_array_str_by_name(_config, node, cfg_arr_name, &array_str);
        if (OK(status) && array_str != NULL) {
            param = add_node_str_array_cfg(_config, node, cfg_arr_name, array_str);
            assert(param);
        }
    }

    if (param ) {
        if (index < ((struct nrc_param_array_str*)param->value)->size) {
            *str = ((struct nrc_param_array_str*)param->value)->value[index];
            status = NRC_R_OK;
        }
        else {
            status = NRC_R_ERROR;
        }
    }
    return status;
}

s32_t nrc_cfg_get_int_from_array(const s8_t *cfg_type, const s8_t *cfg_id, const s8_t *cfg_arr_name, u8_t index, s32_t *value)
{ 
    return NRC_R_ERROR; 
}

