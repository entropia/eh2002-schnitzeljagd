/*
  configfile.h

  Common declarations and handling of the configuration file.

  $Id: configfile.h,v 1.1 2002-03-28 17:41:24 dividuum Exp $
*/

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

const char * Cfg_spoolDir( void );
const char * Cfg_version( void );

int Cfg_maxFetch( void );
int Cfg_autoUnsubscribeDays( void );
int Cfg_threadFollowTime( void );
int Cfg_connectTimeout( void );
Bool Cfg_autoUnsubscribe( void );
Bool Cfg_autoSubscribe( void );
Bool Cfg_infoAlways( void );

/* Ignored. Should be removed in development version. */
Bool Cfg_removeMsgId( void );

Bool Cfg_replaceMsgId( void );
const char * Cfg_autoSubscribeMode( void ); /* Can be: full, thread, over */
const char * Cfg_mailTo( void );

/* Begin iteration through the server names */
void Cfg_beginServEnum( void );

/* Save next server name in "name". Return TRUE if name has been was saved.
   Return FALSE if there are no more server names. */
Bool Cfg_nextServ( Str name );

Bool Cfg_servListContains( const char *name );
/* Prefer server earlier in config file. Known servers are always preferential
   to unknown servers. */
Bool Cfg_servIsPreferential( const char *name1, const char *name2 );
void Cfg_authInfo( const char *name, Str user, Str pass );

/* Begin iteration through expire entries. */
void Cfg_beginExpireEnum( void );

/* Put next expire pattern in "pattern" and return its days count.
   Return -1 if no more expire patterns. */
int Cfg_nextExpire( Str pattern );

/* Return default expire days. */
int Cfg_expire( void );

void Cfg_read( void );

#endif
