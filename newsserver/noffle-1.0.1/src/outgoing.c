/*
  outgoing.c

  $Id: outgoing.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "outgoing.h"

#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

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
#include "util.h"
#include "portable.h"

struct Outgoing
{
    DIR *dir;
    Str serv;
} outgoing = { NULL, "" };

static void
fileOutgoing( Str file, const char *serv, const char *msgId )
{
    snprintf( file, MAXCHAR, "%s/outgoing/%s/%s",
              Cfg_spoolDir(), serv, msgId );
}

static void
createDir( const char *serv )
{
    Str dir;
    int r;

    snprintf( dir, MAXCHAR, "%s/outgoing/%s", Cfg_spoolDir(), serv );
    r = mkdir( dir, 0755 );
    if ( r != 0 )
        Log_dbg( "mkdir: %s", strerror( errno ) );
}

Bool
Out_add( const char *serv, const char *msgId, const DynStr *artTxt )
{
    Str file;
    FILE *f;

    fileOutgoing( file, serv, msgId );
    if ( ! ( f = fopen( file, "w" ) ) )
    {
        createDir( serv );
        if ( ! ( f = fopen( file, "w" ) ) )
        {
            Log_err( "Cannot open %s", file );
            return FALSE;
        }
    }
    fprintf( f, "%s", DynStr_str( artTxt ) );
    fclose( f );
    return TRUE;
}

Bool
Out_first( const char *serv, Str msgId, DynStr *artTxt )
{
    Str file;
    
    snprintf( file, MAXCHAR, "%s/outgoing/%s", Cfg_spoolDir(), serv );
    if ( ! ( outgoing.dir = opendir( file ) ) )
    {
        Log_dbg( "Cannot open %s", file );
        return FALSE;
    }
    Utl_cpyStr( outgoing.serv, serv );
    Out_next( NULL, NULL ); /* "."  */
    Out_next( NULL, NULL ); /* ".." */
    return Out_next( msgId, artTxt );
}

Bool
Out_next( Str msgId, DynStr *artTxt )
{
    struct dirent *d;
    FILE *f;
    Str file, line;

    ASSERT( outgoing.dir );
    if ( ! ( d = readdir( outgoing.dir ) ) )
    {
        closedir( outgoing.dir );
        outgoing.dir = NULL;
        return FALSE;
    }
    if ( artTxt == NULL )
        return ( d->d_name != NULL );
    fileOutgoing( file, outgoing.serv, d->d_name );
    if ( ! ( f = fopen( file, "r" ) ) )
    {
        Log_err( "Cannot open %s for read", file );
        return FALSE;
    }
    DynStr_clear( artTxt );
    while ( fgets( line, MAXCHAR, f ) )
        DynStr_app( artTxt, line );
    Utl_cpyStr( msgId, d->d_name );
    fclose( f );
    return TRUE;
}

void
Out_remove( const char *serv, const char *msgId )
{
    Str file;

    fileOutgoing( file, serv, msgId );
    if ( unlink( file ) != 0 )
        Log_err( "Cannot remove %s", file );
}

Bool
Out_find( const char *msgId, Str server )
{
    Str servdir;
    DIR *d;
    struct dirent *entry;
    Bool res;
    
    
    snprintf( servdir, MAXCHAR, "%s/outgoing", Cfg_spoolDir() );
    if ( ! ( d = opendir( servdir ) ) )
    {
        Log_dbg( "Cannot open %s", servdir );
        return FALSE;
    }

    readdir( d );	/* '.' */
    readdir( d );	/* '..' */

    res = FALSE;
    while ( ! res && ( entry = readdir( d ) ) != NULL )
    {
	Str file;
	struct stat s;

	fileOutgoing( file, entry->d_name, msgId );
	if ( stat( file, &s ) == 0 )
	{
	    res = TRUE;
	    Utl_cpyStr( server, entry->d_name );
	}
    }

    closedir( d );
    return res;
}





