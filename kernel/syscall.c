/*
 * contains the implementation of all syscalls.
 */

#include <stdint.h>
#include <errno.h>

#include "util/types.h"
#include "syscall.h"
#include "string.h"
#include "process.h"
#include "util/functions.h"
#include "elf.h"

#include "spike_interface/spike_utils.h"


extern elf_ctx g_elfloader;

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  sprint(buf);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
  sprint("User exit with code:%d.\n", code);
  // in lab1, PKE considers only one app (one process). 
  // therefore, shutdown the system when the app calls exit()
  shutdown(code);
}

// ! add for lab1_challenge1_backtrace
ssize_t sys_user_print_backtrace(int64 depth) {
  uint64 user_sp = current->trapframe->regs.sp + 16 + 8;
  int64 cur_depth = 0;
  for (uint64 cur_p = user_sp; cur_depth < depth; cur_p += 16) {
    uint64 ra = *(uint64 *) cur_p;
    if (ra == 0) break;
    uint64 tmp = 0;
    int symbol_idx = -1;
    for (int i = 0; i < g_elfloader.symbol_cnt; i++) {
      if (g_elfloader.symbols[i].st_info == STT_FUNC && g_elfloader.symbols[i].st_value < ra && g_elfloader.symbols[i].st_value > tmp) {
        tmp = g_elfloader.symbols[i].st_value;
        symbol_idx = i;
      }
    }
    if (symbol_idx != -1) {
      if (g_elfloader.symbols[symbol_idx].st_value >= 0x81000000 && g_elfloader.symbols[symbol_idx].st_value <= 0x81000000 + 2 + 20 * (depth - 1))
        sprint("%s\n", &g_elfloader.str_table[g_elfloader.symbols[symbol_idx].st_name]);
      else
        continue;
    } else {
      sprint("failed to backtrace symbol %lx\n", ra);
    }
    cur_depth++;
  }
  return 0;
}


//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
 
  switch (a0) {
    case SYS_user_print:
      return sys_user_print((const char*)a1, a2);
    case SYS_user_exit:
      return sys_user_exit(a1);
    // ! add for lab1_challenge1_backtrace
    case SYS_user_showbacktrace:
      return sys_user_print_backtrace(a1);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
