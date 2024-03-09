/*
 * Interface functions between file system and kernel/processes. added @lab4_1
 */

#include "proc_file.h"

#include "hostfs.h"
#include "pmm.h"
#include "process.h"
#include "ramdev.h"
#include "rfs.h"
#include "riscv.h"
#include "spike_interface/spike_file.h"
#include "spike_interface/spike_utils.h"
#include "util/functions.h"
#include "util/string.h"

#include "elf.h"
#include "vmm.h"
#include "memlayout.h"

//
// initialize file system
//
void fs_init(void) {
  // initialize the vfs
  vfs_init();

  // register hostfs and mount it as the root
  if( register_hostfs() < 0 ) panic( "fs_init: cannot register hostfs.\n" );
  struct device *hostdev = init_host_device("HOSTDEV");
  vfs_mount("HOSTDEV", MOUNT_AS_ROOT);

  // register and mount rfs
  if( register_rfs() < 0 ) panic( "fs_init: cannot register rfs.\n" );
  struct device *ramdisk0 = init_rfs_device("RAMDISK0");
  rfs_format_dev(ramdisk0);
  vfs_mount("RAMDISK0", MOUNT_DEFAULT);
}

//
// initialize a proc_file_management data structure for a process.
// return the pointer to the page containing the data structure.
//
proc_file_management *init_proc_file_management(void) {
  proc_file_management *pfiles = (proc_file_management *)alloc_page();
  pfiles->cwd = vfs_root_dentry; // by default, cwd is the root
  pfiles->nfiles = 0;

  for (int fd = 0; fd < MAX_FILES; ++fd)
    pfiles->opened_files[fd].status = FD_NONE;

  sprint("FS: created a file management struct for a process.\n");
  return pfiles;
}

//
// reclaim the open-file management data structure of a process.
// note: this function is not used as PKE does not actually reclaim a process.
//
void reclaim_proc_file_management(proc_file_management *pfiles) {
  free_page(pfiles);
  return;
}

//
// get an opened file from proc->opened_file array.
// return: the pointer to the opened file structure.
//
struct file *get_opened_file(int fd) {
  struct file *pfile = NULL;

  // browse opened file list to locate the fd
  for (int i = 0; i < MAX_FILES; ++i) {
    pfile = &(current->pfiles->opened_files[i]);  // file entry
    if (i == fd) break;
  }
  if (pfile == NULL) panic("do_read: invalid fd!\n");
  return pfile;
}

//
// open a file named as "pathname" with the permission of "flags".
// return: -1 on failure; non-zero file-descriptor on success.
//
int do_open(char *pathname, int flags) {
  struct file *opened_file = NULL;
  if ((opened_file = vfs_open(pathname, flags)) == NULL) return -1;

  int fd = 0;
  if (current->pfiles->nfiles >= MAX_FILES) {
    panic("do_open: no file entry for current process!\n");
  }
  struct file *pfile;
  for (fd = 0; fd < MAX_FILES; ++fd) {
    pfile = &(current->pfiles->opened_files[fd]);
    if (pfile->status == FD_NONE) break;
  }

  // initialize this file structure
  memcpy(pfile, opened_file, sizeof(struct file));

  ++current->pfiles->nfiles;
  return fd;
}

//
// read content of a file ("fd") into "buf" for "count".
// return: actual length of data read from the file.
//
int do_read(int fd, char *buf, uint64 count) {
  struct file *pfile = get_opened_file(fd);

  if (pfile->readable == 0) panic("do_read: no readable file!\n");

  char buffer[count + 1];
  int len = vfs_read(pfile, buffer, count);
  buffer[count] = '\0';
  strcpy(buf, buffer);
  return len;
}

//
// write content ("buf") whose length is "count" to a file "fd".
// return: actual length of data written to the file.
//
int do_write(int fd, char *buf, uint64 count) {
  struct file *pfile = get_opened_file(fd);

  if (pfile->writable == 0) panic("do_write: cannot write file!\n");

  int len = vfs_write(pfile, buf, count);
  return len;
}

//
// reposition the file offset
//
int do_lseek(int fd, int offset, int whence) {
  struct file *pfile = get_opened_file(fd);
  return vfs_lseek(pfile, offset, whence);
}

