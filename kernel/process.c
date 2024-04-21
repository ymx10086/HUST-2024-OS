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

#include "spike_interface/spike_utils.h"
#include "spike_interface/atomic.h"

//Two functions defined in kernel/usertrap.S
extern char smode_trap_vector[];
extern void return_to_user(trapframe *, uint64 satp);

// trap_sec_start points to the beginning of S-mode trap segment (i.e., the entry point
// of S-mode trap vector).
extern char trap_sec_start[];

// process pool. added @lab3_1
process procs[NPROC];

// current points to the currently running user-mode application.
process *current[NCPU] = {NULL};

spinlock_t current_lock;

// points to the first free page in our simple heap. added @lab2_2
// uint64 g_ufree_page = USER_FREE_ADDRESS_START;

//
// switch to a user-mode process
//
// extern spinlock_t proc_lock;
void switch_to(process *proc) {
  uint64 hartid = read_tp();
  assert(proc);

  spinlock_lock(&current_lock);
  current[hartid] = proc;
  spinlock_unlock(&current_lock);

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

  // spinlock_unlock(&proc_lock); 

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

spinlock_t alloc_process_lock;

//
// allocate an empty process, init its vm space. returns the pointer to
// process strcuture. added @lab3_1
//
process *alloc_process(){
  spinlock_lock(&alloc_process_lock);
	// locate the first usable process structure
  uint64 hartid = read_tp();
	int i;

	for (i = 0; i < NPROC; i++)
		if (procs[i].status == FREE)
			break;

	if (i >= NPROC){
		panic("cannot find any free process structure.\n");
		return 0;
	}

	// init proc[i]'s vm space
	procs[i].trapframe = (trapframe *)alloc_page(); // trapframe, used to save context
	memset(procs[i].trapframe, 0, sizeof(trapframe));

	// page directory
	procs[i].pagetable = (pagetable_t)alloc_page();
	memset((void *)procs[i].pagetable, 0, PGSIZE);

	procs[i].kstack = (uint64)alloc_page() + PGSIZE; // user kernel stack top
	uint64 user_stack = (uint64)alloc_page();		 // phisical address of user stack bottom
	procs[i].trapframe->regs.sp = USER_STACK_TOP;	 // virtual address of user stack top
  procs[i].trapframe->regs.tp = hartid; // 十分重要

	// allocates a page to record memory regions (segments)
	procs[i].mapped_info = (mapped_region *)alloc_page();
	memset(procs[i].mapped_info, 0, PGSIZE);

	// map user stack in userspace
	user_vm_map((pagetable_t)procs[i].pagetable, USER_STACK_TOP - PGSIZE, PGSIZE,
				user_stack, prot_to_type(PROT_WRITE | PROT_READ, 1));
  
	procs[i].mapped_info[STACK_SEGMENT].va = USER_STACK_TOP - PGSIZE;
	procs[i].mapped_info[STACK_SEGMENT].npages = 1;
	procs[i].mapped_info[STACK_SEGMENT].seg_type = STACK_SEGMENT;

	// map trapframe in user space (direct mapping as in kernel space).
	user_vm_map((pagetable_t)procs[i].pagetable, (uint64)procs[i].trapframe, PGSIZE,
				(uint64)procs[i].trapframe, prot_to_type(PROT_WRITE | PROT_READ, 0));
	procs[i].mapped_info[CONTEXT_SEGMENT].va = (uint64)procs[i].trapframe;
	procs[i].mapped_info[CONTEXT_SEGMENT].npages = 1;
	procs[i].mapped_info[CONTEXT_SEGMENT].seg_type = CONTEXT_SEGMENT;

	// map S-mode trap vector section in user space (direct mapping as in kernel space)
	// we assume that the size of usertrap.S is smaller than a page.
	user_vm_map((pagetable_t)procs[i].pagetable, (uint64)trap_sec_start, PGSIZE,
				(uint64)trap_sec_start, prot_to_type(PROT_READ | PROT_EXEC, 0)); 
	procs[i].mapped_info[SYSTEM_SEGMENT].va = (uint64)trap_sec_start;
	procs[i].mapped_info[SYSTEM_SEGMENT].npages = 1;
	procs[i].mapped_info[SYSTEM_SEGMENT].seg_type = SYSTEM_SEGMENT;

	sprint("hartid = %lld: in alloc_proc. user frame 0x%lx, user stack 0x%lx, user kstack 0x%lx \n",
		   hartid, procs[i].trapframe, procs[i].trapframe->regs.sp, procs[i].kstack);

  

	// // initialize the process's heap manager
	// procs[i].user_heap.heap_top = USER_FREE_ADDRESS_START;
	// procs[i].user_heap.heap_bottom = USER_FREE_ADDRESS_START;
	// procs[i].user_heap.free_pages_count = 0;

  // initialize the process's heap manager
	procs[i].user_heap.mem_free = -1;
	procs[i].user_heap.mem_used = -1;
	procs[i].user_heap.g_ufree_page = USER_FREE_ADDRESS_START;

	// map user heap in userspace
	procs[i].mapped_info[HEAP_SEGMENT].va = USER_FREE_ADDRESS_START;
	procs[i].mapped_info[HEAP_SEGMENT].npages = 0; // no pages are mapped to heap yet.
	procs[i].mapped_info[HEAP_SEGMENT].seg_type = HEAP_SEGMENT;

	procs[i].total_mapped_region = 4;

	// initialize files_struct
	procs[i].pfiles = init_proc_file_management();
	sprint("hartid = %lld: in alloc_proc. build proc_file_management successfully.\n", hartid);

	procs[i].parent = NULL;

  sprint("hartid = %lld: process %d has been alloc.\n", hartid, i);

	// return after initialization.

  procs[i].status = READY;

  spinlock_unlock(&alloc_process_lock);
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

//   for (uint64 heap_block = proc->user_heap.heap_bottom;
//     heap_block < proc->user_heap.heap_top; heap_block += PGSIZE) {
  
//    user_vm_unmap(proc->pagetable, heap_block, PGSIZE, 0); 
// }

  // add for lab3_challenge1
  if (proc->parent != NULL && proc->parent->status == BLOCKED) {
    proc->parent->status = READY;
    insert_to_ready_queue(proc->parent);
  }

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
  sprint("hartid = %lld: will fork a child from parent %d.\n", read_tp(), parent->pid);
  process *child = alloc_process();

  // sprint("Num of parent->total_mapped_region: %d\n", parent->total_mapped_region);

  // for(int i = 0; i < parent->total_mapped_region; i++){
    // sprint("title : %d\n", parent->mapped_info[i].seg_type);
  // }

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
      case HEAP_SEGMENT:

        // for (uint64 heap_block = current->user_heap.heap_bottom;
        //     heap_block < current->user_heap.heap_top; heap_block += PGSIZE) {
        //   uint64 parent_pa = lookup_pa(parent->pagetable, heap_block);
          
        //   user_vm_map((pagetable_t)child->pagetable, heap_block, PGSIZE, parent_pa,
        //               prot_to_type(PROT_READ, 1));
          
        //   pte_t *child_pte = page_walk(child->pagetable, heap_block, 0);
        //   *child_pte |= PTE_C; 
        //   pte_t *parent_pte = page_walk(parent->pagetable, heap_block, 0);
        //   *parent_pte &= (~PTE_W); 
        // }
        uint64 free_va = parent->user_heap.mem_free;
        while (free_va != -1){
            uint64 free_pa = (uint64)(user_va_to_pa((pagetable_t)parent->pagetable, (void *)free_va));
            cow_vm_map((pagetable_t)child->pagetable, free_va, free_pa);
            free_va = ((mem_block *)free_pa)->next;
        }
        uint64 used_va = parent->user_heap.mem_used;
        while (used_va != -1){
            uint64 used_pa = (uint64)(user_va_to_pa((pagetable_t)parent->pagetable, (void *)used_va));
            cow_vm_map((pagetable_t)child->pagetable, used_va, used_pa);
            used_va = ((mem_block *)used_pa)->next;
        }

        // sprint("free_va : %d, used_va : %d\n", free_va, used_va);
        child->mapped_info[HEAP_SEGMENT].npages = parent->mapped_info[HEAP_SEGMENT].npages;
        memcpy((void*)&child->user_heap, (void*)&parent->user_heap, sizeof(parent->user_heap));
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
         
          map_pages(child->pagetable, parent->mapped_info[i].va + j * PGSIZE, PGSIZE, pa_of_mapped_va, prot_to_type(PROT_READ | PROT_EXEC, 1));
        }

        // ! add for lab3_challenge1
        uint64 pa_of_mapped_va_child = lookup_pa(parent->pagetable, parent->mapped_info[i].va);
        sprint("hartid = %lld: do_fork map code segment at pa:%lx of parent to child at va:%lx.\n", read_tp(), pa_of_mapped_va_child, parent->mapped_info[i].va);

        // after mapping, register the vm region (do not delete codes below!)
        child->mapped_info[child->total_mapped_region].va = parent->mapped_info[i].va;
        child->mapped_info[child->total_mapped_region].npages =
                parent->mapped_info[i].npages;
        child->mapped_info[child->total_mapped_region].seg_type = CODE_SEGMENT;
        // sprint("%d\n", child->total_mapped_region);
        child->total_mapped_region++;
        // sprint("%d\n", child->total_mapped_region);
        break;

      // ! add for lab3_challenge1
      case DATA_SEGMENT:

        
        for (int j = 0; j < parent->mapped_info[i].npages; j++) {
          uint64 pa_of_mapped_va = lookup_pa(parent->pagetable, parent->mapped_info[i].va + j * PGSIZE);
          // need to register new page
          void *new_addr = alloc_page();
          memcpy(new_addr, (void *) pa_of_mapped_va, PGSIZE);
          map_pages(child->pagetable, parent->mapped_info[i].va + j * PGSIZE, PGSIZE, (uint64) new_addr, prot_to_type(PROT_READ | PROT_WRITE, 1));
        }

        // after mapping, register the vm region (do not delete codes below!)
        child->mapped_info[child->total_mapped_region].va = parent->mapped_info[i].va;
        child->mapped_info[child->total_mapped_region].npages = 
                parent->mapped_info[i].npages;
        child->mapped_info[child->total_mapped_region].seg_type = DATA_SEGMENT;

        child->total_mapped_region++;
        break;
    }
  }

          
  child->pfiles->nfiles = parent->pfiles->nfiles;
  child->pfiles->cwd = parent->pfiles->cwd;
  for (int i = 0; i < MAX_FILES; i++) {
      child->pfiles->opened_files[i] = parent->pfiles->opened_files[i];
      if (child->pfiles->opened_files[i].f_dentry != NULL)
          child->pfiles->opened_files[i].f_dentry->d_ref++;
  }

  child->status = READY;
  child->trapframe->regs.a0 = 0;
  child->parent = parent;
  child->trapframe->regs.tp = read_tp();
  insert_to_ready_queue(child);



  return child->pid;
}

