#ifndef _CONFIG_H_
#define _CONFIG_H_

// we use only one HART (cpu) in fundamental experiments
#define NCPU 2 // comment: ���ں������޸�Ϊ2

//interval of timer interrupt. added @lab1_3
#define TIMER_INTERVAL 1000000

#define DRAM_BASE 0x80000000

/* we use fixed physical (also logical) addresses for the stacks and trap frames as in
 Bare memory-mapping mode */
// // user stack top
// #define USER_STACK 0x81100000

// // the stack used by PKE kernel when a syscall happens
// #define USER_KSTACK 0x81200000

// // the trap frame used to assemble the user "process"
// #define USER_TRAP_FRAME 0x81300000

// #define USER_STACK_BASE 0x81100000
// #define USER_STACK(hartid) (USER_STACK_BASE + (hartid) * 0x100000)

// #define USER_KSTACK_BASE 0x81300000
// #define USER_KSTACK(hartid) (USER_KSTACK_BASE + (hartid) * 0x100000)

// #define USER_TRAP_FRAME_BASE 0x81500000
// #define USER_TRAP_FRAME(hartid) (USER_TRAP_FRAME_BASE + (hartid) * 0x100000)

#define USER_STACK_ZERO 0x81100000
#define USER_KSTACK_ZERO 0x81200000
#define USER_TRAP_FRAME_ZERO 0x81300000

#define USER_STACK_ONE 0x85100000
#define USER_KSTACK_ONE 0x85200000
#define USER_TRAP_FRAME_ONE 0x85300000

#endif
