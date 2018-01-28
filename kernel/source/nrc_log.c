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
#include <stdint.h>
#include <stdarg.h>
#include "nrc_types.h"
#include "nrc_log.h"

s8_t *level_to_string(nrc_log_level level)
{
    switch (level) {
        case NRC_LOG_ERROR: return "error";
        case NRC_LOG_WARNING: return "warning";
        case NRC_LOG_INFO: return "info";
        case NRC_LOG_DEBUG: return "debug";
        case NRC_LOG_VERBOSE: return "verbose";
        default: return "unknown";
    }
}

void nrc_log_print(const char* format, ...)
{
    va_list list;
    va_start(list, format);
    nrc_port_vprintf(format, list);
    va_end(list);
}

void nrc_log_write(nrc_log_level level, const char* tag, const char* format, ...)
{
    s32_t d, h, m, s, ms;
    s32_t time = (s32_t)nrc_port_timer_get_time_ms();
    d = time / (1000*60*60*24);
    time -= d * (1000 * 60 * 60 * 24);
    h = time / (1000 * 60 * 60);
    time -= h * (1000 * 60 * 60);
    m = time / (1000 * 60);
    time -= m * (1000 * 60);
    s = time / (1000);
    time -= s * 1000;
    ms = time;

    nrc_log_print("\n\r%d:%02d:%02d:%02d:%03d - [%s]:[%s] ", d, h, m, s, ms, level_to_string(level), tag);	

    va_list list;
    va_start(list, format);
    nrc_port_vprintf(format, list);
    va_end(list);
}