// realease three level pagetable
static void exec_clean_pagetable(pagetable_t page_dir) {

    for (int i = 0; i < PGSIZE / sizeof(pte_t); i++) {
        pte_t* pte1 = page_dir + i;
        if (*pte1 & PTE_V) {
            pagetable_t page_mid_dir = (pagetable_t)PTE2PA(*pte1);
            for (int j = 0; j < PGSIZE / sizeof(pte_t); j++) {
                pte_t* pte2 = page_mid_dir + j;
                if (*pte2 & PTE_V) {
                    pagetable_t page_low_dir = (pagetable_t)PTE2PA(*pte2);
                    for (int k = 0; k < PGSIZE / sizeof(pte_t); k++) {
                        pte_t* pte3 = page_low_dir + k;
                        if (*pte3 & PTE_V) {
                            if (*pte3 & PTE_W & PTE_R) {
                                uint64 page = PTE2PA(*pte3);
                                free_page((void *)page); 
                            }
                            (*pte3) &= ~PTE_V; 
                        }
                    }
                    free_page((void *)page_low_dir);
                }
            }
            free_page((void *)page_mid_dir);
        }
    }
    free_page((void *)page_dir);
}

// release three level pagetable
void exec_clean(process* p) {
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
    user_vm_map((pagetable_t)p->pagetable, (uint64)p->trapframe, PGSIZE, 
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

    // // initialize the process's heap manager
    // p->user_heap.heap_top = USER_FREE_ADDRESS_START;
    // p->user_heap.heap_bottom = USER_FREE_ADDRESS_START;
    // p->user_heap.free_pages_count = 0;

    // initialize the process's heap manager
    p->user_heap.mem_free = -1;
    p->user_heap.mem_used = -1;
    p->user_heap.g_ufree_page = USER_FREE_ADDRESS_START;

    // map user heap in userspace
    p->mapped_info[HEAP_SEGMENT].va = USER_FREE_ADDRESS_START;
    p->mapped_info[HEAP_SEGMENT].npages = 0; // no pages are mapped to heap yet.
    p->mapped_info[HEAP_SEGMENT].seg_type = HEAP_SEGMENT;

    p->mapped_info[DATA_SEGMENT].seg_type = DATA_SEGMENT;
    p->mapped_info[CODE_SEGMENT].seg_type = CODE_SEGMENT;

    p->total_mapped_region = 4;
}

// ! add for lab3_challenge2
// set
semaphore sems[NPROC];

int alloc_sem(int val) {
  // select a free semaphore
  for (int i = 0; i < NPROC; i++) {
    if (sems[i].occupied) continue;
    sems[i].occupied = 1;
    sems[i].val = val;
    sems[i].wl_head = sems[i].wl_tail = NULL;
    return i;
  }
  panic("no enough semaphore, please correct the programm!!!");
}

// v
int v_sem(int sem) {
  if (sem < 0 || sem >= NPROC) return -1;
  sems[sem].val++;
  if (sems[sem].wl_head != NULL) {
    process *t = sems[sem].wl_head;
    sems[sem].wl_head = t->queue_next;
    insert_to_ready_queue(t);
  }
  return 0;
}

//p
int p_sem(int sem) {
  uint64 hartid = read_tp();
  if (sem < 0 || sem >= NPROC) return -1;
  sems[sem].val--;
  if (sems[sem].val < 0) {
    if (sems[sem].wl_head != NULL) {
      sems[sem].wl_tail->queue_next = current[hartid]->queue_next;
      // sprint("RRR : %d", (current[hartid]->queue_next != NULL));
      sems[sem].wl_tail = current[hartid];
    } else {
      sems[sem].wl_head = sems[sem].wl_tail = current[hartid];
      current[hartid]->queue_next = NULL;
    }
    current[hartid]->status = BLOCKED;
    schedule();
  }
  return 0;
}

//
//// insert for free chain
//
static void insert_into_free(uint64 mem_va){
  uint64 hartid = read_tp();
	if (current[hartid]->user_heap.mem_free == -1){
		mem_block *mem_pa = (mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)mem_va));
		mem_pa->next = -1;
		current[hartid]->user_heap.mem_free = mem_va;
		return;
	}
	mem_block *mem_pa = (mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)mem_va));
	uint64 current_va = current[hartid]->user_heap.mem_free;
	mem_block *current_pa = (mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)current[hartid]->user_heap.mem_free));
	// judge if insert into the head of the chain
	if (current_pa->cap > mem_pa->cap){
		mem_pa->next = current[hartid]->user_heap.mem_free;
		current[hartid]->user_heap.mem_free = mem_va;
		return;
	}
  // insert into the correct location
	while (current_pa->next != -1){
		uint64 next_va = current_pa->next;
		mem_block *next_pa = (mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)next_va));
		if (next_pa->cap > mem_pa->cap) break;
		current_va = next_va;
		current_pa = next_pa;
	}
	mem_pa->next = current_pa->next;
	current_pa->next = mem_va;
	return;
}

