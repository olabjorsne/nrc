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


//#include <stdio.h>

#include "nrc_types.h"
#include "nrc_port.h"
#include "nrc_os.h"
#include "nrc_cfg.h"
#include "nrc_host.h"
#include "test_port.h"
#include "nrc_node_factory.h"
#include "nrc_log.h"

// TODO: Do we need this?
extern void nrc_node_inject_register(void);
extern void nrc_node_debug_register(void);
extern void nrc_node_serial_in_register(void);

const s8_t  *_tag = "main";
s8_t        *flows_file = NULL;

int nrc_main(int argc, char** argv)
{
    bool_t ok = FALSE;

    NRC_LOGI(_tag, "loading...");
    NRC_LOGI(_tag, "nrc version : \tv0.01");
    NRC_LOGI(_tag, "Target      : \twin32");

    // Extract configuration file (if any)
    if (argc > 1) {
        flows_file = argv[1];
    }

    // First, nrc_os must be initialized
    nrc_os_init();

    // Register all nodes that may be configured
    nrc_node_inject_register();
    nrc_node_debug_register();
    nrc_node_serial_in_register();

    // Start kernal nodes running at power on
    nrc_cfg_init();
    nrc_host_init();

    nrc_host_start();
    nrc_os_start(FALSE);

#if 0
    ok = test_all();
#endif

#if 0
#include "test_cbuf.h"
    ok = test_cbuf_read_write();
    ok = test_cbuf_read_write_buf();
#endif

#if 0
#include "test_port_uart_echo.h"
    ok = test_port_uart_echo(3);
#endif

    return 0;
 }