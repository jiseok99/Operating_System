#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/exception.h"
#include "userprog/process.h"
#include <devices/input.h>

static void syscall_handler(struct intr_frame *);
void halt(void);
void exit(int status);
int wait(pid_t pid);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
pid_t exec(const char *cmd_line);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  //printf("the sc is %d\n", (uint32_t *)(f->esp)[1]);
  //printf("%d\n",(int)*(uint32_t*)(f->esp));
//	printf("the (int)(f->esp) is %d %d %d %d\n", (int)*(uint32_t *)(f->esp+4),(int *)*(uint32_t *)(f->esp + 8), (int *)*(uint32_t *)(f->esp + 12), (int *)*(uint32_t *)(f->esp + 16));
  //hex_dump(f->esp,f->esp,100,1);
 // printf("the (uint32_t *)(f->esp)ber is %d %d %d %d\n", (uint32_t *)f->esp[1], (uint32_t *)(f->esp)[2], (uint32_t *)(f->esp)[3], (uint32_t *)(f->esp)[4]);


  if (!verify_access((uint32_t *)(f->esp), 1))
    exit(-1);

  switch ((int)*(uint32_t *)(f->esp))
  {
  case SYS_HALT:
    halt();
    break;

  case SYS_EXIT:
    if (!verify_access((uint32_t *)(f->esp) + 4, 1))
      exit(-1);
    exit(*(uint32_t *)(f->esp + 4));
    break;

  case SYS_EXEC:
    if (!verify_access((uint32_t *)(f->esp) + 4, 1))
      exit(-1);
    f->eax = (uint32_t)exec((const char *)*(uint32_t *)(f->esp + 4));
    break;

  case SYS_WAIT:
    if (!verify_access((uint32_t *)(f->esp) + 4, 1))
      exit(-1);
    f->eax = (uint32_t)wait((pid_t) * (uint32_t *)(f->esp + 4));
    break;

  case SYS_READ:
    if (!verify_access((uint32_t *)(f->esp) + 4, 3))
      exit(-1);

    f->eax = (uint32_t)read((int)*(uint32_t *)(f->esp + 4), (void *)*(uint32_t *)(f->esp + 8), (unsigned)*(uint32_t *)(f->esp + 12));
    break;

  case SYS_WRITE:
    if (!verify_access((uint32_t *)(f->esp) + 4, 3))
      exit(-1);
    f->eax = (uint32_t)write((int)*(uint32_t *)(f->esp + 4), (const void *)*(uint32_t *)(f->esp + 8), (unsigned)*(uint32_t *)(f->esp + 12));
    break;

  case SYS_FIBO:
    if (!verify_access((uint32_t *)(f->esp) + 4, 1))
      exit(-1);
    f->eax = (uint32_t)fibonacci((int)*(uint32_t *)(f->esp + 4));
    break;
  case SYS_MAX_FOUR:
    if (!verify_access((uint32_t *)(f->esp) + 4, 4))
      exit(-1);
    f->eax = (uint32_t)max_of_four_int((int)*(uint32_t *)(f->esp + 4), (int)*(uint32_t *)(f->esp + 8), (int)*(uint32_t *)(f->esp + 12), (int)*(uint32_t *)(f->esp + 16));
    break;

  default:
    thread_exit();
    break;
  }
}

void halt()
{
  shutdown_power_off();
}

void exit(int status)
{
  struct thread *now = thread_current();
  now->exit_status = status;
  printf("%s: exit(%d)\n", now->name, now->exit_status);

  if (now->parent)
    now->parent->exit_status = now->exit_status;
  thread_exit();
}

int wait(pid_t pid)
{
  return process_wait((tid_t)pid);
}

int read(int fd, void *buffer, unsigned size)
{
  int ret = -1;

  if (fd == 0)
  {
    ret = 0;
    while (input_getc() != '\0')
    {
      ret++;
    }
  }
  else
  {
    ret = ret;
  }

  return ret;
}

int write(int fd, const void *buffer, unsigned size)
{
  int ret = -1;
  if (fd == 1)
  {
    //printf("end with senario write\n");
    putbuf(buffer, size);
    ret = size;
  }
  else
  {
    /*TODO NOT NOW FOR NOW*/
  }

  return ret;
}
pid_t exec(const char *cmd_line)
{
  return (pid_t)process_execute(cmd_line);
}

int fibonacci(int n)
{
  int now = 1, prev = 0, result = -1;
  n = n + 1;
  if (n < 0)
  {
    result = -1;
  }
  else if (n == 0)
  {
    result = 0;
  }
  else if (n == 1 || n == 2)
  {
    result = 1;
  }
  else
  {
    for (int i = 3; i <= n; i++)
    {
      result = now + prev;
      prev = now;
      now = result;
    }
  }
  return result;
}

int max_of_four_int(int a, int b, int c, int d)
{
  int maxi = a;
  if (maxi < b)
    maxi = b;
  if (maxi < c)
    maxi = c;
  if (maxi < d)
    maxi = d;
  return maxi;
}

bool verify_access(uint32_t *args, int argc)
{
  for (int i = 0; i < argc; i++)
  {
    if (!is_user_vaddr(args + i * 4))
    {
      return false;
    }
  }

  return true;
}
