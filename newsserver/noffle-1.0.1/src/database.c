/*
  database.c

  $Id: database.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $

  Uses GNU gdbm library. Using Berkeley db (included in libc6) was
  cumbersome. It is based on Berkeley db 1.85, which has severe bugs
  (e.g. it is not recommended to delete or overwrite entries with
  overflow pages).
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "database.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <gdbm.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "configfile.h"
#include "itemlist.h"
#include "log.h"
#include "protocol.h"
#include "util.h"
#include "wildmat.h"
#include "portable.h"

static struct Db
{
    GDBM_FILE dbf;

    /* Start string for Xref header line: "Xref: <host>" */
    Str xrefHost;

    /* Msg Id of presently loaded article, empty if none loaded */
    Str msgId;

    /* Status of loaded article */
    int status; /* Flags */
    time_t lastAccess;

    /* Overview of loaded article */
    Str subj; 
    Str from;
    Str date;
    Str ref;
    Str xref;
    size_t bytes;
    size_t lines;

    /* Article text (except for overview header lines) */
    DynStr *txt;

} db = { NULL, "(unknown)", "", 0, 0, "", "", "", "", "", 0, 0, NULL };

static const char *
errMsg( void )
{
    if ( errno != 0 )
        return strerror( errno );
    return gdbm_strerror( gdbm_errno );
}

Bool
Db_open( void )
{
    Str name, host;
    int flags;

    ASSERT( db.dbf == NULL );
    snprintf( name, MAXCHAR, "%s/data/articles.gdbm", Cfg_spoolDir() );
    flags = GDBM_WRCREAT | GDBM_FAST;

    if ( ! ( db.dbf = gdbm_open( name, 512, flags, 0644, NULL ) ) )
    {
        Log_err( "Error opening %s for r/w (%s)", name, errMsg() );
        return FALSE;
    }
    Log_dbg( "%s opened for r/w", name );

    if ( db.txt == NULL )
        db.txt = new_DynStr( 5000 );

    gethostname( host, MAXCHAR );
    snprintf( db.xrefHost, MAXCHAR, "Xref: %s", host );

    return TRUE;
}

void
Db_close( void )
{
    ASSERT( db.dbf );
    Log_dbg( "Closing database" );
    gdbm_close( db.dbf );
    db.dbf = NULL;
    del_DynStr( db.txt );
    db.txt = NULL;
    Utl_cpyStr( db.msgId, "" );
}

static Bool
loadArt( const char *msgId )
{
    static void *dptr = NULL;
    
    datum key, val;
    Str t = "";
    const char *p;
    
    ASSERT( db.dbf );

    if ( strcmp( msgId, db.msgId ) == 0 )
        return TRUE;

    key.dptr = (void *)msgId;
    key.dsize = strlen( msgId ) + 1;
    if ( dptr != NULL )
    {
        free( dptr );
        dptr = NULL;
    }
    val = gdbm_fetch( db.dbf, key );
    dptr = val.dptr;
    if ( dptr == NULL )
    {
        Log_dbg( "database.c loadArt: gdbm_fetch found no entry" );
        return FALSE;
    }
    
    Utl_cpyStr( db.msgId, msgId );
    p = Utl_getLn( t, (char *)dptr );
    if ( ! p || sscanf( t, "%x", &db.status ) != 1 )
    {
        Log_err( "Entry in database '%s' is corrupt (status)", msgId );
        return FALSE;
    }
    p = Utl_getLn( t, p );
    if ( ! p || sscanf( t, "%lu", &db.lastAccess ) != 1 )
    {
        Log_err( "Entry in database '%s' is corrupt (lastAccess)", msgId );
        return FALSE;
    }
    p = Utl_getLn( db.subj, p );
    p = Utl_getLn( db.from, p );
    p = Utl_getLn( db.date, p );
    p = Utl_getLn( db.ref, p );
    p = Utl_getLn( db.xref, p );
    if ( ! p )
    {
        Log_err( "Entry in database '%s' is corrupt (overview)", msgId );
        return FALSE;
    }
    p = Utl_getLn( t, p );
    if ( ! p || sscanf( t, "%u", &db.bytes ) != 1 )
    {
        Log_err( "Entry in database '%s' is corrupt (bytes)", msgId );
        return FALSE;
    }
    p = Utl_getLn( t, p );
    if ( ! p || sscanf( t, "%u", &db.lines ) != 1 )
    {
        Log_err( "Entry in database '%s' is corrupt (lines)", msgId );
        return FALSE;
    }
    DynStr_clear( db.txt );
    DynStr_app( db.txt, p );
    return TRUE;
}

