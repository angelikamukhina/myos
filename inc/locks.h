#ifndef __LOCKS_H__
#define __LOCKS_H__

#include <stdatomic.h>

#define LOCK_NODE_INIT { 0, 0 }
#define LOCK_INIT(nodename) { &(nodename) }

typedef struct node {
  struct node * _Atomic next;
  atomic_int wait;
} spinlock_node_t;

typedef struct spinlock {
  struct node * _Atomic tail;
} spinlock_t;

void lock(spinlock_t *lock, spinlock_node_t *self);
void unlock(spinlock_t *lock, spinlock_node_t *self);

#endif /*__LOCKS_H__*/