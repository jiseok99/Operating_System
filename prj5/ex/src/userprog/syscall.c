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
#include <string.h>
#include "filesys/file.h"
#include "filesys/filesys.h"

struct lock file_lock;

static void syscall_handler(struct intr_frame *);
void halt(void);
void exit(int status);
int wait(pid_t pid);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
pid_t exec(const char *cmd_line);

struct file
{
  struct inode *inode; /* File's inode. */
  off_t pos;           /* Current position. */
  bool deny_write;     /* Has file_deny_write() been called? */
};

void syscall_init(void)
{
  lock_init(&file_lock);
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  //printf("the sc is %d\n", (uint32_t *)(f->esp)[1]);
  //printf("%d\n",(int)*(uint32_t*)(f->esp));
  //	printf("the (int)(f->esp) is %d %d %d %d\n", (int)*(uint32_t *)(f->esp+4),(int *)sc[2], (int *)sc[3], (int *)*(uint32_t *)(f->esp + 16));
  //hex_dump(0x804c7c0, 0x804c7c0, 100, 1);
  //hex_dump(f->esp, f->esp, 100, 1);
  // printf("the (uint32_t *)(f->esp)ber is %d %d %d %d\n", (uint32_t *)f->esp[1], (uint32_t *)(f->esp)[2], (uint32_t *)(f->esp)[3], (uint32_t *)(f->esp)[4]);
  //printf("inside the syscall!\n");
  int *sc = f->esp;
  if (!verify_access((uint32_t *)sc, 1))
    exit(-1);

  //printf("the syscall is %d!\n", sc[0]);

  switch (sc[0])
  {
  case SYS_HALT:
    halt();
    break;
  case SYS_EXIT:
    if (!verify_access((uint32_t *)&sc[1], 1))
      exit(-1);
    exit(sc[1]);
    break;
  case SYS_EXEC:
    if (!verify_access((uint32_t *)&sc[1], 1))
      exit(-1);
    f->eax = (uint32_t)exec((const char *)sc[1]);
    break;
  case SYS_WAIT:
    if (!verify_access((uint32_t *)&sc[1], 1))
      exit(-1);
    f->eax = (uint32_t)wait((pid_t)sc[1]);
    break;
  case SYS_CREATE:
    //printf("inside the syscall!\n");
    if (!verify_access((uint32_t *)&sc[1], 2))
      exit(-1);
    f->eax = (uint32_t)create((const char *)sc[1], (off_t)sc[2]);
    break;
  case SYS_REMOVE:
    if (!verify_access((uint32_t *)&sc[1], 1))
      exit(-1);
    f->eax = (uint32_t)remove((const char *)sc[1]);
    break;
  case SYS_OPEN:
    if (!verify_access((uint32_t *)&sc[1], 1))
      exit(-1);
    //printf("putting %s in open function\n", (const char *)sc[1]);
    f->eax = (uint32_t)open((const char *)sc[1]);
    break;
  case SYS_FILESIZE:
    if (!verify_access((uint32_t *)&sc[1], 1))
      exit(-1);
    f->eax = (uint32_t)filesize((int)sc[1]);
    break;
  case SYS_READ:
    if (!verify_access((uint32_t *)&sc[1], 3))
      exit(-1);
    //printf("read until line 87! descriptor is %d\n", sc[1]);
    //printf("the file fd size is %d, %d\n", (int)sc[1], (unsigned)sc[3]);
    f->eax = (uint32_t)read((int)sc[1], (void *)sc[2], (unsigned)sc[3]);
    break;
  case SYS_WRITE:
    if (!verify_access((uint32_t *)&sc[1], 3))
      exit(-1);
    //printf("the write!\n");
    f->eax = (uint32_t)write((int)sc[1], (const void *)sc[2], (unsigned)sc[3]);
    break;
  case SYS_SEEK:
    if (!verify_access((uint32_t *)&sc[1], 2))
      exit(-1);
    seek((int)sc[1], (unsigned int)sc[2]);
    break;
  case SYS_TELL:
    if (!verify_access((uint32_t *)&sc[1], 1))
      exit(-1);
    f->eax = (uint32_t)tell((int)sc[1]);
    break;
  case SYS_CLOSE:
    if (!verify_access((uint32_t *)&sc[1], 1))
      exit(-1);
    close((int)sc[1]);
    break;
  case SYS_FIBO:
    if (!verify_access((uint32_t *)&sc[1], 1))
      exit(-1);
    f->eax = (uint32_t)fibonacci((int)sc[1]);
    break;
  case SYS_MAX_FOUR:
    if (!verify_access((uint32_t *)&sc[1], 4))
      exit(-1);
    f->eax = (uint32_t)max_of_four_int((int)sc[1], (int)sc[2], (int)sc[3], (int)sc[4]);
    break;

  default:
    exit(-1);
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

  for (int i = 3; i < 128; i++)
  {
    if (thread_current()->fd[i] != NULL)
      close(i);
  }

  thread_exit();
}

pid_t exec(const char *cmd_line)
{

  lock_acquire(&file_lock);
  pid_t ret = (pid_t)process_execute(cmd_line);
  lock_release(&file_lock);
  return ret;
}

int wait(pid_t pid)
{
  return process_wait((tid_t)pid);
}

bool create(const char *file, unsigned initial_size)
{
  if (file == NULL)
    exit(-1);
  lock_acquire(&file_lock);
  bool ret = filesys_create(file, initial_size);
  lock_release(&file_lock);
  return ret;
}

bool remove(const char *file)
{
  if (file == NULL)
    exit(-1);
  lock_acquire(&file_lock);
  bool ret = filesys_remove(file);
  lock_release(&file_lock);
  return ret;
}

int open(const char *file)
{
  int ret = -1;
  if (file == NULL || !is_user_vaddr(file))
    exit(-1);
  //printf("the file name is! %s\n", file);
  lock_acquire(&file_lock);
  struct file *fp = filesys_open(file);
  if (fp == NULL)
  {
    ret = -1;
  }
  else
  {
    for (int i = 3; i < 128; i++)
    {
      if (thread_current()->fd[i] == NULL)
      {
        if (strcmp(thread_current()->name, file) == 0)
        {
          file_deny_write(fp);
        }
        thread_current()->fd[i] = fp;
        ret = i;
        break;
      }
    }
  }
  lock_release(&file_lock);
  //printf("%s file does not success!\n", file);
  return ret;
}

int filesize(int fd)
{

  struct thread *cur = thread_current();

  lock_acquire(&file_lock);
  int ret = file_length(cur->fd[fd]);
  lock_release(&file_lock);
  return ret;
}

int read(int fd, void *buffer, unsigned size)
{
  int ret = -1;
  //printf("read until line 207!\n");
  //printf("the fd is %d the size is %d\n", fd, size);
  if (buffer == NULL || !is_user_vaddr(buffer))
  {
    exit(-1);
  }
  //printf("you shall not pass!\n");
  if (fd == 0)
  {
    //lock_acquire(&file_lock);
    ret = 0;
    while (input_getc() != '\0')
    {
      ret++;
    }
    //lock_release(&file_lock);
  }
  else if (fd > 2)
  {
    //printf("the fd is bigger than 3!\n");
    //printf("read until the file_read!\n");
    //lock_acquire(&file_lock);
    if (thread_current()->fd[fd] == NULL)
    {
      //lock_release(&file_lock);
      exit(-1);
    }
    ret = file_read(thread_current()->fd[fd], buffer, size);
    //lock_release(&file_lock);
  }
  return ret;
}

int write(int fd, const void *buffer, unsigned size)
{
  if (buffer == NULL || !is_user_vaddr(buffer))
  {
    exit(-1);
  }
  //printf("%d %p %d\n", fd, buffer, size);
  int ret = -1;
  //lock_acquire(&file_lock);
  if (fd == 1)
  {
    //printf("end before lock_acquire!\n");
    //ASSERT(thread_current()->fd[fd]);
    putbuf(buffer, size);
    ret = size;
    //printf("%d\n", size);
  }
  else if (fd > 2)
  {
    /*TODO NOT NOW FOR NOW*/
    if (thread_current()->fd[fd] == NULL)
    {
      exit(-1);
    }
    //printf("go to write!\n");
    if (thread_current()->fd[fd]->deny_write)
    {
      //printf("hi!\n");
      file_deny_write(thread_current()->fd[fd]);
    }
    //printf("they write?\n");
    ret = file_write(thread_current()->fd[fd], buffer, size);
  }
  //lock_release(&file_lock);
  return ret;
}

void seek(int fd, unsigned position)
{
  if (thread_current()->fd[fd] == NULL)
    exit(-1);
  lock_acquire(&file_lock);
  file_seek(thread_current()->fd[fd], position);
  lock_release(&file_lock);
}

unsigned tell(int fd)
{
  if (thread_current()->fd[fd] == NULL)
    exit(-1);
  lock_acquire(&file_lock);
  unsigned ret = file_tell(thread_current()->fd[fd]);
  lock_release(&file_lock);
  return ret;
}

void close(int fd)
{
  if (thread_current()->fd[fd] == NULL)
  {
    exit(-1);
  }
  lock_acquire(&file_lock);
  struct file *fp = thread_current()->fd[fd];
  file_close(fp);
  thread_current()->fd[fd] = NULL;
  lock_release(&file_lock);
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
