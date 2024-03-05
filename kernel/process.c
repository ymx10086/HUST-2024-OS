/*
 * Utility functions for process management. 
 *
 * Note: in Lab1, only one process (i.e., our user application) exists. Therefore, 
 * PKE OS at this stage will set "current" to the loaded user application, and also
 * switch to the old "current" process after trap handling.
 */

#include "process.h"
#include "config.h"
#include "elf.h"
#include "memlayout.h"
#include "pmm.h"
#include "riscv.h"
#include "sched.h"
#include "spike_interface/spike_utils.h"
#include "strap.h"
#include "string.h"
#include "vmm.h"

//Two functions defined in kernel/usertrap.S
extern char smode_trap_vector[];
extern void return_to_user(trapframe *, uint64 satp);

// trap_sec_start points to the beginning of S-mode trap segment (i.e., the entry point
// of S-mode trap vector).
extern char trap_sec_start[];

// process pool. added @lab3_1
process procs[NPROC];

// current points to the currently running user-mode application.
process *current = NULL;

// points to the first free page in our simple heap. added @lab2_2
uint64 g_ufree_page = USER_FREE_ADDRESS_START;

//
// switch to a user-mode process
//
void switch_to(process *proc) {
  assert(proc);
  current = proc;

  // write the smode_trap_vector (64-bit func. address) defined in kernel/strap_vector.S
  // to the stvec privilege register, such that trap handler pointed by smode_trap_vector
  // will be triggered when an interrupt occurs in S mode.
  write_csr(stvec, (uint64) smode_trap_vector);

  // set up trapframe values (in process structure) that smode_trap_vector will need when
  // the process next re-enters the kernel.
  proc->trapframe->kernel_sp = proc->kstack;    // process's kernel stack
  proc->trapframe->kernel_satp = read_csr(satp);// kernel page table
  proc->trapframe->kernel_trap = (uint64) smode_trap_handler;

  // SSTATUS_SPP and SSTATUS_SPIE are defined in kernel/riscv.h
  // set S Previous Privilege mode (the SSTATUS_SPP bit in sstatus register) to User mode.
  unsigned long x = read_csr(sstatus);
  x &= ~SSTATUS_SPP;// clear SPP to 0 for user mode
  x |= SSTATUS_SPIE;// enable interrupts in user mode

  // write x back to 'sstatus' register to enable interrupts, and sret destination mode.
  write_csr(sstatus, x);

  // set S Exception Program Counter (sepc register) to the elf entry pc.
  write_csr(sepc, proc->trapframe->epc);

  // make user page table. macro MAKE_SATP is defined in kernel/riscv.h. added @lab2_1
  uint64 user_satp = MAKE_SATP(proc->pagetable);

  // return_to_user() is defined in kernel/strap_vector.S. switch to user mode with sret.
  // note, return_to_user takes two parameters @ and after lab2_1.
  return_to_user(proc->trapframe, user_satp);
}

//
// initialize process pool (the procs[] array). added @lab3_1
//
void init_proc_pool() {
  memset(procs, 0, sizeof(process) * NPROC);

  for (int i = 0; i < NPROC; ++i) {
    procs[i].status = FREE;
    procs[i].pid = i;
  }
}

