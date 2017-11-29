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
#include <stdio.h>

#include "nrc_port.h"
#include "nrc_os.h"

int main(void)
{
	printf("nrc is about to start\n");

#include "nrc_port.h"
    nrc_port_init();
    nrc_os_init();
    nrc_os_start();

    while (1) {
        Sleep(1000);
    }
}