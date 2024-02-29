#include "kernel/riscv.h"
#include "kernel/process.h"
#include "spike_interface/spike_utils.h"

// ! add for lab1_challenge2
#include "string.h" 
char error_path[128], error_code[10240];
struct stat tmp_stat;

// Parameter is the entry of array "process-line".
// line_show prints the code line the entry points to.
//
void line_show(addr_line *line) {
  int len = strlen(current->dir[current->file[line->file].dir]);
  // get construct path
  strcpy(error_path, current->dir[current->file[line->file].dir]);
  sprint("Error : %s", error_path);
  error_path[len] = '/';
  strcpy(error_path + len + 1, current->file[line->file].file);

  // read and print code line
  spike_file_t *f = spike_file_open(error_path, O_RDONLY, 0);
  spike_file_stat(f, &tmp_stat);
  spike_file_read(f, error_code, tmp_stat.st_size);
  spike_file_close(f);

  for (int off = 0, line_cnt = 0; off < tmp_stat.st_size; line_cnt++) {
    int tmp_off = off;
    while (tmp_off < tmp_stat.st_size && error_code[tmp_off] != '\n') tmp_off++;
    if (line_cnt == line->line - 1) {
        error_code[tmp_off] = '\0';
        sprint("Runtime error at %s:%d\n%s\n", error_path, line->line, error_code + off);
        break;
    }
    off = tmp_off + 1;
  }
}

//
// Find the "process->line" array entry
//
void error_displayer() {
  uint64 mepc = read_csr(mepc);
  for (int i = 0; i < current->line_ind; i++) {
    // find the exception line table entry
    if (mepc < current->line[i].addr) {
      line_show(current->line + i - 1);
      break;
    }
  }
}

static void handle_instruction_access_fault() {error_displayer(); panic("Instruction access fault!"); }

static void handle_load_access_fault() {error_displayer(); panic("Load access fault!"); }

static void handle_store_access_fault() {error_displayer(); panic("Store/AMO access fault!"); }

static void handle_illegal_instruction() {error_displayer(); panic("Illegal instruction!"); }

static void handle_misaligned_load() {error_displayer(); panic("Misaligned Load!"); }

static void handle_misaligned_store() {error_displayer(); panic("Misaligned AMO!"); }

// added @lab1_3
static void handle_timer() {
  int cpuid = 0;
  // setup the timer fired at next time (TIMER_INTERVAL from now)
  *(uint64*)CLINT_MTIMECMP(cpuid) = *(uint64*)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

  // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
  write_csr(sip, SIP_SSIP);
}

//
// handle_mtrap calls a handling function according to the type of a machine mode interrupt (trap).
//
void handle_mtrap() {
  uint64 mcause = read_csr(mcause);
  switch (mcause) {
    case CAUSE_MTIMER:
      handle_timer();
      break;
    case CAUSE_FETCH_ACCESS:
      handle_instruction_access_fault();
      break;
    case CAUSE_LOAD_ACCESS:
      handle_load_access_fault();
    case CAUSE_STORE_ACCESS:
      handle_store_access_fault();
      break;
    case CAUSE_ILLEGAL_INSTRUCTION:
      // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
      // interception, and finish lab1_2.
      //panic( "call handle_illegal_instruction to accomplish illegal instruction interception for lab1_2.\n" );
      handle_illegal_instruction();

      break;
    case CAUSE_MISALIGNED_LOAD:
      handle_misaligned_load();
      break;
    case CAUSE_MISALIGNED_STORE:
      handle_misaligned_store();
      break;

    default:
      sprint("machine trap(): unexpected mscause %p\n", mcause);
      sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
      panic( "unexpected exception happened in M-mode.\n" );
      break;
  }
}
