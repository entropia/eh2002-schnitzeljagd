/*
  lock.c

  $Id: lock.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "lock.h"
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

#include <unistd.h>
#include "configfile.h"
#include "log.h"
#include "database.h"
#include "group.h"
#include "request.h"
#include "portable.h"

struct Lock
{
    int lockFd;
    Str lockFile;
} lock = { -1, "" };


#ifdef DEBUG
static Bool
testLock( void )
{
    return ( lock.lockFd != -1 );    
}
#endif

static Bool
waitLock( void )
{
    int fd;
    struct flock l;

    ASSERT( ! testLock() );
    Log_dbg( "Waiting for lock ..." );
    snprintf( lock.lockFile, MAXCHAR, "%s/lock/global", Cfg_spoolDir() );
    if ( ( fd = open( lock.lockFile, O_WRONLY | O_CREAT, 0644 ) ) < 0 )
    {
        Log_err( "Cannot open %s (%s)", lock.lockFile, strerror( errno ) );
        return FALSE;
    }
    l.l_type = F_WRLCK;
    l.l_start = 0;
    l.l_whence = SEEK_SET;
    l.l_len = 0;
    if ( fcntl( fd, F_SETLKW, &l ) < 0 )
    {
        Log_err( "Cannot lock %s: %s", lock.lockFile, strerror( errno ) );
        return FALSE;
    }
    lock.lockFd = fd;
    Log_dbg( "Lock successful" );
    return TRUE;
}

static void
releaseLock( void )
{
    struct flock l;

    ASSERT( testLock() );    
    l.l_type = F_UNLCK;
    l.l_start = 0;
    l.l_whence = SEEK_SET;
    l.l_len = 0;
    if ( fcntl( lock.lockFd, F_SETLK, &l ) < 0 )
        Log_err( "Cannot release %s: %s", lock.lockFile,
                 strerror( errno ) );
    close( lock.lockFd );
    lock.lockFd = -1;
    Log_dbg( "Releasing lock" );
}


/* Open all databases and set global lock. */
Bool
Lock_openDatabases( void )
{
  if ( ! waitLock() )
    {
      Log_err( "Could not get write lock" );
      return FALSE;
    }
  if ( ! Db_open() )
    {
      Log_err( "Could not open database" );
      releaseLock();
      return FALSE;
    }
  if ( ! Grp_open() )
    {
      Log_err( "Could not open groupinfo" );
      Db_close();
      releaseLock();
      return FALSE;
    }
  if ( ! Req_open() )
    {
      Log_err( "Could not initialize request database" );
      Grp_close();
      Db_close();
      releaseLock();
      return FALSE;
    }

  return TRUE;
}


/* Close all databases and release global lock. */
void
Lock_closeDatabases( void )
{
  Grp_close();
  Db_close();
  Req_close();
  releaseLock();
}