static Bool
saveArt( void )
{
    DynStr *s;
    Str t = "";
    datum key, val;

    if ( strcmp( db.msgId, "" ) == 0 )
        return FALSE;
    s = new_DynStr( 5000 );
    snprintf( t, MAXCHAR, "%x", db.status );
    DynStr_appLn( s, t );
    snprintf( t, MAXCHAR, "%lu", db.lastAccess );
    DynStr_appLn( s, t );
    DynStr_appLn( s, db.subj );
    DynStr_appLn( s, db.from );
    DynStr_appLn( s, db.date );
    DynStr_appLn( s, db.ref );
    DynStr_appLn( s, db.xref );
    snprintf( t, MAXCHAR, "%u", db.bytes );
    DynStr_appLn( s, t );
    snprintf( t, MAXCHAR, "%u", db.lines );
    DynStr_appLn( s, t );
    DynStr_appDynStr( s, db.txt );

    key.dptr = (void *)db.msgId;
    key.dsize = strlen( db.msgId ) + 1;
    val.dptr = (void *)DynStr_str( s );
    val.dsize = DynStr_len( s ) + 1;
    if ( gdbm_store( db.dbf, key, val, GDBM_REPLACE ) != 0 )
    {
        Log_err( "Could not store %s in database (%s)", errMsg() );
        return FALSE;
    }

    del_DynStr( s );
    return TRUE;
}

Bool
Db_prepareEntry( const Over *ov, const char *grp, int numb )
{
    const char *msgId;

    ASSERT( db.dbf );
    ASSERT( ov );
    ASSERT( grp );

    msgId = Ov_msgId( ov );
    Log_dbg( "Preparing entry %s", msgId );
    if ( Db_contains( msgId ) )
        Log_err( "Preparing article twice: %s", msgId );

    db.status = DB_NOT_DOWNLOADED;
    db.lastAccess = time( NULL );

    Utl_cpyStr( db.msgId, msgId );
    Utl_cpyStr( db.subj, Ov_subj( ov ) );
    Utl_cpyStr( db.from, Ov_from( ov ) );
    Utl_cpyStr( db.date, Ov_date( ov ) );
    Utl_cpyStr( db.ref, Ov_ref( ov ) );
    snprintf( db.xref, MAXCHAR, "%s:%i", grp, numb );
    db.bytes = Ov_bytes( ov );
    db.lines = Ov_lines( ov );

    DynStr_clear( db.txt );

    return saveArt();
}

