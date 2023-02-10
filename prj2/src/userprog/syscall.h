#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "lib/user/syscall.h"
#include <stdio.h>

void syscall_init(void);
int fibonacci(int n);
int max_of_four_int(int a, int b, int c, int d);
bool verify_access(uint32_t *args, int argc);

#endif /* userprog/syscall.h */
