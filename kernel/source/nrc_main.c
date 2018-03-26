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

#include "nrc_types.h"
#include "nrc_port.h"
#include "nrc_os.h"
#include "nrc_cfg.h"
#include "nrc_factory.h"
#include "nrc_log.h"
#include "nrc_assert.h"

#include "test_port.h"

// TODO: Do we need this?
extern void nrc_node_inject_register(void);
extern void nrc_node_debug_register(void);
extern void nrc_node_serial_in_register(void);
extern void nrc_node_serial_out_register(void);

static const s8_t   *_tag = "nrc_main";

int nrc_main(const char *cfg, unsigned int cfg_size)
{
    bool_t      ok = FALSE;
    const s8_t  *boot_cfg = NULL;
    u32_t       boot_cfg_size = 0;
    nrc_cfg_t*  flow_cfg = NULL;
    s32_t       result;

    NRC_LOGI(_tag, "loading...");
    NRC_LOGI(_tag, "nrc version : \tv0.01");
    NRC_LOGI(_tag, "Target      : \twin32");

    // Initialize core components
    result = nrc_port_init();
    NRC_ASSERT(OK(result));
    result = nrc_os_init();
    NRC_ASSERT(OK(result));
    result = nrc_cfg_init();
    NRC_ASSERT(OK(result));

    // Get boot flow configuration
    if ((cfg != NULL) && (cfg_size > 0)) {
        // Configuration file as nrc_main input argument
        boot_cfg = cfg;
        boot_cfg_size = cfg_size;
    }
    else {
        // TODO: Start compiled boot flow configuration
        result = NRC_R_NOT_SUPPORTED;
    }
    
    if (OK(result)) {
        nrc_port_register_nodes();

        // Install boot flow configuration in nrc_cfg
        flow_cfg = nrc_cfg_create(boot_cfg, boot_cfg_size);
        NRC_ASSERT(flow_cfg != NULL);

        // Start nrc_os with the boot flow configuration
        nrc_os_start(flow_cfg);
    }

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