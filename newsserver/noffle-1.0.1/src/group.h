/*
  group.h

  Groups database

  $Id: group.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef GRP_H
#define GRP_H

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

#define GRP_LOCAL_SERVER_NAME "(local)"

/* open group database */
Bool
Grp_open( void );

/* close group database */
void
Grp_close( void );

/* does group exist? */
Bool
Grp_exists( const char *name );

/* is it a local group? */
Bool
Grp_local( const char *name );

/* create new group and save it in database */
void
Grp_create( const char *name );

/* delete a group and its articles from the database. */
void
Grp_delete( const char *name );

/* Get group description */
const char *
Grp_dsc( const char *name );

/* Get server the group resides on */
const char *
Grp_server( const char *name );

/*
  Get article number of the first article in the group
  This number is a hint only, it is independent of the
  real articles in content.c
*/
int
Grp_first( const char *name );

/*
  Get article number of the last article in the group
  This number is a hint only, it is independent of the
  real articles in content.c
*/
int
Grp_last( const char *name );

int
Grp_lastAccess( const char *name );

int
Grp_rmtNext( const char *name );

time_t
Grp_created( const char *name );

char
Grp_postAllow( const char *name );

/* Replace group's description (only if value != ""). */
void
Grp_setDsc( const char *name, const char *value );

void
Grp_setLocal( const char *name );

void
Grp_setServ( const char *name, const char *value );

void
Grp_setRmtNext( const char *name, int value );

void
Grp_setLastAccess( const char *name, int value );

void
Grp_setFirstLast( const char *name, int first, int last );

void
Grp_setPostAllow( const char *name, char postAllow );

/* Begin iterating trough the names of all groups. Store name of first
   group (or NULL if there aren't any) in name. Returns whether there are
   any groups. */
Bool
Grp_firstGrp( const char **name );

/* Continue iterating trough the names of all groups. Store name of next
   group (or NULL if there aren't any more) in name. Returns TRUE on
   success, FALSE when there are no more groups. */
Bool
Grp_nextGrp( const char **name );

#endif
