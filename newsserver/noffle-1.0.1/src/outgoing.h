/*
  outgoing.h

  Collection of posted articles.

  $Id: outgoing.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef OUT_H
#define OUT_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "dynamicstring.h"

Bool
Out_add( const char *serv, const char *msgId, const DynStr *artTxt );

/* Start enumeration. Return TRUE on success. */
Bool
Out_first( const char *serv, Str msgId, DynStr *artTxt );

/* Continue enumeration. Return TRUE on success. */
Bool
Out_next( Str msgId, DynStr *s );

/* Delete article from outgoing collection */
void
Out_remove( const char *serv, const char *msgId );

/* Find server for outgoing message. */
Bool
Out_find( const char *msgId, Str server );

#endif
