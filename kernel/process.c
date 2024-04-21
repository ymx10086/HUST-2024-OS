/*
 * Utility functions for process management.
 *
 * Note: in Lab1, only one process (i.e., our user application) exists. Therefore,
 * PKE OS at this stage will set "current" to the loaded user application, and also
 * switch to the old "current" process after trap handling.
 */

#include "riscv.h"
#include "strap.h"
#include "config.h"
#include "process.h"
#include "elf.h"
#include "string.h"
#include "vmm.h"
#include "pmm.h"
#include "memlayout.h"
#include "spike_interface/spike_utils.h"

// Two functions defined in kernel/usertrap.S
extern char smode_trap_vector[];
extern void return_to_user(trapframe *, uint64 satp);

// current points to the currently running user-mode application.
process *current = NULL;

// points to the first free page in our simple heap. added @lab2_2
uint64 g_ufree_page = USER_FREE_ADDRESS_START; 

//
// switch to a user-mode process
//
void switch_to(process *proc)
{
	assert(proc);
	current = proc;

	// write the smode_trap_vector (64-bit func. address) defined in kernel/strap_vector.S
	// to the stvec privilege register, such that trap handler pointed by smode_trap_vector
	// will be triggered when an interrupt occurs in S mode.
	write_csr(stvec, (uint64)smode_trap_vector);

	// set up trapframe values (in process structure) that smode_trap_vector will need when
	// the process next re-enters the kernel.
	proc->trapframe->kernel_sp = proc->kstack;	   // process's kernel stack
	proc->trapframe->kernel_satp = read_csr(satp); // kernel page table
	proc->trapframe->kernel_trap = (uint64)smode_trap_handler;

	// SSTATUS_SPP and SSTATUS_SPIE are defined in kernel/riscv.h
	// set S Previous Privilege mode (the SSTATUS_SPP bit in sstatus register) to User mode.
	unsigned long x = read_csr(sstatus);
	x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
	x |= SSTATUS_SPIE; // enable interrupts in user mode

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
//// insert for free chain
//
static void insert_into_free(uint64 mem_va){
	if (current->mem_free == -1){
		mem_block *mem_pa = (mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)mem_va));
		mem_pa->next = -1;
		current->mem_free = mem_va;
		return;
	}
	mem_block *mem_pa = (mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)mem_va));
	uint64 current_va = current->mem_free;
	mem_block *current_pa = (mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)current->mem_free));
	// judge if insert into the head of the chain
	if (current_pa->cap > mem_pa->cap){
		mem_pa->next = current->mem_free;
		current->mem_free = mem_va;
		return;
	}
  // insert into the correct location
	while (current_pa->next != -1){
		uint64 next_va = current_pa->next;
		mem_block *next_pa = (mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)next_va));
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
	mem_block *mem_pa = (mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)mem_va));
	mem_pa->next = current->mem_used;
	current->mem_used = mem_va;
}

static void remove_ptr(uint64 *head, uint64 mem_va)
{
	if (*head == mem_va){
		*head = ((mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)mem_va)))->next;
		return;
	}
	mem_block *p = (mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)(*head)));
	while (p->next != mem_va){
		p = (mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)p->next));
	}
	p->next = ((mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)mem_va)))->next;
	return;
}

// calu the next addr for mem with mem_size aligned
static uint64 get_next_mem_addr(uint64 addr){
	uint64 ret = addr + sizeof(mem_block) - addr % sizeof(mem_block);
	return ret;
}

// free
static void alloc_from_free(uint64 va, uint64 n){
  uint64 page_used = va + sizeof(mem_block) + n;
	uint64 next_mem_addr = get_next_mem_addr(page_used);
	mem_block *pa = (mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)va));
	if (va + pa->cap > next_mem_addr){
    // the left memory can be used ever
		uint64 free_mem = next_mem_addr;
		mem_block *free_mem_pa = (mem_block *)user_va_to_pa((pagetable_t)current->pagetable, (void *)free_mem);
		free_mem_pa->cap = va + pa->cap - next_mem_addr;
		pa->cap = next_mem_addr - va;
		remove_ptr(&(current->mem_free), va);
    // update the chain of used and free
		insert_into_used(va);
		insert_into_free(free_mem);
	}
	else{
		remove_ptr(&(current->mem_free), va);
    // update the chain of used
		insert_into_used(va);
	}
}