//
// allocate an empty process, init its vm space. returns the pointer to
// process strcuture. added @lab3_1
//
process *alloc_process() {
  // locate the first usable process structure
  int i;

  for (i = 0; i < NPROC; i++)
    if (procs[i].status == FREE) break;

  if (i >= NPROC) {
    panic("cannot find any free process structure.\n");
    return 0;
  }

  // init proc[i]'s vm space
  procs[i].trapframe = (trapframe *) alloc_page();//trapframe, used to save context
  memset(procs[i].trapframe, 0, sizeof(trapframe));

  // page directory
  procs[i].pagetable = (pagetable_t) alloc_page();
  memset((void *) procs[i].pagetable, 0, PGSIZE);

  procs[i].kstack = (uint64) alloc_page() + PGSIZE;//user kernel stack top
  uint64 user_stack = (uint64) alloc_page();       //phisical address of user stack bottom
  procs[i].trapframe->regs.sp = USER_STACK_TOP;    //virtual address of user stack top

  // allocates a page to record memory regions (segments)
  procs[i].mapped_info = (mapped_region *) alloc_page();
  memset(procs[i].mapped_info, 0, PGSIZE);

  // map user stack in userspace
  user_vm_map((pagetable_t) procs[i].pagetable, USER_STACK_TOP - PGSIZE, PGSIZE,
              user_stack, prot_to_type(PROT_WRITE | PROT_READ, 1));
  procs[i].mapped_info[0].va = USER_STACK_TOP - PGSIZE;
  procs[i].mapped_info[0].npages = 1;
  procs[i].mapped_info[0].seg_type = STACK_SEGMENT;

  // map trapframe in user space (direct mapping as in kernel space).
  user_vm_map((pagetable_t) procs[i].pagetable, (uint64) procs[i].trapframe, PGSIZE,
              (uint64) procs[i].trapframe, prot_to_type(PROT_WRITE | PROT_READ, 0));
  procs[i].mapped_info[1].va = (uint64) procs[i].trapframe;
  procs[i].mapped_info[1].npages = 1;
  procs[i].mapped_info[1].seg_type = CONTEXT_SEGMENT;

  // map S-mode trap vector section in user space (direct mapping as in kernel space)
  // we assume that the size of usertrap.S is smaller than a page.
  user_vm_map((pagetable_t) procs[i].pagetable, (uint64) trap_sec_start, PGSIZE,
              (uint64) trap_sec_start, prot_to_type(PROT_READ | PROT_EXEC, 0));
  procs[i].mapped_info[2].va = (uint64) trap_sec_start;
  procs[i].mapped_info[2].npages = 1;
  procs[i].mapped_info[2].seg_type = SYSTEM_SEGMENT;

  sprint("in alloc_proc. user frame 0x%lx, user stack 0x%lx, user kstack 0x%lx \n",
         procs[i].trapframe, procs[i].trapframe->regs.sp, procs[i].kstack);

  // initialize files_struct
  procs[i].pfiles = init_proc_file_management();
  sprint("in alloc_proc. build proc_file_management successfully.\n");

  // return after initialization.
  return &procs[i];
}

//
// reclaim a process. added @lab3_1
//
int free_process(process *proc) {
  // we set the status to ZOMBIE, but cannot destruct its vm space immediately.
  // since proc can be current process, and its user kernel stack is currently in use!
  // but for proxy kernel, it (memory leaking) may NOT be a really serious issue,
  // as it is different from regular OS, which needs to run 7x24.
  proc->status = ZOMBIE;

  return 0;
}