//
//// head insert for used chain
// 
static void insert_into_used(uint64 mem_va){
  uint64 hartid = read_tp();
	mem_block *mem_pa = (mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)mem_va));
	mem_pa->next = current[hartid]->user_heap.mem_used;
	current[hartid]->user_heap.mem_used = mem_va;
}

static void remove_ptr(uint64 *head, uint64 mem_va){
  uint64 hartid = read_tp();
	if (*head == mem_va){
		*head = ((mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)mem_va)))->next;
		return;
	}
	mem_block *p = (mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)(*head)));
	while (p->next != mem_va){
		p = (mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)p->next));
	}
	p->next = ((mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)mem_va)))->next;
	return;
}

// calu the next addr for mem with mem_size aligned
static uint64 get_next_mem_addr(uint64 addr){
	uint64 ret = addr + sizeof(mem_block) - addr % sizeof(mem_block);
	return ret;
}

// free
static void alloc_from_free(uint64 va, uint64 n){
  uint64 hartid = read_tp();
  uint64 page_used = va + sizeof(mem_block) + n;
	uint64 next_mem_addr = get_next_mem_addr(page_used);
	mem_block *pa = (mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)va));
	if (va + pa->cap > next_mem_addr){
    // the left memory can be used ever
		uint64 free_mem = next_mem_addr;
		mem_block *free_mem_pa = (mem_block *)user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)free_mem);
		free_mem_pa->cap = va + pa->cap - next_mem_addr;
		pa->cap = next_mem_addr - va;
		remove_ptr(&(current[hartid]->user_heap.mem_free), va);
    // update the chain of used and free
		insert_into_used(va);
		insert_into_free(free_mem);
	}
	else{
		remove_ptr(&(current[hartid]->user_heap.mem_free), va);
    // update the chain of used
		insert_into_used(va);
	}
}

