#pragma once

typedef int tid_t;
void thread_init(void);
tid_t thread_create(void (*fn)(void*), void *arg);
void thread_yield(void);
int thread_join(tid_t tid);
