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

extern void nrc_node_inject_register(void);
extern void nrc_node_debug_register(void);

/** @brief Registers node types supported by win32 port
*
*  @return None
*/
void nrc_port_register_nodes(void)
{
    nrc_node_inject_register();
    nrc_node_debug_register();
}