// free + new_page
static void alloc_from_free_and_g_ufree_page(uint64 va, uint64 n) {
	remove_ptr(&(current->mem_free), va); 
	mem_block* pa = (mem_block*)(user_va_to_pa((pagetable_t)current->pagetable, (void*)va));
	uint64 unallocated = n - (pa->cap - sizeof(mem_block)); // 还有remain个字节需要分配
	uint64 new_pages = unallocated / PGSIZE + (unallocated % PGSIZE != 0); // 需要new_pages个页面
	
  // allovation
  uint64 current_pages = new_pages;
  uint64 current_va = g_ufree_page;
	while(current_pages--){
		uint64 new_pa = (uint64)alloc_page();
		memset((void *)new_pa, 0, PGSIZE);
		user_vm_map((pagetable_t)current->pagetable, current_va, PGSIZE, new_pa,
					prot_to_type(PROT_WRITE | PROT_READ, 1));
    g_ufree_page += PGSIZE;
    current_va = g_ufree_page;
	}

	uint64 last_page_use = unallocated % PGSIZE;
	if (last_page_use != 0){
		uint64 next_mem_addr = get_next_mem_addr(g_ufree_page - PGSIZE + last_page_use);
    // sprint("NEXT : %d", next_mem_addr);
		if (g_ufree_page - next_mem_addr > sizeof(mem_block)) {
		
			mem_block* free_pa = (mem_block*)(user_va_to_pa((pagetable_t)current->pagetable, (void*)next_mem_addr));
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
	// calu counts of pages
	uint64 pages = (n + sizeof(mem_block) + PGSIZE - 1) / PGSIZE;
	uint64 first_page_va = g_ufree_page;
	uint64 last_page_va = g_ufree_page + (pages - 1) * PGSIZE;

	// allovation
  uint64 current_pages = pages;
  uint64 current_va = g_ufree_page;
	while(current_pages--){
		uint64 pa = (uint64)alloc_page();
		memset((void *)pa, 0, PGSIZE);
		user_vm_map((pagetable_t)current->pagetable, current_va, PGSIZE, pa,
					prot_to_type(PROT_WRITE | PROT_READ, 1));
    g_ufree_page += PGSIZE;
    current_va = g_ufree_page;
	}
	// control the memory
	uint64 last_page_used = (n + sizeof(mem_block)) % PGSIZE; // the used space of the last used page
	uint64 next_mem_addr = get_next_mem_addr(last_page_va + last_page_used); //calu the next addr with one mem_block
  
	if (next_mem_addr + sizeof(mem_block) >= last_page_va + PGSIZE){
    // the left memory cannot be used
		mem_block *use_pa = (mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)first_page_va));
		use_pa->cap = pages * PGSIZE;
		insert_into_used(first_page_va);
	}
	else{
		mem_block *use_pa = (mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)first_page_va));
		use_pa->cap = next_mem_addr - first_page_va;
		insert_into_used(first_page_va);
		mem_block *free_pa = (mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)next_mem_addr));
		free_pa->cap = last_page_va + PGSIZE - next_mem_addr;
		insert_into_free(next_mem_addr);
	}
	return (void *)(first_page_va + sizeof(mem_block));
}

void *better_alloc(uint64 n){ 
	if (current->mem_free == -1) return alloc_from_g_ufree_page(n); 
	uint64 va = current->mem_free;
	uint64 max_va = va; // record the max va
  uint64 va_ptr;
	while (va != -1){
		max_va = va > max_va ? va : max_va;
		mem_block *pa = (mem_block *)(user_va_to_pa((pagetable_t)current->pagetable, (void *)va));
		if (pa->cap >= n + sizeof(mem_block)){ 
			va_ptr = va + sizeof(mem_block);
			alloc_from_free(va, n);
			return (void *)va_ptr;
		}
		va = pa->next;
	}

	mem_block* max_pa = (mem_block*)user_va_to_pa((pagetable_t)current->pagetable, (void *)max_va);
	if (max_va + max_pa->cap == g_ufree_page){

		va_ptr = max_va + sizeof(mem_block); 
		alloc_from_free_and_g_ufree_page(max_va, n);
		return (void *)va_ptr;
	}
	return alloc_from_g_ufree_page(n); 
}

void better_free(uint64 va){

	uint64 p = va - sizeof(mem_block); 
	remove_ptr(&(current->mem_used), p);
	insert_into_free(p);
	return;
}