int do_ccwd(char *pa){
  char* path_copy = (char*)pa;
  char mask[MAX_PATH_LEN];
  memset(mask, '\0', MAX_PATH_LEN);
  char origin[MAX_PATH_LEN];
  memset(origin, '\0', MAX_PATH_LEN);
  // sprint("current path : %s\n", current->pfiles->cwd->name);
  memcpy(mask, current->pfiles->cwd->name, strlen(current->pfiles->cwd->name));

  if (path_copy[0] == '.') {
    if (path_copy[1] == '.') {
      // return back to the last directory
      int i = strlen(mask) - 1;
      while(1){
        if (mask[i] == '/') {
          mask[i] = '\0';
          break;
        }
        i--;
      }
      if (strlen(mask) == 0) mask[0] = '/', mask[1] = '\0';
      memset(current->pfiles->cwd->name, '\0', MAX_PATH_LEN);
      memcpy(current->pfiles->cwd->name, mask, strlen(mask));
    }
    else{
      int len = strlen(mask);
      if (strlen(mask) == 0) mask[1] = '\0';
      if (strlen(mask) == 1) mask[0] = '\0', mask[1] = '\0';
      int i = 0, tag = 0;
      while(i < strlen(path_copy)){
        if (path_copy[i] == '/') {
          tag = i;
          break;
        }
        i++;
      }
      if(tag == 0) path_copy[0] = '/', path_copy[1] = '\0';
      memcpy(mask + strlen(mask), path_copy + tag, strlen(path_copy + tag));

      struct dentry *parent = vfs_root_dentry;
      char miss_name[MAX_PATH_LEN];

      // lookup the dir, find its parent direntry
      struct dentry *file_dentry = lookup_final_dentry(mask, &parent, miss_name);
      if (!file_dentry) {
        sprint("vfs_cwd: the directory donot exists!\n");
        return -1;
      }

      memset(current->pfiles->cwd->name, '\0', MAX_PATH_LEN);
      memcpy(current->pfiles->cwd->name, mask, strlen(mask));
    }
  }
  else if (path_copy[0] == '/') {
  
    struct dentry *parent = vfs_root_dentry;
    char miss_name[MAX_PATH_LEN];

    // lookup the dir, find its parent direntry
    struct dentry *file_dentry = lookup_final_dentry(path_copy, &parent, miss_name);
    if (!file_dentry) {
      sprint("vfs_cwd: the directory donot exists!\n");
      return -1;
    }

     // sprint("copy path : %s\n", path_copy);
    memset(current->pfiles->cwd->name, '\0', MAX_PATH_LEN);
    memcpy(current->pfiles->cwd->name, path_copy, strlen(path_copy));
    // sprint("current path : %s\n", current->pfiles->cwd->name);
  }

  return 0;
}

//
// read the vinode information
//
int do_stat(int fd, struct istat *istat) {
  struct file *pfile = get_opened_file(fd);
  return vfs_stat(pfile, istat);
}

//
// read the inode information on the disk
//
int do_disk_stat(int fd, struct istat *istat) {
  struct file *pfile = get_opened_file(fd);
  return vfs_disk_stat(pfile, istat);
}

//
// close a file
//
int do_close(int fd) {
  struct file *pfile = get_opened_file(fd);
  return vfs_close(pfile);
}

//
// open a directory
// return: the fd of the directory file
//
int do_opendir(char *pathname) {
  struct file *opened_file = NULL;
  if ((opened_file = vfs_opendir(pathname)) == NULL) return -1;

  int fd = 0;
  struct file *pfile;
  for (fd = 0; fd < MAX_FILES; ++fd) {
    pfile = &(current->pfiles->opened_files[fd]);
    if (pfile->status == FD_NONE) break;
  }
  if (pfile->status != FD_NONE)  // no free entry
    panic("do_opendir: no file entry for current process!\n");

  // initialize this file structure
  memcpy(pfile, opened_file, sizeof(struct file));

  ++current->pfiles->nfiles;
  return fd;
}

//
// read a directory entry
//
int do_readdir(int fd, struct dir *dir) {
  struct file *pfile = get_opened_file(fd);
  return vfs_readdir(pfile, dir);
}

//
// make a new directory
//
int do_mkdir(char *pathname) {
  return vfs_mkdir(pathname);
}