//
// implements fork syscal in kernel. added @lab3_1
// basic idea here is to first allocate an empty process (child), then duplicate the
// context and data segments of parent process to the child, and lastly, map other
// segments (code, system) of the parent to child. the stack segment remains unchanged
// for the child.
//
int do_fork(process *parent) {
  sprint("will fork a child from parent %d.\n", parent->pid);
  process *child = alloc_process();

  for (int i = 0; i < parent->total_mapped_region; i++) {
    // browse parent's vm space, and copy its trapframe and data segments,
    // map its code segment.
    switch (parent->mapped_info[i].seg_type) {
      case CONTEXT_SEGMENT:
        *child->trapframe = *parent->trapframe;
        break;
      case STACK_SEGMENT:
        memcpy((void *) lookup_pa(child->pagetable, child->mapped_info[0].va),
               (void *) lookup_pa(parent->pagetable, parent->mapped_info[i].va), PGSIZE);
        break;
      case CODE_SEGMENT:
        // TODO (lab3_1): implment the mapping of child code segment to parent's
        // code segment.
        // hint: the virtual address mapping of code segment is tracked in mapped_info
        // page of parent's process structure. use the information in mapped_info to
        // retrieve the virtual to physical mapping of code segment.
        // after having the mapping information, just map the corresponding virtual
        // address region of child to the physical pages that actually store the code
        // segment of parent process.
        // DO NOT COPY THE PHYSICAL PAGES, JUST MAP THEM.
        // panic( "You need to implement the code segment mapping of child in lab3_1.\n" );

        for (int j = 0; j < parent->mapped_info[i].npages; j++) {
          uint64 pa_of_mapped_va = lookup_pa(parent->pagetable, parent->mapped_info[i].va + j * PGSIZE);
          // 建立父进程位于 pa_of_mapped_va 的代码段与子进程对应逻辑地址的映射
          map_pages(child->pagetable, parent->mapped_info[i].va + j * PGSIZE, PGSIZE, pa_of_mapped_va, prot_to_type(PROT_READ | PROT_EXEC, 1));
        }

        // after mapping, register the vm region (do not delete codes below!)
        child->mapped_info[child->total_mapped_region].va = parent->mapped_info[i].va;
        child->mapped_info[child->total_mapped_region].npages =
                parent->mapped_info[i].npages;
        child->mapped_info[child->total_mapped_region].seg_type = CODE_SEGMENT;
        child->total_mapped_region++;
        break;
    }
  }

  child->status = READY;
  child->trapframe->regs.a0 = 0;
  child->parent = parent;
  insert_to_ready_queue(child);

  return child->pid;
}

// 根据alloc_process函数改写
static void exec_clean_pagetable(pagetable_t page_dir) { // comment: pagetable_t是uint64* 其加法遵循指针加法
    int cnt = PGSIZE / sizeof(pte_t); // pte_t是int型
    int vaild_cnt = 0;
    int valid_and_writable_cnt = 0;
    for (int i = 0; i < cnt; i++) {
        pte_t* pte1 = page_dir + i;
        if (*pte1 & PTE_V) {
            pagetable_t page_mid_dir = (pagetable_t)PTE2PA(*pte1);
            for (int j = 0; j < cnt; j++) {
                pte_t* pte2 = page_mid_dir + j;
                if (*pte2 & PTE_V) {
                    pagetable_t page_low_dir = (pagetable_t)PTE2PA(*pte2);
                    for (int k = 0; k < cnt; k++) {
                        pte_t* pte3 = page_low_dir + k;
                        if (*pte3 & PTE_V) {
                            // uint64 page = PTE2PA(*pte3);
                            // // sprint("lgm:the pa is %0x\n", page);
                            // // if (free_mem_start_addr <= page && page < free_mem_end_addr) {
                            // //     free_page((void *)page); // 释放物理页
                            // //     (*pte3) &= ~PTE_V; // 将页表项置为无效
                            // // } 
                            // free_page((void *)page); // 释放物理页(注意：修改了原先的free_page函数)
                            // (*pte3) &= ~PTE_V; // 将页表项置为无效
                            vaild_cnt ++;
                            // sprint("lgm:the pa is %0x\n", PTE2PA(*pte3));
                            if (*pte3 & PTE_W) {
                                valid_and_writable_cnt ++;
                                // sprint("                lgm:the pa is %0x\n", PTE2PA(*pte3));
                                uint64 page = PTE2PA(*pte3);
                                free_page((void *)page); 
                            }
                            (*pte3) &= ~PTE_V; // 将页表项置为无效
                        }
                    }
                    // sprint("lgm:page_low_dir is %0x\n", page_low_dir);
                    free_page((void *)page_low_dir);
                }
            }
            // sprint("lgm:page_mid_dir is %0x\n", page_mid_dir);
            free_page((void *)page_mid_dir);
        }
    }
    // sprint("lgm:page_dir is %0x\n", page_dir);
    free_page((void *)page_dir);
    // sprint("exec_clean_pagetable end\n");
    // panic("stop");
    // sprint("                                         vaild_cnt is %d, valid_and_writable_cnt is %d\n", vaild_cnt, valid_and_writable_cnt);
}

