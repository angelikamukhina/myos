#include <locks.h>
#include <print.h>

void lock(struct spinlock *lock, struct node *self)
{
  atomic_store_explicit(&self->next, 0, memory_order_relaxed);
  atomic_store_explicit(&self->wait, 0, memory_order_relaxed);

  struct node *tail = atomic_exchange_explicit(&lock->tail, self,
                        memory_order_acq_rel);

  if (!tail || (!tail->next && tail->wait))
    return;

  atomic_store_explicit(&tail->next, self, memory_order_relaxed);
  while (!atomic_load_explicit(&self->wait, memory_order_acquire));
}

void unlock(struct spinlock *lock, struct node *self)
{
  struct node *tail = self, *next;

  if (!(next = atomic_load_explicit(&self->next, memory_order_relaxed))) {
    if (atomic_compare_exchange_strong_explicit(&lock->tail, &tail, 0,
          memory_order_release, memory_order_relaxed)) {
      return;
    }

    while (!(next = atomic_load_explicit(&self->next,
                      memory_order_relaxed)));
  }
  atomic_store_explicit(&next->wait, 0, memory_order_release);
}