Bool
Db_storeArt( const char *msgId, const char *artTxt )
{
    Str line, lineEx, field, value;

    ASSERT( db.dbf );

    Log_dbg( "Store article %s", msgId );
    if ( ! loadArt( msgId ) )
    {
        Log_err( "Cannot find info about '%s' in database", msgId );
        return FALSE;
    }
    if ( ! ( db.status & DB_NOT_DOWNLOADED ) )
    {
        Log_err( "Trying to store already retrieved article '%s'", msgId );
        return FALSE;
    }
    db.status &= ~DB_NOT_DOWNLOADED;
    db.status &= ~DB_RETRIEVING_FAILED;
    db.lastAccess = time( NULL );

    DynStr_clear( db.txt );

    /* Read header */
    while ( ( artTxt = Utl_getHeaderLn( lineEx, artTxt ) ) != NULL )
    {
        if ( lineEx[ 0 ] == '\0' )
        {
            DynStr_appLn( db.txt, lineEx );
            break;
        }
        /* Remove fields already in overview and handle x-noffle
           headers correctly in case of cascading NOFFLEs */
        if ( Prt_getField( field, value, lineEx ) )
        {
            if ( strcmp( field, "x-noffle-status" ) == 0 )
            {
                if ( strstr( value, "NOT_DOWNLOADED" ) != 0 )
                    db.status |= DB_NOT_DOWNLOADED;
            }
            else if ( strcmp( field, "message-id" ) != 0
                      && strcmp( field, "xref" ) != 0
                      && strcmp( field, "references" ) != 0
                      && strcmp( field, "subject" ) != 0
                      && strcmp( field, "from" ) != 0
                      && strcmp( field, "date" ) != 0
                      && strcmp( field, "bytes" ) != 0
                      && strcmp( field, "lines" ) != 0
                      && strcmp( field, "x-noffle-lastaccess" ) != 0 )
                DynStr_appLn( db.txt, lineEx );
        }
    }

    if ( artTxt == NULL )
    {
	Log_err( "Article %s malformed: missing body", msgId );
	return FALSE;
    }

    /* Read body */
    while ( ( artTxt = Utl_getLn( line, artTxt ) ) != NULL )
        if ( ! ( db.status & DB_NOT_DOWNLOADED ) )
            DynStr_appLn( db.txt, line );
    
    return saveArt();
}

void
Db_setStatus( const char *msgId, int status )
{
    if ( loadArt( msgId ) )
    {
        db.status = status;
        saveArt();
    }
}

void
Db_updateLastAccess( const char *msgId )
{
    if ( loadArt( msgId ) )
    {
        db.lastAccess = time( NULL );
        saveArt();
    }
}

void
Db_setXref( const char *msgId, const char *xref )
{
    if ( loadArt( msgId ) )
    {
        Utl_cpyStr( db.xref, xref );
        saveArt();
    }
}

/* Search best position for breaking a line */
static const char *
searchBreakPos( const char *line, int wantedLength )
{
    const char *lastSpace = NULL;
    Bool lastWasSpace = FALSE;
    int len = 0;

    while ( *line != '\0' )
    {
        if ( isspace( *line ) )
        {
            if ( len > wantedLength && lastSpace != NULL )
                return lastSpace;
            if ( ! lastWasSpace )
                lastSpace = line;
            lastWasSpace = TRUE;
        }
        else
            lastWasSpace = FALSE;
        ++len;
        ++line;
    }
    if ( len > wantedLength && lastSpace != NULL )
        return lastSpace;
    return line;
}

/* Append header line by breaking long line into multiple lines */
static void
appendLongHeader( DynStr *target, const char *field, const char *value )
{
    const int wantedLength = 78;
    const char *breakPos, *old;
    int len;

    len = strlen( field );
    DynStr_appN( target, field, len );
    DynStr_appN( target, " ", 1 );
    old = value;
    while ( isspace( *old ) )
        ++old;
    breakPos = searchBreakPos( old, wantedLength - len - 1 );
    DynStr_appN( target, old, breakPos - old );
    if ( *breakPos == '\0' )
    {
        DynStr_appN( target, "\n", 1 );
        return;
    }
    DynStr_appN( target, "\n ", 2 );
    while ( TRUE )
    {
        old = breakPos;
        while ( isspace( *old ) )
            ++old;
        breakPos = searchBreakPos( old, wantedLength - 1 );
        DynStr_appN( target, old, breakPos - old );
        if ( *breakPos == '\0' )
        {
            DynStr_appN( target, "\n", 1 );
            return;
        }
        DynStr_appN( target, "\n ", 2 );
    }
}