void exec_clean(process* p) {
    // 释放原先内存
    exec_clean_pagetable(p->pagetable);
    
    // init proc[i]'s vm space
    p->trapframe = (trapframe *)alloc_page(); // trapframe, used to save context 
    memset(p->trapframe, 0, sizeof(trapframe));

    // page directory
    p->pagetable = (pagetable_t)alloc_page();
    memset((void *)p->pagetable, 0, PGSIZE);

    p->kstack = (uint64)alloc_page() + PGSIZE; // user kernel stack top
    uint64 user_stack = (uint64)alloc_page();        // phisical address of user stack bottom
    p->trapframe->regs.sp = USER_STACK_TOP;    // virtual address of user stack top

    // allocates a page to record memory regions (segments)
    p->mapped_info = (mapped_region *)alloc_page();
    memset(p->mapped_info, 0, PGSIZE);

    // map user stack in userspace
    user_vm_map((pagetable_t)p->pagetable, USER_STACK_TOP - PGSIZE, PGSIZE,
                user_stack, prot_to_type(PROT_WRITE | PROT_READ, 1));
    p->mapped_info[STACK_SEGMENT].va = USER_STACK_TOP - PGSIZE;
    p->mapped_info[STACK_SEGMENT].npages = 1;
    p->mapped_info[STACK_SEGMENT].seg_type = STACK_SEGMENT;

    // map trapframe in user space (direct mapping as in kernel space).
    user_vm_map((pagetable_t)p->pagetable, (uint64)p->trapframe, PGSIZE, // trapframe的物理地址等于虚拟地址
                (uint64)p->trapframe, prot_to_type(PROT_WRITE | PROT_READ, 0));
    p->mapped_info[CONTEXT_SEGMENT].va = (uint64)p->trapframe;
    p->mapped_info[CONTEXT_SEGMENT].npages = 1;
    p->mapped_info[CONTEXT_SEGMENT].seg_type = CONTEXT_SEGMENT;

    // map S-mode trap vector section in user space (direct mapping as in kernel space)
    // we assume that the size of usertrap.S is smaller than a page.
    user_vm_map((pagetable_t)p->pagetable, (uint64)trap_sec_start, PGSIZE,
                (uint64)trap_sec_start, prot_to_type(PROT_READ | PROT_EXEC, 0));
    p->mapped_info[SYSTEM_SEGMENT].va = (uint64)trap_sec_start;
    p->mapped_info[SYSTEM_SEGMENT].npages = 1;
    p->mapped_info[SYSTEM_SEGMENT].seg_type = SYSTEM_SEGMENT;

    // initialize the process's heap manager
    p->user_heap.heap_top = USER_FREE_ADDRESS_START;
    p->user_heap.heap_bottom = USER_FREE_ADDRESS_START;
    p->user_heap.free_pages_count = 0;

    // map user heap in userspace
    p->mapped_info[HEAP_SEGMENT].va = USER_FREE_ADDRESS_START;
    p->mapped_info[HEAP_SEGMENT].npages = 0; // no pages are mapped to heap yet.
    p->mapped_info[HEAP_SEGMENT].seg_type = HEAP_SEGMENT;

    p->total_mapped_region = 4;
    // sprint("lgm:exec_clean: p->total_mapped_region = %d\n", p->total_mapped_region);

    // initialize files_struct
    // p->pfiles = init_proc_file_management();
    // sprint("in alloc_proc. build proc_file_management successfully.\n");

    // p->wait_pid = 0; // 没有需要等待的子进程
    p->parent = NULL; // 设置为没有父进程
}