/*
  database.h

  Article database.

  $Id: database.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef DB_H
#define DB_H

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
#include "dynamicstring.h"
#include "over.h"

/* Article status flags: */
#define DB_INTERESTING       0x01  /* Was article ever tried to read? */
#define DB_NOT_DOWNLOADED    0x02  /* Not fully downloaded */
#define DB_RETRIEVING_FAILED 0x04  /* Retrieving of article failed */

/* Open database for r/w. Locking must be done by the caller! */
Bool
Db_open( void );

void
Db_close( void );

/*
  Creates an database entry for the article from the overview
  information. Xref is replaced by grp:numb.
*/
Bool
Db_prepareEntry( const Over *ov, const char *grp, int numb );

/* Store full article. Can only be used after Db_prepareEntry. */
Bool
Db_storeArt( const char *msgId, const char *artTxt );

void
Db_setStatus( const char *msgId, int status );

void
Db_updateLastAccess( const char *msgId );

/* Xref header line without hostname */
void
Db_setXref( const char *msgId, const char *xref );

const char *
Db_header( const char *msgId );

const char *
Db_body( const char *msgId );

int
Db_status( const char *msgId );

/* Get last modification time of entry. Returns -1, if msgId non-existing. */
time_t
Db_lastAccess( const char *msgId );

/* Value of the References header line */
const char *
Db_ref( const char *msgId );

/* Value of the From header line */
const char *
Db_from( const char *msgId );

/* Value of the Date header line */
const char *
Db_date( const char *msgId );

/* Xref header line without hostname */
const char *
Db_xref( const char *msgId );

/* Overview - need to del_Over result when finished with */
Over *
Db_over( const char *msgId );

Bool
Db_contains( const char *msgId );

/* Delete entry from database */
void
Db_delete( const char *msgId );

Bool
Db_first( const char** msgId );

Bool
Db_next( const char** msgId );

/*
  Expire all articles that have not been accessed for a number of
  days determined by their group membership and noffle configuration.
 */
Bool
Db_expire( void );

#endif
