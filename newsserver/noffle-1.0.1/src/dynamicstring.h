/*
  dynamicstring.h

  String utilities

  $Id: dynamicstring.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef DYNAMICSTRING_H
#define DYNAMICSTRING_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>

/* A dynamically growing string */
struct DynStr;
typedef struct DynStr DynStr;

/* Create new DynStr with given capacity */
DynStr *
new_DynStr( int reserve );

/* Delete DynStr */
void
del_DynStr( DynStr *self );

/* Return DynStr's length */
int
DynStr_len( const DynStr *self );

/* Return DynStr's content ptr */
const char *
DynStr_str( const DynStr *self );

/* append C-string to DynStr */
void
DynStr_app( DynStr *self, const char *s );

/* append a DynStr to DynStr */
void
DynStr_appDynStr( DynStr *self, const DynStr *s );

/* Append C-string + newline to DynStr */
void
DynStr_appLn( DynStr *self, const char *s );

/* Append a maximum of n characters from C-string s to DynStr self */
void
DynStr_appN( DynStr *self, const char *s, int n );

/* Truncate content of DynString to zero length */
void
DynStr_clear( DynStr *self );

#endif
