#ifndef __LIST_H__
#define __LIST_H__

#include <kernel.h>

typedef struct list_head {
	struct list_head *next;
	struct list_head *prev;
} list_head_t;

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)
#define LIST_ENTRY(ptr, type, member) CONTAINER_OF(ptr, type, member)

//iterator
#define list_for_each(pos, head) \
  for (pos = (head)->next; pos != (head);	\
       pos = pos->next)

void list_init(struct list_head *head);
void list_add(struct list_head *new, struct list_head *head);

static inline void list_insert_after(struct list_head *new,
			struct list_head *node)
{ list_add(new, node); }

void list_add_tail(struct list_head *new, struct list_head *head);

static inline void list_insert_before(struct list_head *new,
			struct list_head *node)
{ list_add_tail(new, node); }

void list_del(struct list_head *entry);
void list_splice(struct list_head *list, struct list_head *head);
int list_empty(const struct list_head *head);
struct list_head *list_first(struct list_head *head);
size_t list_size(const struct list_head *head);

#endif /*__LIST_H__*/