const char *
Db_header( const char *msgId )
{
    static DynStr *s = NULL;

    Str date, t;
    int status;
    const char *p;

    if ( s == NULL )
        s = new_DynStr( 5000 );
    else
        DynStr_clear( s );
    ASSERT( db.dbf );
    if ( ! loadArt( msgId ) )
        return NULL;
    strftime( date, MAXCHAR, "%Y-%m-%d %H:%M:%S",
              localtime( &db.lastAccess ) );
    status = db.status;
    snprintf( t, MAXCHAR,
              "Message-ID: %s\n"
              "X-NOFFLE-Status:%s%s%s\n"
              "X-NOFFLE-LastAccess: %s\n",
              msgId,
              status & DB_INTERESTING ? " INTERESTING" : "",
              status & DB_NOT_DOWNLOADED ? " NOT_DOWNLOADED" : "",
              status & DB_RETRIEVING_FAILED ? " RETRIEVING_FAILED" : "",
              date );
    DynStr_app( s, t );
    appendLongHeader( s, "Subject:", db.subj );
    appendLongHeader( s, "From:", db.from );
    appendLongHeader( s, "Date:", db.date );
    appendLongHeader( s, "References:", db.ref );
    DynStr_app( s, "Bytes: " );
    snprintf( t, MAXCHAR, "%u", db.bytes );
    DynStr_appLn( s, t );
    DynStr_app( s, "Lines: " );
    snprintf( t, MAXCHAR, "%u", db.lines );
    DynStr_appLn( s, t );
    appendLongHeader( s, db.xrefHost, db.xref );
    p = strstr( DynStr_str( db.txt ), "\n\n" );
    if ( ! p )
        DynStr_appDynStr( s, db.txt );
    else
        DynStr_appN( s, DynStr_str( db.txt ), p - DynStr_str( db.txt ) + 1 );
    return DynStr_str( s );
}

const char *
Db_body( const char *msgId )
{
    const char *p;

    if ( ! loadArt( msgId ) )
        return "";
    p = strstr( DynStr_str( db.txt ), "\n\n" );
    if ( ! p )
        return "";
    return ( p + 2 );
}

int
Db_status( const char *msgId )
{
    if ( ! loadArt( msgId ) )
        return 0;
    return db.status;
}

time_t
Db_lastAccess( const char *msgId )
{
    if ( ! loadArt( msgId ) )
        return -1;
    return db.lastAccess;
}

const char *
Db_ref( const char *msgId )
{
    if ( ! loadArt( msgId ) )
        return "";
    return db.ref;
}

const char *
Db_xref( const char *msgId )
{
    if ( ! loadArt( msgId ) )
        return "";
    return db.xref;
}

const char *
Db_from( const char *msgId )
{
    if ( ! loadArt( msgId ) )
        return "";
    return db.from;
}

const char *
Db_date( const char *msgId )
{
    if ( ! loadArt( msgId ) )
        return "";
    return db.date;
}

Over *
Db_over( const char *msgId )
{
    if ( ! loadArt( msgId ) )
	return NULL;
    return new_Over( db.subj, db.from, db.date, msgId,
		     db.ref, db.bytes, db.lines );
}

Bool
Db_contains( const char *msgId )
{
    datum key;

    ASSERT( db.dbf );
    if ( strcmp( msgId, db.msgId ) == 0 )
        return TRUE;
    key.dptr = (void*)msgId;
    key.dsize = strlen( msgId ) + 1;
    return gdbm_exists( db.dbf, key );
}

void
Db_delete( const char *msgId )
{
    datum key;

    ASSERT( db.dbf );
    if ( strcmp( msgId, db.msgId ) == 0 )
        db.msgId[ 0 ] = '\0';
    key.dptr = (void*)msgId;
    key.dsize = strlen( msgId ) + 1;
    gdbm_delete( db.dbf, key );
}

static datum cursor = { NULL, 0 };

Bool
Db_first( const char** msgId )
{
    ASSERT( db.dbf );
    if ( cursor.dptr != NULL )
    {
        free( cursor.dptr );
        cursor.dptr = NULL;
    }
    cursor = gdbm_firstkey( db.dbf );
    *msgId = cursor.dptr;
    return ( cursor.dptr != NULL );
}

