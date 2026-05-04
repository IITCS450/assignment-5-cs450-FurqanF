#include "types.h"
#include "stat.h"
#include "user.h"

typedef int tid_t;

// Forward declarations
struct context;
void uswtch(struct context **old, struct context *nw);
void thread_yield(void);
tid_t thread_create(void (*fn)(void*), void *arg);
int   thread_join(tid_t tid);

// ---------------------------------------------------------------
// Context layout — must match push order in uswtch.S:
//   pushl %ebp  -> highest address in frame
//   pushl %ebx
//   pushl %esi
//   pushl %edi  -> lowest address  (ctx points here)
//   [ret pops eip from ctx+16]
// ---------------------------------------------------------------
struct context {
  uint edi;   // offset  0
  uint esi;   // offset  4
  uint ebx;   // offset  8
  uint ebp;   // offset 12
  uint eip;   // offset 16  <- ret jumps here
};

#define MAX_THREADS  16
#define STACK_SIZE   4096

typedef enum {
  T_FREE = 0,
  T_RUNNABLE,
  T_RUNNING,
  T_ZOMBIE
} tstate_t;

struct thread {
  tstate_t        state;
  struct context *ctx;
  char           *stack;
  tid_t           tid;
};

static struct thread threads[MAX_THREADS];
static int           current;

// ---------------------------------------------------------------
// thread_stub
//
// When uswtch does `ret` into a fresh thread the stack looks like:
//   esp+0  : fake return address (0)
//   esp+4  : fn   (first  argument)
//   esp+8  : arg  (second argument)
// ---------------------------------------------------------------
static void
thread_stub(void (*fn)(void*), void *arg)
{
  fn(arg);
  threads[current].state = T_ZOMBIE;
  thread_yield();
  exit();
}

void
thread_init(void)
{
  int i;
  for(i = 0; i < MAX_THREADS; i++)
    threads[i].state = T_FREE;

  threads[0].state = T_RUNNING;
  threads[0].stack = 0;
  threads[0].tid   = 0;
  threads[0].ctx   = 0;
  current = 0;
}

// ---------------------------------------------------------------
// thread_create
//
// Initial stack layout (top = stack+STACK_SIZE):
//   [arg]              <- second param for thread_stub
//   [fn]               <- first  param for thread_stub
//   [0]                <- fake return address
//   [struct context]   <- edi,esi,ebx,ebp,eip=thread_stub
//   ^-- ctx (saved sp)
// ---------------------------------------------------------------
tid_t
thread_create(void (*fn)(void*), void *arg)
{
  int i;
  struct thread *t = 0;
  char *sp;
  struct context *ctx;

  for(i = 1; i < MAX_THREADS; i++){
    if(threads[i].state == T_FREE){
      t = &threads[i];
      break;
    }
  }
  if(t == 0) return -1;

  t->stack = malloc(STACK_SIZE);
  if(t->stack == 0) return -1;

  sp = t->stack + STACK_SIZE;

  // Second parameter: arg
  sp -= 4;
  *(uint*)sp = (uint)arg;

  // First parameter: fn
  sp -= 4;
  *(uint*)sp = (uint)fn;

  // Fake return address (thread_stub never returns normally)
  sp -= 4;
  *(uint*)sp = 0;

  // Context frame
  sp -= sizeof(struct context);
  ctx = (struct context*)sp;
  ctx->edi = 0;
  ctx->esi = 0;
  ctx->ebx = 0;
  ctx->ebp = 0;
  ctx->eip = (uint)thread_stub;

  t->ctx   = ctx;
  t->tid   = i;
  t->state = T_RUNNABLE;

  return t->tid;
}

static int
pick_next(void)
{
  int i, idx;
  for(i = 1; i <= MAX_THREADS; i++){
    idx = (current + i) % MAX_THREADS;
    if(threads[idx].state == T_RUNNABLE)
      return idx;
  }
  return -1;
}

void
thread_yield(void)
{
  int next;
  struct thread *old, *nw;

  if(threads[current].state == T_RUNNING)
    threads[current].state = T_RUNNABLE;

  next = pick_next();
  if(next < 0){
    threads[current].state = T_RUNNING;
    return;
  }

  old = &threads[current];
  nw  = &threads[next];

  nw->state = T_RUNNING;
  current   = next;

  uswtch(&old->ctx, nw->ctx);
}

int
thread_join(tid_t tid)
{
  int i;
  struct thread *t = 0;

  for(i = 0; i < MAX_THREADS; i++){
    if(threads[i].tid == tid && threads[i].state != T_FREE){
      t = &threads[i];
      break;
    }
  }
  if(t == 0) return -1;

  while(t->state != T_ZOMBIE)
    thread_yield();

  if(t->stack)
    free(t->stack);
  t->stack = 0;
  t->ctx   = 0;
  t->state = T_FREE;
  return 0;
}