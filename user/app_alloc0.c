// #include "user_lib.h"
// #include "util/types.h"

// #define N 5
// #define BASE 0

// int main(void) {
//   void *p[N];

//   for (int i = 0; i < N; i++) {
//     p[i] = better_malloc(50);
//     int *pi = p[i];
//     *pi = BASE + i;
//     printu("=== user alloc 0 @ vaddr 0x%x\n", p[i]);
//   }

//   for (int i = 0; i < N; i++) {
//     int *pi = p[i];
//     printu("=== user0: %d\n", *pi);
//     better_free(p[i]);
//   }

//   exit(0);
// }

/*
 * This app fork a child process to read and write the heap data from parent process.
 * Because implemented copy on write, when child process only read the heap data,
 * the physical address is the same as the parent process.
 * But after writing, child process heap will have different physical address.              
 */

#include "user/user_lib.h"
#include "util/types.h"

int main(int argc, char *argv[]) {
  int *heap_data = better_malloc(20);
  int *heap_data1 = better_malloc(20);
  printu("the physical address of parent process heap is: ");
  printpa(heap_data);
  printpa(heap_data1);
  int pid = fork();
  if (pid == 0) {
    printu("the physical address of child process heap before copy on write is: \n");
    printpa(heap_data);
    printpa(heap_data1);
    heap_data[0] = 0;
    heap_data1[0] = 0;
    printu("the physical address of child process heap after copy on write is: \n");
    printpa(heap_data);
    printpa(heap_data1);
  }
  exit(0);
  return 0;
}
