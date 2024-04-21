#ifndef _PROC_H_
#define _PROC_H_

#include "riscv.h"

// ! add for lab2_challenge2
typedef struct mem_block {
  uint64 cap; // mem_block size
  uint64 next; //
}mem_block;

typedef struct trapframe_t {
  // space to store context (all common registers)
  /* offset:0   */ riscv_regs regs;

  // process's "user kernel" stack
  /* offset:248 */ uint64 kernel_sp;
  // pointer to smode_trap_handler
  /* offset:256 */ uint64 kernel_trap;
  // saved user process counter
  /* offset:264 */ uint64 epc;

  // kernel page table. added @lab2_1
  /* offset:272 */ uint64 kernel_satp;
}trapframe;

// the extremely simple definition of process, used for begining labs of PKE
typedef struct process_t {
  // pointing to the stack used in trap handling.
  uint64 kstack;
  // user page table
  pagetable_t pagetable;
  // trapframe storing the context of a (User mode) process.
  trapframe* trapframe;
  // ! add for lab2_challenge2
  uint64 mem_used; // used mem va
  uint64 mem_free; // free mem va
}process;

// switch to run user app
void switch_to(process*);

// current running process
extern process* current;

// address of the first free page in our simple heap. added @lab2_2
extern uint64 g_ufree_page;

// ! add for lab2_challenge2
void* better_alloc(uint64 n); 
void better_free(uint64 va); 

#endif