// free + new_page
static void alloc_from_free_and_g_ufree_page(uint64 va, uint64 n) {
  uint64 hartid = read_tp();
	remove_ptr(&(current[hartid]->user_heap.mem_free), va); 
	mem_block* pa = (mem_block*)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void*)va));
	uint64 unallocated = n - (pa->cap - sizeof(mem_block)); // 还有remain个字节需要分配
	uint64 new_pages = unallocated / PGSIZE + (unallocated % PGSIZE != 0); // 需要new_pages个页面
	
  // allovation
  uint64 current_pages = new_pages;
  // uint64 current_va = g_ufree_page;
  uint64 current_va = current[hartid]->user_heap.g_ufree_page;
	while(current_pages--){
		uint64 new_pa = (uint64)alloc_page();
		memset((void *)new_pa, 0, PGSIZE);
		user_vm_map((pagetable_t)current[hartid]->pagetable, current_va, PGSIZE, new_pa,
					prot_to_type(PROT_WRITE | PROT_READ, 1));
    current[hartid]->user_heap.g_ufree_page += PGSIZE;
    current_va = current[hartid]->user_heap.g_ufree_page;
	}

	uint64 last_page_use = unallocated % PGSIZE;
	if (last_page_use != 0){
		uint64 next_mem_addr = get_next_mem_addr(current[hartid]->user_heap.g_ufree_page - PGSIZE + last_page_use);
    // sprint("NEXT : %d", next_mem_addr);
		if (current[hartid]->user_heap.g_ufree_page - next_mem_addr > sizeof(mem_block)) {
		
			mem_block* free_pa = (mem_block*)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void*)next_mem_addr));
			free_pa->next = -1;
			free_pa->cap = PGSIZE - next_mem_addr % PGSIZE;
			insert_into_free(next_mem_addr);
			pa->cap = next_mem_addr - va;
		}
		else {
			pa->cap += new_pages * PGSIZE;
		}
	}
	insert_into_used(va);
}

