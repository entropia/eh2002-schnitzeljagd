/*
  request.h

  Collection of requested articles.

  $Id: request.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef REQ_H
#define REQ_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

/* Is request for message msgId from server serv already recorded? This
   function has no error detection facility. On error, FALSE is returned.
   Nevertheless, errors are logged. */
Bool
Req_contains( const char *serv, const char *msgId );

/* Add request for message "msgId" from server "serv". Return TRUE if
   successful. */ 
Bool
Req_add( const char *serv, const char *msgId );

/* Remove request for message msgIg from server serv. This function does
   not return any errors. Nevertheless, they are logged. */
void
Req_remove( const char *serv, const char *msgId );

/* Begin iteration through all messages requested from one server. Return
   TRUE if there are any requests. Save first message ID in msgId. On
   error, it is logged, and FALSE is returned.
*/
Bool
Req_first( const char *serv, Str msgId );

/* Continue iteration. Return TRUE on success, FALSE when there are no more
   requests. Save message ID in msgId. On error, it is logged, and FALSE is
   returned. */
Bool
Req_next( Str msgId );

/* Get exclusive access to the request files. Refresh cache as necessary. */
Bool 
Req_open(void);

/* Write changes to disk */
void
Req_close(void);

#endif
