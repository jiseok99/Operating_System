#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed_point.c"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;
/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list sleep_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/*the min tick to awake*/
/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
{
  void *eip;             /* Return address. */
  thread_func *function; /* Function to call. */
  void *aux;             /* Auxiliary data for function. */
};

/* Statistics. */
static long long idle_ticks;   /* # of timer ticks spent idle. */
static long long kernel_ticks; /* # of timer ticks in kernel threads. */
static long long user_ticks;   /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4          /* # of timer ticks to give each thread. */
static unsigned thread_ticks; /* # of timer ticks since last yield. */

#define NICE_DEFAULT 0
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT 0

#ifndef USERPROG
/* Project #3 */
#endif
bool thread_prior_aging;
bool thread_mlfqs;
extern bool thread_started;

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *running_thread(void);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static bool is_thread(struct thread *) UNUSED;
static void *alloc_frame(struct thread *, size_t size);
static void schedule(void);
void thread_schedule_tail(struct thread *prev);
static tid_t allocate_tid(void);
static int load_avg;

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void thread_init(void)
{
  ASSERT(intr_get_level() == INTR_OFF);

  lock_init(&tid_lock);
  list_init(&ready_list);
  list_init(&all_list);
  list_init(&sleep_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread();
  init_thread(initial_thread, "main", PRI_DEFAULT);
  initial_thread->nice = 0;       /* new */
  initial_thread->recent_cpu = 0; /* new */
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void thread_start(void)
{
  /* Create the idle thread. */
  struct semaphore start_idle;
  sema_init(&start_idle, 0);
  thread_create("idle", PRI_MIN, idle, &start_idle);
  load_avg = LOAD_AVG_DEFAULT;

  /* Start preemptive thread scheduling. */
  intr_enable();
  /* Wait for the idle thread to initialize idle_thread. */
  sema_down(&start_idle);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void thread_tick(void)
{
  struct thread *t = thread_current();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return();
}

/* Prints thread statistics. */
void thread_print_stats(void)
{
  printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
         idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t thread_create(const char *name, int priority,
                    thread_func *function, void *aux)
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;
  ASSERT(function != NULL);

  /* Allocate thread. */
  t = palloc_get_page(PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread(t, name, priority);
  tid = t->tid = allocate_tid();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame(t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame(t, sizeof *ef);
  ef->eip = (void (*)(void))kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame(t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;
  /*add child_elem into current running thread*/
  intr_set_level(old_level);

  thread_unblock(t);
  if (priority > thread_get_priority())
  {
    thread_yield();
  } //우선순위 변화
  return tid;
}

/* Puts the current thread to sleep.  It will not be schedul ed
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void thread_block(void)
{
  //if (!thread_started)
  //  return;

  ASSERT(!intr_context());
  ASSERT(intr_get_level() == INTR_OFF);

  thread_current()->status = THREAD_BLOCKED;
  schedule();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void thread_unblock(struct thread *t)
{
  enum intr_level old_level;

  ASSERT(is_thread(t));

  old_level = intr_disable();
  ASSERT(t->status == THREAD_BLOCKED);
  //list_push_back(&ready_list, &t->elem);
  list_insert_ordered(&ready_list, &t->elem, priority_setup, 0);
  t->status = THREAD_READY;
  intr_set_level(old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name(void)
{
  return thread_current()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current(void)
{
  struct thread *t = running_thread();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT(is_thread(t));
  ASSERT(t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t thread_tid(void)
{
  return thread_current()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void thread_exit(void)
{
  ASSERT(!intr_context());

#ifdef USERPROG
  process_exit();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable();
  for (int i = 0; i < 128; i++)
  {
    thread_current()->fd[i] = NULL;
  }
  //sema_up(&(thread_current()->sema_wait));
  //list_remove(&(thread_current()->child_elem));
  list_remove(&thread_current()->allelem);
  //sema_down(&(thread_current()->sema_exit));
  running_thread()->status = THREAD_DYING;
  schedule();
  NOT_REACHED();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void thread_yield(void)
{
  //if (!thread_started)
  //  return;

  struct thread *cur = thread_current();
  enum intr_level old_level;

  ASSERT(!intr_context());

  old_level = intr_disable();
  //if (cur != idle_thread)
  //  list_push_back(&ready_list, &cur->elem);
  if (cur != idle_thread)
    list_insert_ordered(&ready_list, &cur->elem, priority_setup, 0);

  cur->status = THREAD_READY;
  schedule();
  intr_set_level(old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void thread_foreach(thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT(intr_get_level() == INTR_OFF);

  for (e = list_begin(&all_list); e != list_end(&all_list);
       e = list_next(e))
  {
    struct thread *t = list_entry(e, struct thread, allelem);
    func(t, aux);
  }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void thread_set_priority(int new_priority)
{
  if (!thread_mlfqs)
  {
    int thread_priority = thread_current()->priority;
    thread_current()->priority = new_priority;
    //refresh_priority();

    if (new_priority < thread_priority)
    {
      thread_yield();
    }
    //ready_and_running_priority();
  }
}

/* Returns the current thread's prioritfy. */
int thread_get_priority(void)
{
  return thread_current()->priority;
}

/* Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice UNUSED)
{
  /* Not yet implemented. */
  /* 현제 스레드의 nice값을 변경하는 함수를 구현하다.  해당 작업중에 인터럽트는 비활성화 해야 한다. */

  /* 현제 스레드의 nice 값을 변경한다.  nice 값 변경 후에 현재 스레드의 우선순위를 재계산 하고 우선순위에 의해 스케줄링 한다. */
  enum intr_level old_level = intr_disable();
  thread_current()->nice = nice;
  mlfqs_priority(thread_current());
  ready_and_running_priority();
  intr_set_level(old_level);
}

/* Returns the current thread's nice value. */
int thread_get_nice(void)
{
  /* 현재 스레드의 nice 값을 반환한다.  해당 작업중에 인터럽트는 비활성되어야 한다. */
  enum intr_level old_level;

  //old_level = intr_disable();
  int ret = thread_current()->nice;
  //intr_set_level(old_level);
  return ret;
}

/* Returns 100 times the system load average. */
int thread_get_load_avg(void)
{
  /* Not yet implemented. */
  enum intr_level old_level;

  //old_level = intr_disable();
  int ret = fp_to_int_round(mult_mixed(load_avg, 100));
  //intr_set_level(old_level);
  return ret;
}

/* Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void)
{
  enum intr_level old_level;

  //old_level = intr_disable();
  //printf("%d %d %d\n", thread_current()->recent_cpu, mult_mixed(thread_current()->recent_cpu, 100), fp_to_int(mult_mixed(thread_current()->recent_cpu, 100)));
  int ret = fp_to_int(mult_mixed(thread_current()->recent_cpu, 100));
  //intr_set_level(old_level);
  return ret;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle(void *idle_started_ UNUSED)
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current();
  sema_up(idle_started);

  for (;;)
  {
    /* Let someone else run. */
    intr_disable();
    thread_block();

    /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
    asm volatile("sti; hlt"
                 :
                 :
                 : "memory");
  }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread(thread_func *function, void *aux)
{
  ASSERT(function != NULL);

  intr_enable(); /* The scheduler runs with interrupts off. */
  function(aux); /* Execute the thread function. */
  thread_exit(); /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread(void)
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm("mov %%esp, %0"
      : "=g"(esp));
  return pg_round_down(esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread(struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread(struct thread *t, const char *name, int priority)
{
  ASSERT(t != NULL);
  ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT(name != NULL);

  memset(t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy(t->name, name, sizeof t->name);
  t->stack = (uint8_t *)t + PGSIZE;
  t->priority = priority;
  t->init_priority = priority;
  t->magic = THREAD_MAGIC;
  list_push_back(&all_list, &t->allelem);

  t->nice = running_thread()->nice;
  t->recent_cpu = running_thread()->recent_cpu;
#ifdef USERPROG
  t->parent = running_thread();
  sema_init(&(t->sema_exit), 0);
  sema_init(&(t->sema_wait), 0);
  sema_init(&(t->sema_load), 0);
  list_init(&(t->child)); // child list  initialize
  list_push_back(&(running_thread()->child), &(t->child_elem));

  //list_init(&(t->donations));

  for (int i = 0; i < 128; i++)
  {
    t->fd[i] = NULL;
  }
#endif
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame(struct thread *t, size_t size)
{
  /* Stack data is always allocated in word-size units. */
  ASSERT(is_thread(t));
  ASSERT(size % sizeof(uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run(void)
{
  if (list_empty(&ready_list))
    return idle_thread;
  else
    return list_entry(list_pop_front(&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void thread_schedule_tail(struct thread *prev)
{
  struct thread *cur = running_thread();

  ASSERT(intr_get_level() == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
  {
    ASSERT(prev != cur);
    palloc_free_page(prev);
  }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule(void)
{
  struct thread *cur = running_thread();
  struct thread *next = next_thread_to_run();
  struct thread *prev = NULL;

  ASSERT(intr_get_level() == INTR_OFF);
  ASSERT(cur->status != THREAD_RUNNING);
  ASSERT(is_thread(next));

  if (cur != next)
    prev = switch_threads(cur, next);
  thread_schedule_tail(prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid(void)
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire(&tid_lock);
  tid = next_tid++;
  lock_release(&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof(struct thread, stack);

void thread_sleep(int64_t ticks)
{
  /* 현재 스레드가 idle 스레드가 아닐경우
  thread의 상태를 BLOCKED로 바꾸고 깨어나야 할 ticks을 저장,
  슬립 큐에 삽입하고, awake함수가 실행되어야 할 tick값을 update */
  /* 현재 스레드를 슬립 큐에 삽입한 후에 스케줄한다. */
  /* 해당 과정중에는 인터럽트를 받아들이지 않는다. */

  enum intr_level old_level;
  old_level = intr_disable();
  struct thread *cur = thread_current();

  ASSERT(!intr_context());
  ASSERT(cur != idle_thread);
  cur->wake_up_tick = ticks;
  list_push_back(&sleep_list, &cur->elem);
  thread_block();
  intr_set_level(old_level);
}

void thread_awake(int64_t ticks)
{
  /* sleep list의 모든 entry 를 순회하며 다음과 같은 작업을 수행한다.
현재 tick이 깨워야 할 tick 보다 크거나 같다면 슬립 큐에서 제거하고
unblock 한다.
*/
  struct thread *cur;
  struct list_elem *find = list_begin(&sleep_list);
  struct list_elem *end = list_end(&sleep_list);

  while (find != end)
  {
    cur = list_entry(find, struct thread, elem);

    if (cur->wake_up_tick <= ticks)
    {
      find = list_remove(find);
      thread_unblock(cur);
    }
    else
      find = list_next(find);
  }
}

bool priority_setup(struct list_elem *a, struct list_elem *b, void *aux UNUSED)
{
  return list_entry(a, struct thread, elem)->priority > list_entry(b, struct thread, elem)->priority;
}

void ready_and_running_priority()
{

  if (list_empty(&ready_list))
  {
    return;
  }

  struct thread *ready = list_entry(list_begin(&ready_list), struct thread, elem);
  struct thread *cur = thread_current();
  if (cur->priority < ready->priority)
  {
    //printf("goging into thread_yield!\n");
    thread_yield();
  }
}

void thread_aging()
{
  struct thread *ready_process;
  struct list_elem *temp = list_begin(&(ready_list));
  struct list_elem *end = list_end(&(ready_list));

  while (temp != list_end(&(ready_list)))
  {
    ready_process = list_entry(temp, struct thread, elem);
    ready_process->priority += 1;
    temp = list_next(temp); //다음 원소로 이동
  }
}

void mlfqs_priority(struct thread *t)
{
  /*priority계산식을 구현 (fixed_point.h의 계산함수 이용)*/

  t->priority = fp_to_int(sub_fp(sub_fp(int_to_fp(PRI_MAX), div_mixed(t->recent_cpu, 4)), mult_mixed(int_to_fp(t->nice), 2)));

  if (t->priority > PRI_MAX)
  {
    t->priority = PRI_MAX;
  }
  if (t->priority < PRI_MIN)
  {
    t->priority = PRI_MIN;
  }
  if (thread_current()->priority < max_priority())
  {
    intr_yield_on_return();
  }
}

void mlfqs_recent_cpu(struct thread *t)
{
  /* 해당 스레드가 idle_thread 가 아닌지 검사 */
  /*recent_cpu계산식을 구현 (fixed_point.h의 계산함수 이용)*/
  if (thread_current() != idle_thread)
    t->recent_cpu = add_mixed(mult_fp(div_fp(mult_mixed(load_avg, 2), add_mixed(mult_mixed(load_avg, 2), 1)), t->recent_cpu), t->nice);
}

void mlfqs_load_avg(void)
{
  /* load_avg계산식을 구현 (fixed_point.h의 계산함수 이용) */
  /* load_avg 는 0 보다 작아질 수 없다.*/
  int ready_size = list_size(&ready_list);

  if (thread_current() != idle_thread)
    ready_size++;

  load_avg = div_mixed(add_mixed(mult_mixed(load_avg, 59), ready_size), 60);
}

void mlfqs_increment(void)
{
  /* 해당 스레드가 idle_thread 가 아닌지 검사 */
  /* 현재 스레드의 recent_cpu 값을 1증가 시킨다. */
  if (thread_current() != idle_thread)
    thread_current()->recent_cpu = add_mixed(thread_current()->recent_cpu, 1);
  //printf("the recent cpu is %d!\n", thread_current()->recent_cpu);
}

void mlfqs_recalc_recent_cpu(void)
{
  /* 모든 thread의 recent_cpㅕ 재계산 한다. */

  struct list_elem *cur = list_begin(&all_list);
  struct list_elem *end = list_end(&all_list);

  while (cur != end)
  {
    struct thread *temp = list_entry(cur, struct thread, allelem);
    mlfqs_recent_cpu(temp);
    cur = list_next(cur);
  }
}

void mlfqs_recalc_priority(void)
{
  /* 모든 thread의 recent_cpu와 priority값 재계산 한다. */

  struct list_elem *cur = list_begin(&all_list);
  struct list_elem *end = list_end(&all_list);

  while (cur != end)
  {
    struct thread *temp = list_entry(cur, struct thread, allelem);
    mlfqs_priority(temp);
    cur = list_next(cur);
  }
}

int max_priority()
{
  int priority = -1;
  struct thread *temp;
  if (!list_empty(&ready_list))
  {
    temp = list_entry(list_front(&ready_list), struct thread, elem);
    priority = temp->priority;
  }
  return priority;
}