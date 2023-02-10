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

struct lock f_lock;

static void syscall_handler (struct intr_frame *);
void halt(void);
void exit(int stat);
pid_t exec(const char *file);
int wait(pid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

struct file
{
  struct inode *inode; 
  off_t pos;           
  bool deny_write;     
};

void
syscall_init (void) 
{
  lock_init(&f_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
   if (!verif_acs((uint32_t *)(f->esp), 1))
    exit(-1);

   switch (*(uint32_t *)(f->esp)) {
    case SYS_HALT:
      halt();
      break;

    case SYS_EXIT:
      if (!verif_acs((uint32_t *)(f->esp) + 4, 1))
        exit(-1);
      exit(*(uint32_t *)(f->esp + 4));
      break;

    case SYS_EXEC:
      if (!verif_acs((uint32_t *)(f->esp) + 4, 1))
        exit(-1);
      f->eax = (uint32_t)exec((const char *)*(uint32_t *)(f->esp + 4));
      break;

    case SYS_WAIT:
      if (!verif_acs((uint32_t *)(f->esp) + 4, 1))
        exit(-1);
      f->eax = (uint32_t)wait((pid_t) * (uint32_t *)(f->esp + 4));
      break;
    
    case SYS_CREATE:
     if (!verif_acs((uint32_t *)(f->esp) + 4, 2))
        exit(-1);
      f->eax = (uint32_t)create((const char *)*(uint32_t *)(f->esp + 4), (unsigned)*(uint32_t *)(f->esp + 8));
      break;

    case SYS_REMOVE:
     if (!verif_acs((uint32_t *)(f->esp) + 4, 1))
        exit(-1);
      f->eax = (uint32_t)remove((const char *)*(uint32_t *)(f->esp + 4));
      break;

    case SYS_OPEN:
     if (!verif_acs((uint32_t *)(f->esp) + 4, 1))
        exit(-1);
       f->eax = (uint32_t)open((const char *)*(uint32_t *)(f->esp + 4));
      break;

    case SYS_FILESIZE:
     if (!verif_acs((uint32_t *)(f->esp) + 4, 1))
        exit(-1);
       f->eax = (uint32_t)filesize((int)*(uint32_t *)(f->esp + 4));
      break;

    case SYS_READ:
      if (!verif_acs((uint32_t *)(f->esp) + 4, 3))
        exit(-1);
      f->eax = (uint32_t)read((int)*(uint32_t *)(f->esp + 4), (void *)*(uint32_t *)(f->esp + 8), (unsigned)*(uint32_t *)(f->esp + 12));
      break;

    case SYS_WRITE:
      if (!verif_acs((uint32_t *)(f->esp) + 4, 3))
        exit(-1);
      f->eax = (uint32_t)write((int)*(uint32_t *)(f->esp + 4), (const void *)*(uint32_t *)(f->esp + 8), (unsigned)*(uint32_t *)(f->esp + 12));
      break;

    case SYS_SEEK:
      if (!verif_acs((uint32_t *)(f->esp) + 4, 2))
        exit(-1);
      seek((int)*(uint32_t *)(f->esp + 4), (unsigned)*(uint32_t *)(f->esp + 8));
      break;
    
    case SYS_TELL:
      if (!verif_acs((uint32_t *)(f->esp) + 4, 1))
        exit(-1);
      f->eax = (uint32_t)tell((int)*(uint32_t *)(f->esp + 4));
      break;

    case SYS_CLOSE:
      if (!verif_acs((uint32_t *)(f->esp) + 4, 1))
        exit(-1);
      close((int)*(uint32_t *)(f->esp + 4));
      break;

    case SYS_FIB:
      if (!verif_acs((uint32_t *)(f->esp) + 4, 1))
        exit(-1);
      f->eax = (uint32_t)fibonacci((int)*(uint32_t *)(f->esp + 4));
      break;

    case SYS_MOFI:
      if (!verif_acs((uint32_t *)(f->esp) + 4, 4))
        exit(-1);
      f->eax = (uint32_t)max_of_four_int((int)*(uint32_t *)(f->esp + 4), (int)*(uint32_t *)(f->esp + 8), (int)*(uint32_t *)(f->esp + 12), (int)*(uint32_t *)(f->esp + 16));
      break;

    default:
      exit(-1);
      break;
  }

}

void halt(void){
	shutdown_power_off();
}

void 
exit(int stat)
{
  thread_current()->exit_stat = stat;

  printf("%s: exit(%d)\n", thread_current()->name, thread_current()->exit_stat);
 
  int i = 3;
  while (i < 128)
  {
    if (thread_current()->fd[i] != NULL)
      close(i);
    i++;
  }

  if (thread_current()->parent)
   thread_current()->parent->exit_stat =  thread_current()->exit_stat;

  thread_exit();
}

pid_t 
exec(const char *file)
{
  lock_acquire(&f_lock);
  pid_t tmp = (pid_t)process_execute(file);
  lock_release(&f_lock);
  return tmp;
}

int 
wait(pid_t pid)
{
  return process_wait((tid_t)pid);
}

bool 
create(const char *file, unsigned initial_size)
{
  if (file == NULL)
    exit(-1);
  return filesys_create(file, initial_size);
}

bool 
remove(const char *file)
{
  if (file == NULL)
    exit(-1);
  return filesys_remove(file);
}

int 
open(const char *file)
{
  if (file == NULL || !is_user_vaddr(file))
    exit(-1);
  
  lock_acquire(&f_lock);
  struct file *fp = filesys_open(file);

  if (fp == NULL)
  {
    lock_release(&f_lock);
    return -1;
  }

  else
  {
    int tmp = -1, i = 3;
    while (i < 128)
    {
      if (thread_current()->fd[i] == NULL)
      {
        if (strcmp(thread_current()->name, file) == 0)
          file_deny_write(fp);
        
        thread_current()->fd[i] = fp;
        tmp = i;
        break;
      }
      i++;
    }
    lock_release(&f_lock);
    return tmp;
  }
}

int 
filesize(int fd)
{
  return file_length(thread_current()->fd[fd]);
}

int 
read (int fd, void *buffer, unsigned size)
{
  if (!is_user_vaddr(buffer))
    exit(-1);

  if (fd == 0)
  {
    int i = 0;
    for (i = 0; i < (int)size; i ++)
      if(((char *)buffer)[i] == '\0')
        break;

    return i;
  } 

  else if (fd >= 3)
  {
    int tmp = -1;

    if (thread_current()->fd[fd] == NULL)
      exit(-1);
    lock_acquire(&f_lock);
    tmp = file_read(thread_current()->fd[fd], buffer, size);
    lock_release(&f_lock);

    return tmp;
  }

}

int 
write (int fd, const void *buffer, unsigned size)
{
  if (!is_user_vaddr(buffer))
    exit(-1);

  if (fd == 1)
  {
    putbuf(buffer, size);
    return size;
  }

  else if (fd >= 3)
  {
    int tmp = -1;

    if (thread_current()->fd[fd] == NULL)
      exit(-1);
    
    if (thread_current()->fd[fd]->deny_write)
      file_deny_write(thread_current()->fd[fd]);
  
    lock_acquire(&f_lock);
    tmp = file_write(thread_current()->fd[fd], buffer, size);
    lock_release(&f_lock);

    return tmp;
  }
}

void 
seek(int fd, unsigned position)
{
  if (thread_current()->fd[fd] == NULL)
    exit(-1);
  file_seek(thread_current()->fd[fd], position);
}

unsigned 
tell(int fd)
{
  if (thread_current()->fd[fd] == NULL)
    exit(-1);
  return file_tell(thread_current()->fd[fd]);
}

void 
close(int fd)
{
  if (thread_current()->fd[fd] == NULL)
    exit(-1);
    
  file_close(thread_current()->fd[fd]);
  thread_current()->fd[fd] = NULL;
}

int 
fibonacci(int n)
{
	if(n == 0)
    return 0;
    
  else if(n == 1 || n == 2)
    return 1;

  else
    return fibonacci(n-1) + fibonacci(n-2);
}

int 
max_of_four_int(int a, int b, int c, int d)
{
  int max = a;

  if (max < b)
    max = b;
  if (max < c)
    max = c;
  if (max < d)
    max = d;

  return max;
}

bool 
verif_acs(uint32_t *args, int argc)
{
  for (int i = 0; i < argc; i++)
    if (!is_user_vaddr(args + i * 4))
      return false;

  return true;
}