// new_page
static void *alloc_from_g_ufree_page(uint64 n){

  uint64 hartid = read_tp();

	// calu counts of pages
	uint64 pages = (n + sizeof(mem_block) + PGSIZE - 1) / PGSIZE;
	uint64 first_page_va = current[hartid]->user_heap.g_ufree_page;
	uint64 last_page_va = current[hartid]->user_heap.g_ufree_page + (pages - 1) * PGSIZE;  

	// allovation
  uint64 current_pages = pages;
  uint64 current_va = current[hartid]->user_heap.g_ufree_page;
	while(current_pages--){
		uint64 pa = (uint64)alloc_page();
		memset((void *)pa, 0, PGSIZE);
		user_vm_map((pagetable_t)current[hartid]->pagetable, current_va, PGSIZE, pa,
					prot_to_type(PROT_WRITE | PROT_READ, 1));
    current[hartid]->user_heap.g_ufree_page += PGSIZE;
    current_va = current[hartid]->user_heap.g_ufree_page;
	}

	// control the memory
	uint64 last_page_used = (n + sizeof(mem_block)) % PGSIZE; // the used space of the last used page
	uint64 next_mem_addr = get_next_mem_addr(last_page_va + last_page_used); //calu the next addr with one mem_block
  
	if (next_mem_addr + sizeof(mem_block) >= last_page_va + PGSIZE){
    // the left memory cannot be used
  
		mem_block *use_pa = (mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)first_page_va));
		
    use_pa->cap = pages * PGSIZE;
		insert_into_used(first_page_va);
	}
	else{
     
		mem_block *use_pa = (mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)first_page_va));
		use_pa->cap = next_mem_addr - first_page_va;
		insert_into_used(first_page_va);
		mem_block *free_pa = (mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)next_mem_addr));
		free_pa->cap = last_page_va + PGSIZE - next_mem_addr;
		insert_into_free(next_mem_addr);
	}   
 
	return (void *)(first_page_va + sizeof(mem_block));
}

