/*
  group.c

  The group database resides in groupinfo.gdbm and stores all we know about
  the groups we know of. One database record is cached in the global struct
  grp. Group information is transfered between the grp and the database by
  loadGrp() and saveGrp(). This is done transparently. Access to the groups
  database is done by group name, by the functions defined in group.h.        

  $Id: group.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "group.h"
#include <gdbm.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "configfile.h"
#include "log.h"
#include "util.h"
#include "portable.h"

/* currently only used within grp */
typedef struct
{
    int first;		/* number of first article within group */
    int last;		/* number of last article within group */
    int rmtNext;
    time_t created;
    time_t lastAccess;
} Entry;

struct
{
    Str name;		/* name of the group */
    Entry entry;	/* more information about this group */
    Str serv;		/* server the group resides on */
    Str dsc;		/* description of the group */
    char postAllow;	/* Posting status */
    GDBM_FILE dbf;
} grp = { "(no grp)", { 0, 0, 0, 0, 0 }, "", "", ' ', NULL };

/*
  Note: postAllow should really go in Entry. But changing Entry would
  make backwards group file format capability tricky, so it goes
  where it is, and we test the length of the retrieved record to
  determine if it exists.

  Someday if we really change the record format this should be tidied up.
 */

static const char *
errMsg( void )
{
    if ( errno != 0 )
        return strerror( errno );
    return gdbm_strerror( gdbm_errno );
}

Bool
Grp_open( void )
{
    Str name;
    int flags;

    ASSERT( grp.dbf == NULL );
    snprintf( name, MAXCHAR, "%s/data/groupinfo.gdbm", Cfg_spoolDir() );
    flags = GDBM_WRCREAT | GDBM_FAST;
    if ( ! ( grp.dbf = gdbm_open( name, 512, flags, 0644, NULL ) ) )
    {
        Log_err( "Error opening %s for r/w (%s)", errMsg() );
        return FALSE;
    }
    Log_dbg( "%s opened for r/w", name );
    return TRUE;
}

void
Grp_close( void )
{
    ASSERT( grp.dbf );
    Log_dbg( "Closing groupinfo" );
    gdbm_close( grp.dbf );
    grp.dbf = NULL;
    Utl_cpyStr( grp.name, "" );
}

/* Load group info from gdbm-database into global struct grp */
static Bool
loadGrp( const char *name )
{
    const char *p;
    datum key, val;

    ASSERT( grp.dbf );
    if ( strcmp( grp.name, name ) == 0 )
         return TRUE;
    key.dptr = (void *)name;
    key.dsize = strlen( name ) + 1;
    val = gdbm_fetch( grp.dbf, key );
    if ( val.dptr == NULL )
        return FALSE;
    grp.entry = *( (Entry *)val.dptr );
    p = val.dptr + sizeof( grp.entry );
    Utl_cpyStr( grp.serv, p );
    p += strlen( p ) + 1;
    Utl_cpyStr( grp.dsc, p );
    p += strlen( p) + 1;
    if ( p - val.dptr < val.dsize )
	grp.postAllow = p[ 0 ];
    else
	grp.postAllow = 'y';
    Utl_cpyStr( grp.name, name );
    free( val.dptr );
    return TRUE;
}

/* Save group info from global struct grp into gdbm-database */
static void
saveGrp( void )
{
    size_t lenServ, lenDsc, bufLen;
    datum key, val;
    void *buf;
    char *p;

    ASSERT( grp.dbf );
    lenServ = strlen( grp.serv );
    lenDsc = strlen( grp.dsc );
    bufLen = sizeof( grp.entry ) + lenServ + lenDsc + 2 + sizeof( char );
    buf = malloc( bufLen );
    memcpy( buf, (void *)&grp.entry, sizeof( grp.entry ) );
    p = (char *)buf + sizeof( grp.entry );
    strcpy( p, grp.serv );
    p += lenServ + 1;
    strcpy( p, grp.dsc );
    p += lenDsc + 1;
    p[ 0 ] = grp.postAllow;
    key.dptr = (void *)grp.name;
    key.dsize = strlen( grp.name ) + 1;
    val.dptr = buf;
    val.dsize = bufLen;
    if ( gdbm_store( grp.dbf, key, val, GDBM_REPLACE ) != 0 )
        Log_err( "Could not save group %s: %s", errMsg() );
    free( buf );
}

