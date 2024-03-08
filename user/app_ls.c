#include "user_lib.h"
#include "util/string.h"
#include "util/types.h"

int dir_print(int dir_fd, char *path, int times){
  struct dir dir;
  int width = 20;
  int times_;
  
  while(readdir_u(dir_fd, &dir) == 0) {
    for(int i = 0; i < times * 3; i++)
      printu(" ");
    if(times)
      for(int i = 0; i < 3; i++)
        printu("-");
    // we do not have %ms :(
    char name[width + 1];
    memset(name, ' ', width + 1);
    name[width] = '\0';
    if (strlen(dir.name) < width) {
      strcpy(name, dir.name);
      name[strlen(dir.name)] = ' ';
      printu("%s %d\n", name, dir.inum);
    }
    else
      printu("%s %d\n", dir.name, dir.inum);
    char *inpath = naive_malloc();
    memset(inpath, '\0', 128);
    memcpy(inpath, path, strlen(path) + 1);
    
    inpath[strlen(path)] = '/';
    memcpy(inpath + strlen(path) + 1, dir.name, strlen(dir.name) + 1);

    int dir_fd_in = opendir_u(inpath);
    // printu("%d", dir_fd_in);
    if(dir_fd_in != -1){
      times_ = times + 1;
      dir_print(dir_fd_in, inpath, times_);
      closedir_u(dir_fd_in);
    }
    
  }
  return 0;
}

int main(int argc, char *argv[]) {
  char *path = argv[0];
  int dir_fd = opendir_u(path);
  // printu("%d", dir_fd);
  if(dir_fd == -1){
    printu("Donnot ls for a file !");
    exit(0);
    return 0;
  }
  int times = 0;
  printu("---------- ls command -----------\n");
  printu("ls \"%s\":\n", path);
  printu("[name]               [inode_num]\n");
  dir_print(dir_fd, path, times);
  printu("------------------------------\n");
  closedir_u(dir_fd);
  exit(0);
  return 0;
}