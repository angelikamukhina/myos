#include "list.h"

void list_init(struct list_head *head)
{ head->next = head->prev = head; }

static void list_insert(struct list_head *new, struct list_head *prev,
			struct list_head *next)
{
	new->prev = prev;
	new->next = next;
	prev->next = new;
	next->prev = new;
}

void list_add(struct list_head *new, struct list_head *head)
{ list_insert(new, head, head->next); }

bool list_empty(const struct list_head *head)
{ return head->next == head; }

void list_del_init(struct list_head *entry)
{
	//remove from list
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	//reinit
	INIT_LIST_HEAD(entry);
}