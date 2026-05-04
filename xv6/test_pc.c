#include "types.h"
#include "stat.h"
#include "user.h"
#include "uthread.h"
#include "umutex.h"

#define NBUFSLOTS 8
#define NITEMS    200

static int buf[NBUFSLOTS];
static int head = 0;
static int tail = 0;
static int count = 0;
static umutex_t mu;

static void
producer(void *arg)
{
  int id = (int)arg;
  int i;

  for(i = 0; i < NITEMS; i++){
    for(;;){
      mutex_lock(&mu);
      if(count < NBUFSLOTS){
        buf[tail] = id * 100000 + i;
        tail = (tail + 1) % NBUFSLOTS;
        count++;
        mutex_unlock(&mu);
        break;
      }
      mutex_unlock(&mu);
      thread_yield();
    }
  }
}

static void
consumer(void *arg)
{
  int item;
  int got;

  (void)arg;
  got = 0;

  while(got < 2 * NITEMS){
    for(;;){
      mutex_lock(&mu);
      if(count > 0){
        item = buf[head];
        head = (head + 1) % NBUFSLOTS;
        count--;
        mutex_unlock(&mu);
        got++;
        if((got % 100) == 0)
          printf(1, "consumer got %d items (last=%d)\n", got, item);
        break;
      }
      mutex_unlock(&mu);
      thread_yield();
    }
  }
}

int
main(void)
{
  tid_t p1, p2, c1;

  thread_init();
  mutex_init(&mu);

  p1 = thread_create(producer, (void*)1);
  p2 = thread_create(producer, (void*)2);
  c1 = thread_create(consumer, 0);

  if(p1 < 0 || p2 < 0 || c1 < 0){
    printf(1, "thread_create failed\n");
    exit();
  }

  thread_join(p1);
  thread_join(p2);
  thread_join(c1);

  printf(1, "test_pc: done\n");
  exit();
}