#include <locks.h>
#include <print.h>

#include <ints.h>

void lock(struct spinlock *lock, struct node *self)
{
  //printf("lock() called \n");
  atomic_store_explicit(&self->next, 0, memory_order_relaxed);
  atomic_store_explicit(&self->wait, 0, memory_order_relaxed);


  struct node *old_next_tail = atomic_exchange_explicit(&lock->tail->next, self,
                        memory_order_acq_rel);

  if (!old_next_tail)
  {
    atomic_store_explicit(&lock->tail->wait, 1, memory_order_relaxed);
    return;
  }

  atomic_store_explicit(&old_next_tail->next, self, memory_order_relaxed);
  atomic_store_explicit(&self->wait, 1, memory_order_relaxed);

  while (atomic_load_explicit(&self->wait, memory_order_acquire));
}

void unlock(struct spinlock *lock, struct node *self)
{
  //printf("unlock() called \n");
  struct node *tail = self, *next;

    if (atomic_compare_exchange_strong_explicit(&lock->tail->next, &tail, 0,
          memory_order_release, memory_order_relaxed))
    {
	  atomic_store_explicit(&lock->tail->wait, 0, memory_order_relaxed);  
    }
    else 
    {
	  next = atomic_load_explicit(&self->next, memory_order_relaxed);
	  atomic_store_explicit(&next->wait, 0, memory_order_relaxed);
    }
    
	   
      return;
}