//
// close a directory
//
int do_closedir(int fd) {
  struct file *pfile = get_opened_file(fd);
  return vfs_closedir(pfile);
}

//
// create hard link to a file
//
int do_link(char *oldpath, char *newpath) {
  return vfs_link(oldpath, newpath);
}

//
// remove a hard link to a file
//
int do_unlink(char *path) {
  return vfs_unlink(path);
}

// load exec
static void exec_bincode(process *p, char *path){
    sprint("Application: %s\n", path);
    int fp = do_open(path, O_RDONLY);
    spike_file_t *f = (spike_file_t *)(get_opened_file(fp)->f_dentry->dentry_inode->i_fs_info); 
    elf_header ehdr;
    if (spike_file_read(f, &ehdr, sizeof(elf_header)) != sizeof(elf_header)){
        panic("read elf header error\n");
    }

    // load the code_segmant and data_segmant
    elf_prog_header ph_addr;
    for (int i = 0, off = ehdr.phoff; i < ehdr.phnum; i++, off += sizeof(ph_addr)){
        
        spike_file_lseek(f, off, SEEK_SET); // seek to the program header
        if (spike_file_read(f, &ph_addr, sizeof(ph_addr)) != sizeof(ph_addr))
            panic("read elf program header error\n");
        if (ph_addr.type != ELF_PROG_LOAD) continue;

        void *pa = alloc_page(); 
        memset(pa, 0, PGSIZE);
        user_vm_map((pagetable_t)p->pagetable, ph_addr.vaddr, PGSIZE, (uint64)pa, prot_to_type(PROT_WRITE | PROT_READ | PROT_EXEC, 1));
        spike_file_lseek(f, ph_addr.off, SEEK_SET);
        if (spike_file_read(f, pa, ph_addr.memsz) != ph_addr.memsz){
            panic("read program segment error.\n");
        }

        int pos;
        for (pos = 0; pos < PGSIZE / sizeof(mapped_region); pos++) // seek the last mapped region
            if (p->mapped_info[pos].va == 0x0)
                break;

        p->mapped_info[pos].va = ph_addr.vaddr;
        p->mapped_info[pos].npages = 1;

        // SEGMENT_READABLE, SEGMENT_EXECUTABLE, SEGMENT_WRITABLE are defined in kernel/elf.h
        if (ph_addr.flags == (SEGMENT_READABLE | SEGMENT_EXECUTABLE)){
            p->mapped_info[pos].seg_type = CODE_SEGMENT;
            sprint("CODE_SEGMENT added at mapped info offset:%d\n", pos);
        }
        else if (ph_addr.flags == (SEGMENT_READABLE | SEGMENT_WRITABLE)){
            p->mapped_info[pos].seg_type = DATA_SEGMENT;
            sprint("DATA_SEGMENT added at mapped info offset:%d\n", pos);
        }
        else
            panic("unknown program segment encountered, segment flag:%d.\n", ph_addr.flags);

        p->total_mapped_region++;
    }
    
    p->trapframe->epc = ehdr.entry;
    do_close(fp); 
    sprint("Application program entry point (virtual address): 0x%lx\n", p->trapframe->epc);
}

int do_exec(char *path_, char *arg_){
   
	int PathLen = strlen(path_);
	char path[PathLen + 1];
	strcpy(path, path_);
	int ArgLen = strlen(arg_);
	char arg[ArgLen + 1];
	strcpy(arg, arg_);

  sprint("=============");
  
	exec_clean(current);

  sprint("Path : %s, arg : %s \n", path, arg);
	
	uint64 argv_va = current->trapframe->regs.sp - ArgLen - 1;
	argv_va = argv_va - argv_va % 8; 
	uint64 argv_pa = (uint64)user_va_to_pa(current->pagetable, (void *)argv_va);
	strcpy((char *)argv_pa, arg);

	uint64 argvs_va = argv_va - 8; 
	uint64 argvs_pa = (uint64)user_va_to_pa(current->pagetable, (void *)argvs_va);
	*(uint64 *)argvs_pa = argv_va; 

	current->trapframe->regs.a0 = 1; 
	current->trapframe->regs.a1 = argvs_va; 
	current->trapframe->regs.sp = argvs_va - argvs_va % 16; 

	load_bincode_from_host_elf(current, path); 

	return -1;
}
