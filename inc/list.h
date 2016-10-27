#ifndef __LIST_H__
#define __LIST_H__

#include <stdbool.h>
#include <stddef.h>

//list el structure
struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

//offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define CONTAINER_OF(ptr, type, member) \
	(type *)( (char *)(ptr) - offsetof(type, member) )

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

#define LIST_ENTRY(ptr, type, member) CONTAINER_OF(ptr, type, member)

//iterator
#define list_for_each(pos, head) \
  for (pos = (head)->next; pos != (head);	\
       pos = pos->next)

void list_init(struct list_head *head);
void list_add(struct list_head *new, struct list_head *head);
bool list_empty(const struct list_head *head);
void list_del_init(struct list_head *entry);

#endif /*__LIST_H__*/
