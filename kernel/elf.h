#ifndef _ELF_H_
#define _ELF_H_

#include "util/types.h"
#include "process.h"

#define MAX_CMDLINE_ARGS 64

// elf header structure
typedef struct elf_header_t {
  uint32 magic;
  uint8 elf[12];
  uint16 type;      /* Object file type */
  uint16 machine;   /* Architecture */
  uint32 version;   /* Object file version */
  uint64 entry;     /* Entry point virtual address */
  uint64 phoff;     /* Program header table file offset */
  uint64 shoff;     /* Section header table file offset */
  uint32 flags;     /* Processor-specific flags */
  uint16 ehsize;    /* ELF header size in bytes */
  uint16 phentsize; /* Program header table entry size */
  uint16 phnum;     /* Program header table entry count */
  uint16 shentsize; /* Section header table entry size */
  uint16 shnum;     /* Section header table entry count */
  uint16 shstrndx;  /* Section header string table index */
} elf_header;

// Program segment header.
typedef struct elf_prog_header_t {
  uint32 type;   /* Segment type */
  uint32 flags;  /* Segment flags */
  uint64 off;    /* Segment file offset */
  uint64 vaddr;  /* Segment virtual address */
  uint64 paddr;  /* Segment physical address */
  uint64 filesz; /* Segment size in file */
  uint64 memsz;  /* Segment size in memory */
  uint64 align;  /* Segment alignment */
} elf_prog_header;

//  add for lab1_challenge1_backtrace
//  support: https://man7.org/linux/man-pages/man5/elf.5.html
// section header structure
typedef struct elf_section_header_t {
  uint32 sh_name;      /* Section name */
  uint32 sh_type;      /* Section type */
  uint64 sh_flags;     /* Section flags */
  uint64 sh_addr;      /* Section virtual address at execution */
  uint64 sh_offset;    /* Section offset of file */
  uint64 sh_size;      /* Section's size in bytes */
  uint32 sh_link;      /* Section header table link which links to another section */
  uint32 sh_info;      /* Extra information */
  uint64 sh_addralign; /* Section address alignment constrainents */
  uint64 sh_entsize;   /* Entry size in bytes if section holds table */
} elf_section_header;

//  add for lab1_challenge1_backtrace
//  support: https://man7.org/linux/man-pages/man5/elf.5.html
// string and symbol tables structure
typedef struct elf_symbol_t {
  uint32 st_name;  /* Symbol names if nonzero */
  unsigned char st_info;  /* Symbol's type and binding attributes */
  unsigned char st_other; /* Symbol visibility */
  uint16 st_shndx; /* Symbol section header table index */
  uint64 st_value; /* Symbol value */
  uint64 st_size;  /* Symbol size */
} elf_symbol;

#define ELF_SECTION_HEADER_SZ (sizeof(elf_section_header))
#define ELF_SYMBOL_SZ (sizeof(elf_symbol))
#define SHT_SYMTAB 2// symbol table
#define SHT_STRTAB 3// string table
#define STT_FILE 4  // symbol name is file name
#define STT_FUNC 18

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian
#define ELF_PROG_LOAD 1

typedef enum elf_status_t {
  EL_OK = 0,

  EL_EIO,
  EL_ENOMEM,
  EL_NOTELF,
  EL_ERR,

} elf_status;

typedef struct elf_ctx_t {
  void *info;
  elf_header ehdr;

  // ! add for lab1_challenge1_backtrace
  char str_table[4096];
  elf_symbol symbols[128];
  uint64 symbol_cnt;
  
} elf_ctx;

elf_status elf_init(elf_ctx *ctx, void *info);
elf_status elf_load(elf_ctx *ctx);

//  add for lab1_challenge1_backtrace
elf_status load_elf_symbol(elf_ctx *ctx);

void load_bincode_from_host_elf(process *p);

#endif
