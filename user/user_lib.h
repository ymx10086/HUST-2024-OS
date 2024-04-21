/*
 * header file to be used by applications.
 */

int printu(const char *s, ...);
int exit(int code);
void* naive_malloc();
void naive_free(void* va);
int fork();
void yield();

// ! add for lab3_challenge2
int sem_new(int val);
int sem_P(int sem);
int sem_V(int sem);
