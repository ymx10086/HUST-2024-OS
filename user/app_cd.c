#include "user_lib.h"
#include "util/string.h"
#include "util/types.h"


int main(int argc, char *argv[]) {
    char *dir = argv[0];
    printu("\n======== cd command ========\n");

    if (change_cwd(dir) != 0)
    printu("cd failed\n");

    exit(0);
    return 0;
}
