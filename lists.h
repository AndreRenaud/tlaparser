#ifndef LIST_H
#define LIST_H

struct list_t
{
   struct list_t *next;
   struct list_t *prev;
   void *data;
};

typedef struct list_t list_t;

list_t *list_append (list_t *l, void *data);
list_t *list_prepend (list_t *l, void *data);

list_t *list_list_append (list_t *l, list_t *l2);

void lists_tests (void);

#endif
