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

#include "instr.h"

#include <stdint.h>
#include <sys/shm.h>
#include <unistd.h>

#ifndef __APPLE__
#include <sys/wait.h>
#endif

uint8_t  __afl_area_initial[MAP_SIZE];
uint8_t* afl_area_ptr = __afl_area_initial;
__thread uint32_t __afl_prev_loc;
unsigned int afl_instr_rms = MAP_SIZE;

int is_persistent = 0;

void afl_start_forkserver(void) {
  static uint8_t tmp[4];
  int32_t child_pid;

  uint8_t  child_stopped = 0;

  /* Phone home and tell the parent that we're OK. If parent isn't there,
     assume we're not running in forkserver mode and just execute program. */

  if (write(FORKSRV_FD + 1, tmp, 4) != 4) return;

  while (1) {

    uint32_t was_killed;
    int status;

    /* Wait for parent by reading from the pipe. Abort if read fails. */

    if (read(FORKSRV_FD, &was_killed, 4) != 4) _exit(1);

    /* If we stopped the child in persistent mode, but there was a race
       condition and afl-fuzz already issued SIGKILL, write off the old
       process. */

    if (child_stopped && was_killed) {
      child_stopped = 0;
      if (waitpid(child_pid, &status, 0) < 0) _exit(1);
    }

    if (!child_stopped) {

      /* Once woken up, create a clone of our process. */

      child_pid = fork();
      if (child_pid < 0) _exit(1);

      /* In child process: close fds, resume execution. */

      if (!child_pid) {

        close(FORKSRV_FD);
        close(FORKSRV_FD + 1);
        return;
  
      }

    } else {

      /* Special handling for persistent mode: if the child is alive but
         currently stopped, simply restart it with SIGCONT. */

      kill(child_pid, SIGCONT);
      child_stopped = 0;

    }

    /* In parent process: write PID to pipe, then wait for child. */

    if (write(FORKSRV_FD + 1, &child_pid, 4) != 4) _exit(1);

    if (waitpid(child_pid, &status, is_persistent ? WUNTRACED : 0) < 0)
      _exit(1);

    /* In persistent mode, the child stops itself with SIGSTOP to indicate
       a successful run. In this case, we want to wake it up without forking
       again. */

    if (WIFSTOPPED(status)) child_stopped = 1;

    /* Relay wait status to pipe, then loop back. */

    if (write(FORKSRV_FD + 1, &status, 4) != 4) _exit(1);

  }

}

int __afl_persistent_loop(unsigned int max_cnt) {
  static uint8_t  first_pass = 1;
  static uint32_t cycle_cnt;

  if (first_pass) {

    /* Make sure that every iteration of __AFL_LOOP() starts with a clean slate.
       On subsequent calls, the parent will take care of that, but on the first
       iteration, it's our job to erase any trace of whatever happened
       before the loop. */

    if (is_persistent) {

      memset(afl_area_ptr, 0, MAP_SIZE);
      afl_area_ptr[0] = 1;
      __afl_prev_loc = 0;
    }

    cycle_cnt  = max_cnt;
    first_pass = 0;
    return 1;

  }

  if (is_persistent) {

    if (--cycle_cnt) {

      raise(SIGSTOP);

      afl_area_ptr[0] = 1;
      __afl_prev_loc = 0;

      return 1;

    } else {

      /* When exiting __AFL_LOOP(), make sure that the subsequent code that
         follows the loop is not traced. We do that by pivoting back to the
         dummy output region. */

      afl_area_ptr = __afl_area_initial;

    }

  }

  return 0;

}

void afl_setup(void) {
    char* id_str = getenv(SHM_ENV_VAR);
    char* instr_r = getenv("AFL_INST_RATIO");

    if (instr_r) {
        unsigned int r = atoi(instr_r);
        if (r > 100) r = 100;
        if (!r) r = 1;

        afl_instr_rms = MAP_SIZE * r / 100;
    }

    if (id_str) {
        int shm_id = atoi(id_str);
        afl_area_ptr = shmat(shm_id, NULL, 0);

        if (afl_area_ptr == (void*) -1) abort();

        if (instr_r) afl_area_ptr[0] = 1;
    }

    // TODO(meme) check AFL_INST_LIBS
}