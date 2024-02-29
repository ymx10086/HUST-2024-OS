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
#include "pmm.h"
#include "vmm.h"
#include "spike_interface/spike_utils.h"

#include "sync_utils.h"

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  // buf is now an address in user space of the given app's user stack,
  // so we have to transfer it into phisical address (kernel is running in direct mapping).
  assert( current[read_tp()] );
  char* pa = (char*)user_va_to_pa((pagetable_t)(current[read_tp()]->pagetable), (void*)buf);
  sprint(pa);
  return 0;
}

static int shutdown_mutex = 0;

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
  sprint("hartid = %d: User exit with code:%d.\n", read_tp(), code);
  // in lab1, PKE considers only one app (one process). 
  // therefore, shutdown the system when the app calls exit()

  // ! add for lab1_challenge3
  sync_barrier(&shutdown_mutex, NCPU);
  if(!read_tp()){
    sprint("hartid = %d: shutdown with code:%d.\n", read_tp(), code);
    shutdown(code);
  }
  return 0;

}

//
// maybe, the simplest implementation of malloc in the world ... added @lab2_2
//
uint64 sys_user_allocate_page() {

  void* pa = alloc_page();
  uint64 va = g_ufree_page[read_tp()];
  g_ufree_page[read_tp()] += PGSIZE;
  user_vm_map((pagetable_t)current[read_tp()]->pagetable, va, PGSIZE, (uint64)pa,
         prot_to_type(PROT_WRITE | PROT_READ, 1));
  sprint("hartid = %lld: vaddr 0x%x is mapped to paddr 0x%x\n", read_tp(), va, pa);
  return va;
}

//
// reclaim a page, indicated by "va". added @lab2_2
//
uint64 sys_user_free_page(uint64 va) {
  uint64 hartid = read_tp();
  user_vm_unmap((pagetable_t)current[hartid]->pagetable, va, PGSIZE, 1);
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
    // added @lab2_2
    case SYS_user_allocate_page:
      return sys_user_allocate_page();
    case SYS_user_free_page:
      return sys_user_free_page(a1);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
