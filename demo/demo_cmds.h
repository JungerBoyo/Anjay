/*
 * Copyright 2017-2022 AVSystem <avsystem@avsystem.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DEMO_CMDS_H
#define DEMO_CMDS_H

typedef struct {
    struct anjay_demo_struct *demo;
    char cmd[];
} demo_command_invocation_t;

void demo_command_dispatch(const demo_command_invocation_t *invocation);

#endif
