/*
  fetchlist.h

  List of groups that are to be fetched presently.

  $Id: fetchlist.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef FETCHLIST_H
#define FETCHLIST_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

typedef enum { FULL, THREAD, OVER } FetchMode;

void
Fetchlist_read( void );

/* Invalidates any indices (list is sorted by name before saving) */
Bool
Fetchlist_write( void );

int
Fetchlist_size( void );

Bool
Fetchlist_contains( const char *name );

/* Get element number index. */
Bool
Fetchlist_element( const char **name, FetchMode *mode, int idx );

/* Add entry. Invalidates any indices. Returns TRUE if new entry, FALSE if
   entry was overwritten. */
Bool
Fetchlist_add( const char *name, FetchMode mode );

/* Remove entry. Invalidates any indices. Returns FALSE if not found. */
Bool
Fetchlist_remove( const char *name );

#endif
