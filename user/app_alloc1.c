// #include "user_lib.h"
// #include "util/types.h"

// #define N 5
// #define BASE 5

// int main(void) {
//   void *p[N];

//   for (int i = 0; i < N; i++) {
//     p[i] = better_malloc(50);
//     int *pi = p[i];
//     *pi = BASE + i;
//     printu(">>> user alloc 1 @ vaddr 0x%x\n", p[i]);
//   }

//   for (int i = 0; i < N; i++) {
//     int *pi = p[i];
//     printu(">>> user 1: %d\n", *pi);
//     better_free(p[i]);
//   }

//   exit(0);
// }

#include "user_lib.h"
#include "util/types.h"

int main(void) {

  int pid;

  for(int i = 0; i < 100; i++){
    pid = fork();
  }

  if (pid == 0) {
    printu("the physical address of child process heap before copy on write is: \n");
  
    printu("the physical address of child process heap after copy on write is: \n");

  }

  exit(0);
}
