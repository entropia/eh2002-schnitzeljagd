/*
  itemlist.h

  Copy a string wiht a list of separated items (as found in several
  header lines) and provide a convenient way of accessing the
  individual items.
  
  $Id: itemlist.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $ */

#ifndef ITEMLIST_H
#define ITEMLIST_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>

struct ItemList;
typedef struct ItemList ItemList;

/* Make a new item list. */
ItemList *
new_Itl( const char *list, const char *separators );

/* Delete an item list. */
void
del_Itl( ItemList *self );

/* Get first item. */
const char *
Itl_first( ItemList *self);

/* Get next item or NULL. */
const char *
Itl_next( ItemList *self );

/* Get count of items in list. */
size_t
Itl_count( const ItemList *self );

#endif
