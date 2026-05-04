#include "types.h"
#include "user.h"
#include "uthread.h"
#include "umutex.h"
 
void
mutex_init(umutex_t *m)
{
  m->locked = 0;
}
 
// Cooperative lock: spin-yield until the mutex is free.
// Safe without atomic ops because xv6 user threads are cooperative —
// only one thread runs at a time, so no torn read/write is possible.
void
mutex_lock(umutex_t *m)
{
  while(m->locked)
    thread_yield();
  m->locked = 1;
}
 
void
mutex_unlock(umutex_t *m)
{
  m->locked = 0;
}