Bool
Db_next( const char** msgId )
{
    void *oldDptr = cursor.dptr;

    ASSERT( db.dbf );
    if ( cursor.dptr == NULL )
        return FALSE;
    cursor = gdbm_nextkey( db.dbf, cursor );
    free( oldDptr );
    *msgId = cursor.dptr;
    return ( cursor.dptr != NULL );
}

static int
calcExpireDays( const char *msgId )
{
    const char *xref;
    ItemList *refs;
    const char *ref;
    int res;

    xref = Db_xref( msgId );
    if ( xref[ 0 ] == '\0' )
	return -1;

    res = -1;
    refs = new_Itl( xref, " :" );
    for ( ref = Itl_first( refs ); ref != NULL; ref = Itl_next( refs ) )
    {
	Str pattern;
	int days;
	
	Cfg_beginExpireEnum();
	while ( ( days = Cfg_nextExpire( pattern ) ) != -1 )
	    if ( Wld_match( ref, pattern )
		 && ( ( days > res && res != 0 ) ||
		      days == 0 ) )
	    {
		res = days;
		Log_dbg ( "Custom expiry %d for %s in group %s",
			  days, msgId, ref );
		break;
	    }
	
	Itl_next( refs );	/* Throw away group number */
    }

    if ( res == -1 )
	res = Cfg_expire();
    return res;
}

Bool
Db_expire( void )
{
    int cntDel, cntLeft, flags, expDays;
    time_t nowTime, lastAccess;
    const char *msgId;
    Str name, tmpName;
    GDBM_FILE tmpDbf;
    datum key, val;

    if ( ! Db_open() )
        return FALSE;
    snprintf( name, MAXCHAR, "%s/data/articles.gdbm", Cfg_spoolDir() );
    snprintf( tmpName, MAXCHAR, "%s/data/articles.gdbm.new", Cfg_spoolDir() );
    flags = GDBM_NEWDB | GDBM_FAST;
    if ( ! ( tmpDbf = gdbm_open( tmpName, 512, flags, 0644, NULL ) ) )
    {
        Log_err( "Error opening %s for read/write (%s)", tmpName, errMsg() );
        Db_close();
        return FALSE;
    }
    Log_inf( "Expiring articles" );
    cntDel = 0;
    cntLeft = 0;
    nowTime = time( NULL );
    if ( Db_first( &msgId ) )
        do
        {
	    expDays = calcExpireDays( msgId );
            lastAccess = Db_lastAccess( msgId );
	    if ( expDays == -1 )
		Log_err( "Internal error: Failed expiry calculation on %s",
			 msgId );
	    else if ( lastAccess == -1 )
                Log_err( "Internal error: Getting lastAccess of %s failed",
                         msgId );
            else if ( expDays > 0
		      && difftime( nowTime, lastAccess ) >
		          ( (double) expDays * 24 * 3600 ) )
            {
#ifdef DEBUG
		Str last, now;

		Utl_cpyStr( last, ctime( &lastAccess ) );
		last[ strlen( last ) - 1 ] = '\0';
		Utl_cpyStr( now, ctime( &nowTime ) );
		last[ strlen( now ) - 1 ] = '\0';
                Log_dbg( "Expiring %s: last access %s, time now %s",
			 msgId, last, now );
#endif
                ++cntDel;
            }
            else
            {
                ++cntLeft;
                key.dptr = (void *)msgId;
                key.dsize = strlen( msgId ) + 1;

                val = gdbm_fetch( db.dbf, key );
                if ( val.dptr != NULL )
                {
                    if ( gdbm_store( tmpDbf, key, val, GDBM_INSERT ) != 0 )
                        Log_err( "Could not store %s in new database (%s)",
                                 errMsg() );
                    free( val.dptr );
                }
            }
        }
        while ( Db_next( &msgId ) );
    Log_inf( "%lu articles deleted, %lu left", cntDel, cntLeft );
    gdbm_close( tmpDbf );
    Db_close();
    rename( tmpName, name );
    return TRUE;
}
