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

#include <Windows.h>
//#include <stdio.h>

#include "nrc_types.h"
#include "nrc_port.h"
#include "nrc_os.h"
#include "test_port.h"
#include "nrc_node_factory.h"
#include "nrc_log.h"

const s8_t *TAG = "main";

int main(void)
{
    bool_t ok = FALSE;

    NRC_LOGI(TAG, "nrc version : v0.01");
    NRC_LOGI(TAG, "target : win32");
    NRC_LOGI(TAG, "loading...");

    nrc_os_init();

#if 0
    struct nrc_os_register_node_pars    npars;
    struct nrc_node_factory_pars        fpars;
    nrc_node_t                          inject;
    nrc_node_t                          debug;

    fpars.cfg_id = "234.567";
    fpars.cfg_name = "my debug";
    fpars.cfg_type = "debug";
    debug = nrc_factory_create_debug(&fpars);

    npars.api = fpars.api;
    npars.cfg_id = fpars.cfg_id;

    nrc_os_node_register(TRUE, debug, npars);

    nrc_os_start(TRUE);

    fpars.cfg_id = "123.456";
    fpars.cfg_name = "my inject";
    fpars.cfg_type = "inject";
    inject = nrc_factory_create_inject(&fpars);

    npars.api = fpars.api;
    npars.cfg_id = fpars.cfg_id;

    nrc_os_node_register(FALSE, inject, npars);



#endif

    nrc_os_start(FALSE);

#if 0
    ok = test_all();
#endif

    while (1) {
        //NRC_LOGD(TAG, "sleep 1s");
        Sleep(1000);
    }
}