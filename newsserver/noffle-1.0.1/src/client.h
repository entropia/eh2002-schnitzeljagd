/*
  client.h

  Noffle acting as client to other NNTP-servers

  $Id: client.h,v 1.1 2002-03-28 17:41:24 dividuum Exp $
*/

#ifndef CLIENT_H
#define CLIENT_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include "common.h"
#include "database.h"
#include "fetchlist.h"

/* Format of server name: <host>[:<port>] */
Bool
Client_connect( const char *serv );

void
Client_disconnect( void );

Bool
Client_getGrps( void );

Bool
Client_getDsc( void );

Bool
Client_getNewgrps( const time_t *lastTime );

/*
  Change to group <name> at server if it is also in current local grouplist.
  Returns TRUE at success.
*/
Bool
Client_changeToGrp( const Str name );

/*
  Get overviews <rmtFirst> - <rmtLast> from server and append it
  to the current content. For articles that are to be fetched due to FULL
  or THREAD mode, store IDs in request database.
*/
Bool
Client_getOver( int rmtFirst, int rmtLast, FetchMode mode );

/*
  Retrieve full article text and store it into database.
*/
void
Client_retrieveArt( const char *msgId );

/*
  Same, but for a list of msgId's (new line after each msgId).
  All ARTICLE commands are sent and then all answers read.
*/
void
Client_retrieveArtList( const char *list );

/*
  Store IDs of first and last article of group selected by
  Client_changeToGroup at remote server. 
*/
void
Client_rmtFirstLast( int *first, int *last );

Bool
Client_postArt( const char *msgId, const char *artTxt, Str errStr );

#endif
