#pragma once

/*
  Copyright 2013 Google LLC All rights reserved.
  Copyright 2020 Keegan S. <keegan@sdf.org>

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at:

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <frida-gum.h>

#define FORKSRV_FD 198
#define SHM_ENV_VAR "__AFL_SHM_ID"
#define MAP_SIZE_POW2 16
#define MAP_SIZE (1 << MAP_SIZE_POW2)

extern int is_persistent;

void afl_setup(void);
void afl_start_forkserver(void);
int __afl_persistent_loop(unsigned int max_cnt);

__attribute__ ((always_inline))
static inline void afl_maybe_log(guint64 current_pc) {
    extern unsigned int afl_instr_rms;
    extern uint8_t* afl_area_ptr;
    extern __thread uint64_t __afl_prev_loc;

    current_pc = (current_pc >> 4) ^ (current_pc << 8);
    current_pc &= MAP_SIZE - 1;

    if (current_pc >= afl_instr_rms) return;

    afl_area_ptr[current_pc ^ __afl_prev_loc]++;
    __afl_prev_loc = current_pc >> 1;
}
