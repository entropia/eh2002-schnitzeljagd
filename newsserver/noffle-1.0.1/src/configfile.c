/*
  configfile.c

  The following macros must be set, when compiling this file:
    CONFIGFILE
    SPOOLDIR
    VERSION

  $Id: configfile.c,v 1.1 2002-03-28 17:41:24 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "configfile.h"

#include <limits.h>
#include "log.h"
#include "util.h"
#include "portable.h"

typedef struct
{
    Str name;
    Str user;
    Str pass;
}
ServEntry;

typedef struct
{
    Str pattern;
    int days;
}
ExpireEntry;

struct
{
    /* Compile time options */
    const char *spoolDir;
    const char *version;
    /* Options from the config file */
    int maxFetch;
    int autoUnsubscribeDays;
    int threadFollowTime;
    int connectTimeout;
    Bool autoSubscribe;
    Bool autoUnsubscribe;
    Bool infoAlways;
    Bool removeMsgId;
    Bool replaceMsgId;
    Str autoSubscribeMode;
    Str mailTo;
    int defaultExpire;
    int numServ;
    int maxServ;
    ServEntry *serv;
    int servIdx; /* for server enumeration */
    int numExpire;
    int maxExpire;
    ExpireEntry *expire;
    int expireIdx;
} config =
{
    SPOOLDIR, /* spoolDir */
    VERSION,  /* version */
    300,      /* maxFetch */
    30,       /* autoUnsubscribeDays */
    7,        /* threadFollowTime */
    30,       /* connectTimeout */
    FALSE,    /* autoSubscribe */
    FALSE,    /* autoUnsubscribe */
    TRUE,     /* infoAlways */
    FALSE,    /* removeMsgId */
    TRUE,     /* replaceMsgId */
    "over",   /* autoSubscribeMode */
    "",       /* mailTo */
    14,       /* defaultExpire */
    0,        /* numServ */
    0,        /* maxServ */
    NULL,     /* serv */
    0,	      /* servIdx */
    0,        /* numExpire */
    0,        /* maxExpire */
    NULL,     /* expire */
    0         /* expireIdx */
};

const char * Cfg_spoolDir( void ) { return config.spoolDir; }
const char * Cfg_version( void ) { return config.version; }

int Cfg_maxFetch( void ) { return config.maxFetch; }
int Cfg_autoUnsubscribeDays( void ) { return config.autoUnsubscribeDays; }
int Cfg_threadFollowTime( void ) { return config.threadFollowTime; }
int Cfg_connectTimeout( void ) { return config.connectTimeout; }
Bool Cfg_autoUnsubscribe( void ) { return config.autoUnsubscribe; }
Bool Cfg_autoSubscribe( void )  { return config.autoSubscribe; }
Bool Cfg_infoAlways( void )  { return config.infoAlways; }
Bool Cfg_removeMsgId( void ) { return config.removeMsgId; }
Bool Cfg_replaceMsgId( void ) { return config.replaceMsgId; }
const char * Cfg_autoSubscribeMode( void ) {
    return config.autoSubscribeMode; }
const char * Cfg_mailTo( void ) { return config.mailTo; }
int Cfg_expire( void ) { return config.defaultExpire; }

void
Cfg_beginServEnum( void )
{
    config.servIdx = 0;
}

Bool
Cfg_nextServ( Str name )
{
    if ( config.servIdx >= config.numServ )
        return FALSE;
    strcpy( name, config.serv[ config.servIdx ].name );
    ++config.servIdx;
    return TRUE;
}

static Bool
searchServ( const char *name, int *idx )
{
    int i;

    for ( i = 0; i < config.numServ; ++i )
        if ( strcmp( name, config.serv[ i ].name ) == 0 )
        {
            *idx = i;
            return TRUE;
        }
    return FALSE;
}

Bool
Cfg_servListContains( const char *name )
{
    int idx;

    return searchServ( name, &idx );
}

Bool
Cfg_servIsPreferential( const char *name1, const char *name2 )
{
    Bool exists1, exists2;
    int idx1, idx2;

    exists1 = searchServ( name1, &idx1 );
    exists2 = searchServ( name2, &idx2 );
    if ( exists1 && exists2 )
        return ( idx1 < idx2 );
    if ( exists1 && ! exists2 )
        return TRUE;
    /* ( ! exists1 && exists2 ) || ( ! exists1 && ! exists2 ) */
    return FALSE;
}

void
Cfg_authInfo( const char *name, Str user, Str pass )
{
    int idx;

    if ( searchServ( name, &idx ) )
    {
        strcpy( user, config.serv[ idx ].user );
        strcpy( pass, config.serv[ idx ].pass );
    }
    else
    {
        user[ 0 ] = '\0';
        pass[ 0 ] = '\0';
    }
}

void
Cfg_beginExpireEnum( void )
{
    config.expireIdx = 0;
}

int
Cfg_nextExpire( Str pattern )
{
    if ( config.expireIdx >= config.numExpire )
        return -1;
    strcpy( pattern, config.expire[ config.expireIdx ].pattern );
    return config.expire[ config.expireIdx++ ].days;
}

static void
logSyntaxErr( const char *line )
{
    Log_err( "Syntax error in config file: %s", line );
}

static void
getBool( Bool *variable, const char *line )
{
    Str value, name, lowerLn;

    strcpy( lowerLn, line );
    Utl_toLower( lowerLn );
    if ( sscanf( lowerLn, "%s %s", name, value ) != 2 )
    {
        logSyntaxErr( line );
        return;
    }
    
    if ( strcmp( value, "yes" ) == 0 )
        *variable = TRUE;
    else if ( strcmp( value, "no" ) == 0 )
        *variable = FALSE;
    else
        Log_err( "Error in config file %s must be yes or no", name );
}

