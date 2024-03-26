/*
 * This app starts a very simple shell and executes some simple commands.
 * The commands are stored in the hostfs_root/shellrc
 * The shell loads the file and executes the command line by line.                 
 */
#include "user_lib.h"
#include "string.h"
#include "util/types.h"

#define Author "Mingxin_Yang"
#define Host "ymx"

int main(int argc, char *argv[]) {
  printu("\n======== Shell Start ========\n\n");

  printu("Author: %s\n", Author);
  printu("Operating System: RISC-V\n\n"); 

  int start = 0;
  char str[20] = "cross";
  while(TRUE){
    char *command = (char*) better_malloc(100);
    char *para = (char*) better_malloc(100);
    // char *command = (char*) naive_malloc();
    // char *para = (char*) naive_malloc();
    char path[30];
    memset(path, '\0', strlen(path));
    read_cwd(path);
    printu("%s@%s:%s ", Author, Host, path);
    int a = scanfu("%s%s", command, para);

    if(!strcmp(command, "exit")) break;
    printu("Next command: %s %s\n\n", command, para);
    printu("==========Command Start============\n\n");
    int ret = 0;
    int pid = fork();
    if(pid == 0) {
      // printu("Next command: %s %s\n\n", command, para);
      ret = exec(command, para);
      
      if (ret == -1)
      printu("exec failed!\n");
    }
    else{
      wait(pid);
      printu("==========Command End============\n\n");
    }
  }
  exit(0);
  return 0;
}

/*
 * This app starts a very simple shell and executes some simple commands.
 * The commands are stored in the hostfs_root/shellrc
 * The shell loads the file and executes the command line by line.                 
 */
// #include "user_lib.h"
// #include "string.h"
// #include "util/types.h"

// int main(int argc, char *argv[]) {
//   printu("\n======== Shell Start ========\n\n");
//   int fd;
//   int MAXBUF = 1024;
//   char buf[MAXBUF];
//   char *token;
//   char delim[3] = " \n";
//   fd = open("/shellrc", O_RDONLY);

//   read_u(fd, buf, MAXBUF);
//   close(fd);
//   char *command = better_malloc(50);
//   char *para = better_malloc(50);
//   int start = 0;
//   while (1)
//   {
//     if(!start) {
//       token = strtok(buf, delim);
//       start = 1;
//     }
//     else 
//       token = strtok(NULL, delim);
//     strcpy(command, token);
//     token = strtok(NULL, delim);
//     strcpy(para, token);
//     if(strcmp(command, "END") == 0 && strcmp(para, "END") == 0)
//       break;
//     printu("Next command: %s %s\n\n", command, para);
//     printu("==========Command Start============\n\n");
//     int pid = fork();
//     if(pid == 0) {
//       int ret = exec(command, para);
//       if (ret == -1)
//       printu("exec failed!\n");
//     }
//     else
//     {
//       wait(pid);
//       printu("==========Command End============\n\n");
//     }
//   }
//   exit(0);
//   return 0;
// }
