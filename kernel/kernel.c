/*
 * Supervisor-mode startup codes
 */

#include "riscv.h"
#include "string.h"
#include "elf.h"
#include "process.h"

#include "spike_interface/spike_utils.h"
#include "sync_utils.h"
#include "spike_interface/atomic.h"

// process is a structure defined in kernel/process.h
process user_app[NCPU]; 
spinlock_t proc_lock;

//
// load the elf, and construct a "process" (with only a trapframe).
// load_bincode_from_host_elf is defined in elf.c
//
void load_user_program(process *proc) {
  // USER_TRAP_FRAME is a physical address defined in kernel/config.h
  int hartid = read_tp();
  if (hartid == 0) {
    proc->trapframe = (trapframe *)USER_TRAP_FRAME_ZERO;
    memset(proc->trapframe, 0, sizeof(trapframe));
    proc->kstack = USER_KSTACK_ZERO;
    proc->trapframe->regs.sp = USER_STACK_ZERO;
    proc->trapframe->regs.tp = hartid; // comment:我真的...
  }
  else {
    proc->trapframe = (trapframe *)USER_TRAP_FRAME_ONE;
    memset(proc->trapframe, 0, sizeof(trapframe));
    proc->kstack = USER_KSTACK_ONE;
    proc->trapframe->regs.sp = USER_STACK_ONE;
    proc->trapframe->regs.tp = hartid; // comment:我真的...
  }
  // load_bincode_from_host_elf() is defined in kernel/elf.c
  load_bincode_from_host_elf(proc);
}

//
// s_start: S-mode entry point of riscv-pke OS kernel.
//

extern spinlock_t root_lock; // 外部变量

int s_start(void) {
  int hartid = read_tp(); // 获取当前cpu的id
  sprint("hartid = %d: Enter supervisor mode...\n", hartid); // comment:修改打印信息 将cpu的id打印出
  // Note: we use direct (i.e., Bare mode) for memory mapping in lab1.
  // which means: Virtual Address = Physical Address
  // therefore, we need to set satp to be 0 for now. we will enable paging in lab2_x.
  // 
  // write_csr is a macro defined in kernel/riscv.h
  write_csr(satp, 0);

  // the application code (elf) is first loaded into memory, and then put into execution
  load_user_program(&user_app[hartid]);

  spinlock_unlock(&root_lock); // 释放锁

  sprint("hartid = %d: Switch to user mode...\n", hartid); // comment:修改打印信息 将cpu的id打印出

  // switch_to() is defined in kernel/process.c
  spinlock_lock(&proc_lock);
  switch_to(&user_app[hartid]);

  // we should never reach here.
  return 0;
}
