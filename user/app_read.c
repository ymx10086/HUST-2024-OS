 #include "user_lib.h"
#include "util/string.h"
#include "util/types.h"

int main(int argc, char *argv[]) {
  int fd;
  int MAXBUF = 512;
  char buf[MAXBUF];
  printu("------------------------------\n");
  printu("read: /hostfile.txt\n");

  fd = open("/hostfile.txt", O_RDONLY);
  printu("file descriptor fd: %d\n", fd);

  read_u(fd, buf, MAXBUF);
  printu("read content: \n%s\n", buf);
  printu("------------------------------\n");
  close(fd);

  exit(0);
  return 0;
}