static void
getInt( int *variable, int min, int max, const char *line )
{
    int value;
    Str name;

    if ( sscanf( line, "%s %d", name, &value ) != 2 )
    {
        logSyntaxErr( line );
        return;
    }
    if ( value < min || value > max )
    {
        Log_err( "Range error in config file %s [%d,%d]", name, min, max );
        return;
    }
    *variable = value;
}

static void
getStr( char *variable, const char *line )
{
    Str dummy;

    if ( sscanf( line, "%s %s", dummy, variable ) != 2 )
    {
        logSyntaxErr( line );
        return;
    }
}

static void
getServ( const char *line )
{
    Str dummy;
    int r, len;
    ServEntry entry;

    entry.user[ 0 ] = '\0';
    entry.pass[ 0 ] = '\0';
    r = sscanf( line, "%s %s %s %s",
                dummy, entry.name, entry.user, entry.pass );
    if ( r < 2 )
    {
        logSyntaxErr( line );
        return;
    }
    len = strlen( entry.name );
    /* To make server name more definit, it is made lowercase and
       port is removed, if it is the default port */
    if ( len > 4 && strcmp( entry.name + len - 4, ":119" ) == 0 )
        entry.name[ len - 4 ] = '\0';
    Utl_toLower( entry.name );

    if ( config.maxServ < config.numServ + 1 )
    {
        if ( ! ( config.serv = realloc( config.serv,
                                        ( config.maxServ + 5 )
                                        * sizeof( ServEntry ) ) ) )
        {
            Log_err( "Could not realloc server list" );
            exit( EXIT_FAILURE );
        }
        config.maxServ += 5;
    }
    config.serv[ config.numServ++ ] = entry;
}

static void
getExpire( const char *line )
{
    Str dummy;
    ExpireEntry entry;
    int days;

    /*
      The line is either "expire <num>" or "expire <pat> <num>".
      The former updates the overall default.
     */
    if ( sscanf( line, "%s %s %d", dummy, entry.pattern, &days ) != 3 )
    {
	logSyntaxErr( line );
	return;
    }
    else
    {
	if ( days < 0 )
	{
	    Log_err( "Expire days error in '%s': must be integer > 0",
		     line, days );
	    return;
	}

	Utl_toLower( entry.pattern );
	entry.days = days;

	if ( config.maxExpire < config.numExpire + 1 )
	{
	    if ( ! ( config.expire = realloc( config.expire,
					      ( config.maxExpire + 5 )
					      * sizeof( ExpireEntry ) ) ) )
	    {
		Log_err( "Could not realloc exipre list" );
		exit( EXIT_FAILURE );
	    }
	    config.maxExpire += 5;
	}
	config.expire[ config.numExpire++ ] = entry;
    }
}

void
Cfg_read( void )
{
    char *p;
    FILE *f;
    Str file, line, lowerLine, name, s;

    snprintf( file, MAXCHAR, CONFIGFILE );
    if ( ! ( f = fopen( file, "r" ) ) )
    {
        Log_err( "Cannot read %s", file );
        return;
    }
    while ( fgets( line, MAXCHAR, f ) )
    {
        p = Utl_stripWhiteSpace( line );
	Utl_stripComment( p );
        Utl_cpyStr( lowerLine, p );
        Utl_toLower( lowerLine );
        if ( *p == '\0' )
            continue;
        if ( sscanf( p, "%s", name ) != 1 )
            Log_err( "Syntax error in %s: %s", file, line );
        else if ( strcmp( "max-fetch", name ) == 0 )
            getInt( &config.maxFetch, 0, INT_MAX, p );
        else if ( strcmp( "auto-unsubscribe-days", name ) == 0 )
            getInt( &config.autoUnsubscribe, -1, INT_MAX, p );
        else if ( strcmp( "thread-follow-time", name ) == 0 )
            getInt( &config.threadFollowTime, 0, INT_MAX, p );
        else if ( strcmp( "connect-timeout", name ) == 0 )
            getInt( &config.connectTimeout, 0, INT_MAX, p );
        else if ( strcmp( "default-expire", name ) == 0 )
            getInt( &config.defaultExpire, 0, INT_MAX, p );
        else if ( strcmp( "auto-subscribe", name ) == 0 )
            getBool( &config.autoSubscribe, p );
        else if ( strcmp( "auto-unsubscribe", name ) == 0 )
            getBool( &config.autoUnsubscribe, p );
        else if ( strcmp( "info-always-unread", name ) == 0 )
            getBool( &config.infoAlways, p );
        else if ( strcmp( "remove-messageid", name ) == 0 )
            getBool( &config.removeMsgId, p );
        else if ( strcmp( "replace-messageid", name ) == 0 )
            getBool( &config.replaceMsgId, p );
        else if ( strcmp( "auto-subscribe-mode", name ) == 0 )
        {
            getStr( s, p );
            Utl_toLower( s );
            if ( strcmp( s, "full" ) != 0
                 && strcmp( s, "thread" ) != 0
                 && strcmp( s, "over" ) != 0
                 && strcmp( s, "off" ) != 0 )
            {
                Log_err( "Syntax error in config file: %s", line );
                return;
            }
            else
                strcpy( config.autoSubscribeMode, s );
        }
        else if ( strcmp( "server", name ) == 0 )
            /* Server needs line not p,
               because password may contain uppercase */
            getServ( line );
        else if ( strcmp( "mail-to", name ) == 0 )
            getStr( config.mailTo, p );
        else if ( strcmp( "expire", name ) == 0 )
            getExpire( p );
        else
            Log_err( "Unknown config option: %s", name );
    }
    fclose( f );
    if ( ! config.numServ )
    {
        Log_err( "Config file contains no server" );
        exit( EXIT_FAILURE );
    }
}