void *better_alloc(uint64 n){ 

  uint64 hartid = read_tp();
  
	if (current[hartid]->user_heap.mem_free == -1) return alloc_from_g_ufree_page(n); 
	uint64 va = current[hartid]->user_heap.mem_free;
	uint64 max_va = va; // record the max va
  uint64 va_ptr;
	while (va != -1){
		max_va = va > max_va ? va : max_va;
		mem_block *pa = (mem_block *)(user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)va));
		if (pa->cap >= n + sizeof(mem_block)){ 
			va_ptr = va + sizeof(mem_block);
			alloc_from_free(va, n);
			return (void *)va_ptr;
		}
		va = pa->next;
	}

	mem_block* max_pa = (mem_block*)user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)max_va);
	if (max_va + max_pa->cap == current[hartid]->user_heap.g_ufree_page){

		va_ptr = max_va + sizeof(mem_block); 
		alloc_from_free_and_g_ufree_page(max_va, n);
		return (void *)va_ptr;
	}
	return alloc_from_g_ufree_page(n); 
}

void better_free(uint64 va){

  uint64 hartid = read_tp();

	uint64 p = va - sizeof(mem_block); 
	remove_ptr(&(current[hartid]->user_heap.mem_used), p);
	insert_into_free(p);
	return;
}

void cow_vm_map(pagetable_t page_dir, uint64 va, uint64 pa){
	pte_t *pte = page_walk(page_dir, va, 1);
	*pte = PA2PTE(pa) | PTE_V | PTE_R | PTE_U | PTE_A | PTE_C;

}