Bool
Grp_exists( const char *name )
{
    datum key;

    ASSERT( grp.dbf );
    key.dptr = (void*)name;
    key.dsize = strlen( name ) + 1;
    return gdbm_exists( grp.dbf, key );
}

Bool
Grp_local( const char *name )
{
    if ( ! loadGrp( name ) )
        return 0;
    return ( strcmp( grp.serv, GRP_LOCAL_SERVER_NAME ) == 0 );
}

void
Grp_create( const char *name )
{
    Utl_cpyStr( grp.name, name );
    Utl_cpyStr( grp.serv, "(unknown)" );
    grp.dsc[ 0 ] = '\0';
    grp.entry.first = 1;
    grp.entry.last = 0;
    grp.entry.rmtNext = 0;
    grp.entry.created = time( NULL );
    grp.entry.lastAccess = 0;
    grp.postAllow = 'y';
    saveGrp();
}

void
Grp_delete( const char *name )
{
    datum key;

    ASSERT( grp.dbf );
    key.dptr = (void*)name;
    key.dsize = strlen( name ) + 1;
    gdbm_delete( grp.dbf, key );
}

const char *
Grp_dsc( const char *name )
{
    if ( ! loadGrp( name ) )
        return NULL;
    return grp.dsc;
}

const char *
Grp_server( const char *name )
{
    static Str serv = "";

    if ( ! loadGrp( name ) )
        return "[unknown grp]";
    if ( Cfg_servListContains( grp.serv )
         || Grp_local( name ) )
        Utl_cpyStr( serv, grp.serv );
    else
        snprintf( serv, MAXCHAR, "[%s]", grp.serv );
    return serv;
}

int
Grp_first( const char *name )
{
    if ( ! loadGrp( name ) )
        return 0;
    return grp.entry.first;
}

int
Grp_last( const char *name )
{
    if ( ! loadGrp( name ) )
        return 0;
    return grp.entry.last;
}

int
Grp_lastAccess( const char *name )
{
    if ( ! loadGrp( name ) )
        return 0;
    return grp.entry.lastAccess;
}

int
Grp_rmtNext( const char *name )
{
    if ( ! loadGrp( name ) )
        return 0;
    return grp.entry.rmtNext;
}

time_t
Grp_created( const char *name )
{
    if ( ! loadGrp( name ) )
        return 0;
    return grp.entry.created;
}

char
Grp_postAllow( const char *name )
{
    if ( ! loadGrp( name ) )
        return 0;
    return grp.postAllow;
}


/* Replace group's description (only if value != ""). */
void
Grp_setDsc( const char *name, const char *value )
{
    if ( loadGrp( name ) )
    {
        Utl_cpyStr( grp.dsc, value );
        saveGrp();
    }
}

void
Grp_setLocal( const char *name )
{
    Grp_setServ( name, GRP_LOCAL_SERVER_NAME );
}

void
Grp_setServ( const char *name, const char *value )
{
    if ( loadGrp( name ) )
    {
        Utl_cpyStr( grp.serv, value );
        saveGrp();
    }
}

void
Grp_setRmtNext( const char *name, int value )
{
    if ( loadGrp( name ) )
    {
        grp.entry.rmtNext = value;
        saveGrp();
    }
}

void
Grp_setLastAccess( const char *name, int value )
{
    if ( loadGrp( name ) )
    {
        grp.entry.lastAccess = value;
        saveGrp();
    }
}

void
Grp_setPostAllow( const char *name, char postAllow )
{
    if ( loadGrp( name ) )
    {
        grp.postAllow = postAllow;
        saveGrp();
    }
}

void
Grp_setFirstLast( const char *name, int first, int last )
{
    if ( loadGrp( name ) )
    {
        grp.entry.first = first;
        grp.entry.last = last;
        saveGrp();
    }
}

static datum cursor = { NULL, 0 };

Bool
Grp_firstGrp( const char **name )
{
    ASSERT( grp.dbf );
    if ( cursor.dptr != NULL )
    {
        free( cursor.dptr );
        cursor.dptr = NULL;
    }
    cursor = gdbm_firstkey( grp.dbf );
    *name = cursor.dptr;
    return ( cursor.dptr != NULL );
}

Bool
Grp_nextGrp( const char **name )
{
    void *oldDptr = cursor.dptr;

    ASSERT( grp.dbf );
    if ( cursor.dptr == NULL )
        return FALSE;
    cursor = gdbm_nextkey( grp.dbf, cursor );
    free( oldDptr );
    *name = cursor.dptr;
    return ( cursor.dptr != NULL );
}
