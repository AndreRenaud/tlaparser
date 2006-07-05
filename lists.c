#include <stdio.h>
#include <stdlib.h>

#include "lists.h"

static list_t *list_create ()
{
   list_t *ret;

   ret = malloc (sizeof (list_t));
   ret->next = NULL;
   ret->prev = NULL;
   ret->data = NULL;

   return ret;
}

list_t *list_prepend (list_t *l, void *data)
{
   list_t *n;

   if (!data) // skip it
      return NULL;

   n = list_create ();
   n->next = l;
   n->data = data;

   if (l)
      l->prev = n;

   return n;
}

static list_t *list_get_last (list_t *l)
{
   if (!l)
      return NULL;

   while (l->next)
      l=l->next;

   return l;
}

list_t *list_append (list_t *l, void *data)
{
   list_t *n;
   list_t *orig = l;

   if (!data) // skip it
      return l;

   n = list_create ();
   n->data = data;
   l = list_get_last (l);

   if (l) // list already exists
   {
      n->prev = l;
      l->next = n;
      return orig;
   }
   else
      return n;
}

list_t *list_list_append (list_t *l, list_t *l2)
{
   list_t *orig = l;

   l = list_get_last (l);
   if (l)
   {
      l->next = l2;
      if (l2)
         l2->prev = l;

      return orig;
   }
   else
      return l2;
}

void lists_tests (void)
{
   list_t *l = NULL;
   list_t *n;
   
   l = list_prepend (l, (void *)2);
   l = list_append (l, (void *)3);
   l = list_prepend (l, (void *)4);
   l = list_append (l, (void *)5);

   for (n = l; n != NULL; n = n->next)
      printf ("list: %d\n", (int)n->data);
}
