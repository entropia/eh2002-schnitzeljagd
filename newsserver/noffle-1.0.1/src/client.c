/*
  client.c

  $Id: client.c,v 1.1 2002-03-28 17:41:24 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "client.h"

#include <stdio.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <unistd.h>
#include "configfile.h"
#include "content.h"
#include "dynamicstring.h"
#include "group.h"
#include "itemlist.h"
#include "log.h"
#include "over.h"
#include "protocol.h"
#include "pseudo.h"
#include "request.h"
#include "util.h"
#include "wildmat.h"
#include "portable.h"

/*
  Some newsgroups names are reserved for server-specific or server
  pseudo groups. We don't want to fetch them. For example, INN
  keeps all its control messages in a 'control' hierarchy, and
  used the "to." hierarchy for dark and mysterious purposes I think
  are to do with newsfeeds. The recommended restrictions are documented
  in C.Lindsay, "News Article Format", <draft-ietf-usefor-article-03.txt>.
*/

struct ForbiddenGroupName
{
    const char *pattern;
    Bool match;
} forbiddenGroupNames[] =
{
    { "*.*", FALSE },			/* Single component */
    { "control.*", TRUE },		/* control.* groups */
    { "to.*", TRUE },			/* to.* groups */
    { "*.all", TRUE },			/* 'all' as a component */
    { "*.all.*", TRUE },
    { "all.*", TRUE },
    { "*.ctl", TRUE },			/* 'ctl' as a component */
    { "*.ctl.*", TRUE },
    { "ctl.*", TRUE }
};

struct
{
    FILE* in;       /* Receiving socket from server */
    FILE* out;      /* Sending socket to server */
    Str lastCmd;    /* Last command line */
    Str lastStat;   /* Response from server to last command */
    Str grp;        /* Selected group */
    int rmtFirst;   /* First article of current group at server */
    int rmtLast;    /* Last article of current group at server */
    Bool auth;      /* Authentication already done? */
    Bool connected; /* Connection alive? */
    Str serv;       /* Remote server name */
} client = { NULL, NULL, "", "", "", 1, 0, FALSE, FALSE, "" };

static void
breakDown( void )
{
    Log_err( "Connection to remote server lost "
             "(article numbers could be inconsistent)" );
    client.connected = FALSE;
}

static Bool
getLn( Str line )
{
    Bool r;

    if ( ! client.connected )
        return FALSE;
    r = Prt_getLn( line, client.in, Cfg_connectTimeout() );
    if ( ! r )
        breakDown();
    return r; 
}

static Bool
getTxtLn( Str line, Bool *err )
{
    Bool r;

    if ( ! client.connected )
        return FALSE;
    r = Prt_getTxtLn( line, err, client.in, Cfg_connectTimeout() );
    if ( *err )
        breakDown();
    return r; 
}

static void
putTxtBuf( const char *buf )
{
    Prt_putTxtBuf( buf, client.out );
    fflush( client.out );
    Log_dbg( "[S FLUSH]" );
}

static void
putEndOfTxt( void )
{
    Prt_putEndOfTxt( client.out );
    fflush( client.out );
    Log_dbg( "[S FLUSH]" );
}

static Bool
putCmdLn( const char *line )
{
    Bool err;
    unsigned int n;

    if ( ! client.connected )
        return FALSE;
    strcpy( client.lastCmd, line );
    strcpy( client.lastStat, "[no status available]" );
    Log_dbg( "[S] %s", line );
    n = fprintf( client.out, "%s\r\n", line );
    err = ( n != strlen( line ) + 2 );
    if ( err )
        breakDown();
    return ! err;
}

static Bool
putCmd( const char *fmt, ... )
{
    Str line;
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( line, MAXCHAR, fmt, ap );
    va_end( ap );
    if ( ! putCmdLn( line ) )
        return FALSE;
    fflush( client.out );
    Log_dbg( "[S FLUSH]" );
    return TRUE;
}

static Bool
putCmdNoFlush( const char *fmt, ... )
{
    Str line;
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( line, MAXCHAR, fmt, ap );
    va_end( ap );
    return putCmdLn( line );
}

static int getStat( void );

static Bool
performAuth( void )
{
    int stat;
    Str user, pass;
    
    Cfg_authInfo( client.serv, user, pass );
    if ( strcmp( user, "" ) == 0 )
    {
        Log_err( "No username for authentication set" );
        return FALSE;
    }    
    putCmd( "AUTHINFO USER %s", user );
    stat = getStat();
    if ( stat == STAT_AUTH_ACCEPTED )
        return TRUE;
    else if ( stat != STAT_MORE_AUTH_REQUIRED )
    {
        Log_err( "Username rejected. Server stat: %s", client.lastStat );
        return FALSE;
    }    
    if ( strcmp( pass, "" ) == 0 )
    {
        Log_err( "No password for authentication set" );
        return FALSE;
    }
    putCmd( "AUTHINFO PASS %s", pass );
    stat = getStat();
    if ( stat != STAT_AUTH_ACCEPTED )
    {
        Log_err( "Password rejected. Server status: %s", client.lastStat );
        return FALSE;
    }    
    return TRUE;    
}

static int
getStat( void )
{
    int result;
    Str lastCmd;

    if ( ! getLn( client.lastStat ) )
        result = STAT_PROGRAM_FAULT;
    else if ( sscanf( client.lastStat, "%d", &result ) != 1 )
    {
        Log_err( "Invalid server status: %s", client.lastStat );
        result = STAT_PROGRAM_FAULT;
    }
    if ( result == STAT_AUTH_REQUIRED && ! client.auth )
    {
        client.auth = TRUE;
        strcpy( lastCmd, client.lastCmd );
        if ( performAuth() )
        {
            putCmd( lastCmd );
            return getStat();
        }
    }
    return result;
}

static void
connectAlarm( int sig )
{
    UNUSED( sig );
    
    return;
}

static sig_t
installSignalHandler( int sig, sig_t handler )
{
    struct sigaction act, oldAct;

    act.sa_handler = handler;
    sigemptyset( &act.sa_mask );
    act.sa_flags = 0;
    if ( sig == SIGALRM )
        act.sa_flags |= SA_INTERRUPT;
    else
        act.sa_flags |= SA_RESTART;
    if ( sigaction( sig, &act, &oldAct ) < 0 )
        return SIG_ERR;
    return oldAct.sa_handler;
}

static Bool
connectWithTimeout( int sock, const struct sockaddr *servAddr,
                    socklen_t addrLen )
{
    sig_t oldHandler;
    int r, to;

    oldHandler = installSignalHandler( SIGALRM, connectAlarm );
    if ( oldHandler == SIG_ERR )
    {
        Log_err( "client.c:connectWithTimeout: signal failed." );
        return FALSE;
    }
    to = Cfg_connectTimeout();
    if ( alarm( ( unsigned int ) to ) != 0 )
        Log_err( "client.c:connectWithTimeout: Alarm was already set." );
    r = connect( sock, servAddr, addrLen );
    alarm( 0 );
    installSignalHandler( SIGALRM, oldHandler );
    return ( r >= 0 );
}

Bool
Client_connect( const char *serv )
{
    unsigned short int port;
    int sock, i;
    unsigned int stat;
    struct hostent *hp;
    char *pStart, *pColon;
    Str host, s;
    struct sockaddr_in sIn;

    client.connected = FALSE;
    client.auth = FALSE;
    Utl_cpyStr( s, serv );
    pStart = Utl_stripWhiteSpace( s );
    pColon = strstr( pStart, ":" );
    if ( pColon == NULL )
    {
        strcpy( host, pStart );
        port = 119;
    }
    else
    {
        *pColon = '\0';
        strcpy( host, pStart );
        if ( sscanf( pColon + 1, "%hi", &port ) != 1 )
        {
            Log_err( "Syntax error in server name: '%s'", serv );
            return FALSE;;
        }
        if ( port <= 0 || port > 65535 )
        {
            Log_err( "Invalid port number %hi. Must be in [1, 65535]", port );
            return FALSE;;
        }
    }
    memset( (void *)&sIn, 0, sizeof( sIn ) );
    hp = gethostbyname( host );
    if ( hp )
    {
        for ( i = 0; (hp->h_addr_list)[ i ]; ++i )
        {
            sIn.sin_family = hp->h_addrtype;
            sIn.sin_port = htons( port );
            sIn.sin_addr = *( (struct in_addr *)hp->h_addr_list[ i ] );
            sock = socket( AF_INET, SOCK_STREAM, 0 );
            if ( sock < 0 )
                break;
            if ( ! connectWithTimeout( sock, (struct sockaddr *)&sIn,
                                       sizeof( sIn ) ) )
            {
                close( sock );
                break;
            }
            if ( ! ( client.out = fdopen( sock, "w" ) )
                 || ! ( client.in  = fdopen( dup( sock ), "r" ) ) )
            {
		if ( client.out != NULL )
		    fclose( client.out );
                close( sock );
                break;
            }
            Utl_cpyStr( client.serv, serv );
            client.connected = TRUE;
            stat = getStat();
	    if ( stat == STAT_READY_POST_ALLOW ||
		 stat == STAT_READY_NO_POST_ALLOW )
	    {
		/* INN needs a MODE READER before it will permit POST. */
		putCmd( "MODE READER" );
		stat = getStat();
	    }
            switch( stat ) {
            case STAT_READY_POST_ALLOW:
            case STAT_READY_NO_POST_ALLOW: 
                Log_inf( "Connected to %s:%d",
                         inet_ntoa( sIn.sin_addr ), port );
                return TRUE;
            default:
                Log_err( "Bad server stat %d", stat ); 
            }
            shutdown( fileno( client.out ), 0 );
	    fclose( client.in );
	    fclose( client.out );
	    close( sock );
        }
    }
    client.connected = FALSE;
    return FALSE;
}

static Bool
isForbiddenGroupName( const char *name )
{
    size_t i;

    for ( i = 0;
	  i < sizeof( forbiddenGroupNames ) /
	      sizeof( struct ForbiddenGroupName );
	  i++ )
    {
	/* Negate result of Wld_match to ensure it is 1 or 0. */
	if ( forbiddenGroupNames[i].match !=
	     ( ! Wld_match( name, forbiddenGroupNames[i].pattern ) ) )
	    return TRUE;
    }

    return FALSE;
}

static void
processGrps( void )
{
    char postAllow;
    Bool err;
    int first, last;
    Str grp, line, file;
    Bool groupupdate;
    
    groupupdate = FALSE;
    while ( getTxtLn( line, &err ) && ! err )
    {
        if ( sscanf( line, "%s %d %d %c",
                     grp, &last, &first, &postAllow ) != 4 )
        {
            Log_err( "Unknown reply to LIST or NEWGROUPS: %s", line );
            continue;
        }
	if ( isForbiddenGroupName( grp ) )
	{
	    Log_inf( "Group %s forbidden", grp );
	    continue;
	}
        if ( ! Grp_exists( grp ) )
        {
            Log_inf( "Registering new group '%s'", grp );
            Grp_create( grp );
            /* Start local numbering with remote first number to avoid
               new numbering at the readers if noffle is re-installed */
            if ( first != 0 )
                Grp_setFirstLast( grp, first, first - 1 );
            else
                Grp_setFirstLast( grp, 1, 0 );
            Grp_setRmtNext( grp, first );
            Grp_setServ( grp, client.serv );
	    Grp_setPostAllow( grp, postAllow );
	    groupupdate = TRUE;
        }
        else
        {
            if ( Cfg_servIsPreferential( client.serv, Grp_server( grp ) ) )
            {
                Log_inf( "Changing server for '%s': '%s'->'%s'",
                         grp, Grp_server( grp ), client.serv );
                Grp_setServ( grp, client.serv );
                Grp_setRmtNext( grp, first );
		Grp_setPostAllow( grp, postAllow );
		groupupdate = TRUE;
            }
            else
                Log_dbg( "Group %s is already fetched from %s",
                           grp, Grp_server( grp ) );
            
        }
    }
    if ( ! err )
    {
	snprintf( file, MAXCHAR, "%s/lastupdate.%s",
		  Cfg_spoolDir(), client.serv );
	Utl_stamp( file );
	if ( groupupdate )
	{
	    snprintf( file, MAXCHAR, "%s/groupinfo.lastupdate",
		      Cfg_spoolDir() );
	    Utl_stamp( file );
	}
    }
}

void
Client_disconnect( void )
{
    if ( putCmd( "QUIT" ) )
        getStat();
    fclose( client.in );
    fclose( client.out );
    client.in = client.out = NULL;
    client.connected = FALSE;
}

Bool
Client_getGrps( void )
{
    if ( ! client.connected )
        return FALSE;
    if ( ! putCmd( "LIST" ) )
        return FALSE;
    if ( getStat() != STAT_GRPS_FOLLOW )
    {
        Log_err( "LIST command failed: %s", client.lastStat );
        return FALSE;
    }
    processGrps();
    return TRUE;
}

Bool
Client_getDsc( void )
{
    Bool err;
    Str name, line, dsc;

    if ( ! client.connected )
        return FALSE;
    Log_inf( "Querying group descriptions" );
    if ( ! putCmd( "LIST NEWSGROUPS" ) )
        return FALSE;
    if ( getStat() != STAT_GRPS_FOLLOW )
    {
        Log_err( "LIST NEWSGROUPS failed: %s", client.lastStat );
        return FALSE;
    }
    while ( getTxtLn( line, &err ) && ! err )
    {
        if ( sscanf( line, "%s", name ) != 1 )
        {
            Log_err( "Unknown reply to LIST NEWSGROUPS: %s", line );
            continue;
        }
        strcpy( dsc, Utl_restOfLn( line, 1 ) );
        if ( Grp_exists( name ) )
        {
            Log_dbg( "Description of %s: %s", name, dsc );
            Grp_setDsc( name, dsc );
        }
    }
    return TRUE;
}

Bool
Client_getNewgrps( const time_t *lastTime )
{
    Str s;
    const char *p;

    ASSERT( *lastTime > 0 );
    if ( ! client.connected )
        return FALSE;
    strftime( s, MAXCHAR, "%Y%m%d %H%M00", gmtime( lastTime ) );
    /*
      Do not use century for working with old server software until 2000.
      According to newest IETF draft, this is still valid after 2000.
      (directly using %y in fmt string causes a Y2K compiler warning)
    */
    p = s + 2;
    if ( ! putCmd( "NEWGROUPS %s GMT", p ) )
        return FALSE;
    if ( getStat() != STAT_NEW_GRP_FOLLOW )
    {
        Log_err( "NEWGROUPS command failed: %s", client.lastStat );
        return FALSE;
    }
    processGrps();
    return TRUE;
}

static const char *
readField( Str result, const char *p )
{
    size_t len;
    char *r;

    if ( ! p )
        return NULL;
    r = result;
    *r = '\0';
    len = 0;
    while ( *p != '\t' && *p != '\n' )
    {
        if ( ! *p )
            return p;
        *(r++) = *(p++);
        ++len;
        if ( len >= MAXCHAR - 1 )
        {
            *r = '\0';
            Log_err( "Field in overview too long: %s", r );
            return ++p;
        }
    }
    *r = '\0';
    return ++p;
}

static Bool
parseOvLn( Str line, int *numb, Str subj, Str from,
           Str date, Str msgId, Str ref, size_t *bytes, size_t *lines )
{
    const char *p;
    Str t;
    
    p = readField( t, line );
    if ( sscanf( t, "%d", numb ) != 1 )
        return FALSE;
    p = readField( subj, p );
    p = readField( from, p );
    p = readField( date, p );
    p = readField( msgId, p );
    p = readField( ref, p );
    p = readField( t, p );
    *bytes = 0;
    *lines = 0;
    if ( sscanf( t, "%d", bytes ) != 1 )
        return TRUE;
    p = readField( t, p );
    if ( sscanf( t, "%d", lines ) != 1 )
        return TRUE;
    return TRUE;
}

static const char*
nextXref( const char *pXref, Str grp, int *numb )
{
    Str s;
    const char *pColon, *src;
    char *dst;

    src = pXref;
    while ( *src && isspace( *src ) )
        ++src;
    dst = s;
    while ( *src && ! isspace( *src ) )
        *(dst++) = *(src++);
    *dst = '\0';
    if ( strlen( s ) == 0 )
        return NULL;
    pColon = strstr( s, ":" );
    if ( ! pColon || sscanf( pColon + 1, "%d", numb ) != 1 )
    {
        Log_err( "Corrupt Xref at position '%s'", pXref );
        return NULL;
    }
    Utl_cpyStrN( grp, s, pColon - s );
    Log_dbg( "client.c: nextXref: grp '%s' numb %lu", grp, numb );
    return src;
}

static Bool
needsMark( const char *ref )
{
    Bool interesting, result;
    const char *msgId;
    int status;
    time_t lastAccess, nowTime;
    double threadFollowTime, secPerDay, maxTime, timeSinceLastAccess;
    ItemList *itl;

    Log_dbg( "Checking references '%s' for thread mode", ref );
    result = FALSE;
    itl = new_Itl( ref, " \t" );
    nowTime = time( NULL );
    threadFollowTime = (double)Cfg_threadFollowTime();
    secPerDay = 24.0 * 3600.0;
    maxTime = threadFollowTime * secPerDay;
    Log_dbg( "Max time = %.0f", maxTime );
    for ( msgId = Itl_first( itl ); msgId != NULL; msgId = Itl_next( itl ) )
    {
        /*
          References does not have to contain only Message IDs,
          but often it does, so we look up every item in the database.
        */          
        if ( Db_contains( msgId ) )
        {
            status = Db_status( msgId );
            lastAccess = Db_lastAccess( msgId );
            interesting = ( status & DB_INTERESTING );
            timeSinceLastAccess = difftime( nowTime, lastAccess );
            Log_dbg( "Msg ID '%s': since last access = %.0f, interesting = %s",
                     msgId, timeSinceLastAccess, ( interesting ? "y" : "n" ) );
            if ( interesting && timeSinceLastAccess <= maxTime )
            {
                result = TRUE;
                break;
            }
        }
        else
        {
            Log_dbg( "MsgID '%s': not in database.", msgId );
        }
    }
    del_Itl( itl );
    Log_dbg( "Article %s marking for download.",
             ( result ? "needs" : "doesn't need" ) );
    return result;
}

static void
prepareEntry( Over *ov )
{
    Str g, t;
    const char *msgId, *p, *xref;
    int n;

    msgId = Ov_msgId( ov );
    if ( Pseudo_isGeneralInfo( msgId ) )
        Log_dbg( "Skipping general info '%s'", msgId );
    else if ( Db_contains( msgId ) )
    {
        xref = Db_xref( msgId );
        Log_dbg( "Entry '%s' already in db with Xref '%s'", msgId, xref );
        p = nextXref( xref, g, &n );
        if ( p == NULL )
            Log_err( "Overview with no group in Xref '%s'", msgId );
        else
        {
            /* TODO: This code block seems unnessesary. Can we remove it? */
            if ( Cfg_servIsPreferential( client.serv, Grp_server( g ) ) )
            {
                Log_dbg( "Changing first server for '%s' from '%s' to '%s'",
                         msgId, Grp_server( g ), client.serv );
                snprintf( t, MAXCHAR, "%s:%d %s",
                          client.grp, Ov_numb( ov ), xref );
                Db_setXref( msgId, t );
            }
            else
            {
                Log_dbg( "Adding '%s' to Xref of '%s'", g, msgId );
                snprintf( t, MAXCHAR, "%s %s:%d",
                          xref, client.grp, Ov_numb( ov ) );
                Db_setXref( msgId, t );
            }
        }
    }
    else
    {
        Log_dbg( "Preparing '%s' in database", msgId );
        Db_prepareEntry( ov, client.grp, Ov_numb( ov ) );
    }
}

Bool
Client_getOver( int rmtFirst, int rmtLast, FetchMode mode )
{
    Bool err;
    size_t bytes, lines;
    int rmtNumb, oldLast, cntMarked;
    Over *ov;
    Str line, subj, from, date, msgId, ref;

    ASSERT( strcmp( client.grp, "" ) != 0 );
    if ( ! client.connected )
        return FALSE;
    if ( ! putCmd( "XOVER %lu-%lu", rmtFirst, rmtLast ) )
        return FALSE;
    if ( getStat() != STAT_OVERS_FOLLOW )
    {
        Log_err( "XOVER command failed: %s", client.lastStat );
        return FALSE;
    }
    Log_dbg( "Requesting overview for remote %lu-%lu", rmtFirst, rmtLast );
    oldLast = Cont_last();
    cntMarked = 0;
    while ( getTxtLn( line, &err ) && ! err )
    {
        if ( ! parseOvLn( line, &rmtNumb, subj, from, date, msgId, ref,
                          &bytes, &lines ) )
            Log_err( "Bad overview line: %s", line );
        else
        {
            ov = new_Over( subj, from, date, msgId, ref, bytes, lines );
            Cont_app( ov );
            prepareEntry( ov );
            if ( mode == FULL || ( mode == THREAD && needsMark( ref ) ) )
            {
                Req_add( client.serv, msgId );
                ++cntMarked;
            }
        }
        Grp_setRmtNext( client.grp, rmtNumb + 1 );
    }
    if ( oldLast != Cont_last() )
        Log_inf( "Added %s %lu-%lu", client.grp, oldLast + 1, Cont_last() );
    Log_inf( "%u articles marked for download in %s", cntMarked, client.grp  );
    return err;
}

static void
retrievingFailed( const char* msgId, const char *reason )
{
    int status;

    Log_err( "Retrieving of %s failed: %s", msgId, reason );
    status = Db_status( msgId );
    Pseudo_retrievingFailed( msgId, reason );
    Db_setStatus( msgId, status | DB_RETRIEVING_FAILED );
}

static Bool
retrieveAndStoreArt( const char *msgId )
{
    Bool err;
    DynStr *s = NULL;
    Str line;

    Log_inf( "Retrieving %s", msgId );
    s = new_DynStr( 5000 );
    while ( getTxtLn( line, &err ) && ! err )
        DynStr_appLn( s, line );
    if ( ! err )
        Db_storeArt( msgId, DynStr_str( s ) );
    else
        retrievingFailed( msgId, "Connection broke down" );
    del_DynStr( s );
    return ! err;
}

void
Client_retrieveArt( const char *msgId )
{
    if ( ! client.connected )
        return;
    if ( ! Db_contains( msgId ) )
    {
        Log_err( "Article '%s' not prepared in database. Skipping.", msgId );
        return;
    }
    if ( ! ( Db_status( msgId ) & DB_NOT_DOWNLOADED ) )
    {
        Log_inf( "Article '%s' already retrieved. Skipping.", msgId );
        return;
    }
    if ( ! putCmd( "ARTICLE %s", msgId ) )
        retrievingFailed( msgId, "Connection broke down" );
    else if ( getStat() != STAT_ART_FOLLOWS )
        retrievingFailed( msgId, client.lastStat );
    else
        retrieveAndStoreArt( msgId );
}

void
Client_retrieveArtList( const char *list )
{
    Str msgId;
    DynStr *s;
    const char *p;
    
    if ( ! client.connected )
        return;
    Log_inf( "Retrieving article list" );
    s = new_DynStr( (int)strlen( list ) );
    p = list;
    while ( ( p = Utl_getLn( msgId, p ) ) )
        if ( ! Db_contains( msgId ) )
            Log_err( "Skipping retrieving of %s (not prepared in database)",
                     msgId );
        else if ( ! ( Db_status( msgId ) & DB_NOT_DOWNLOADED ) )
            Log_inf( "Skipping %s (already retrieved)", msgId );
        else if ( ! putCmdNoFlush( "ARTICLE %s", msgId ) )
        {
            retrievingFailed( msgId, "Connection broke down" );
            del_DynStr( s );
            return;
        }
        else
            DynStr_appLn( s, msgId );
    fflush( client.out );
    Log_dbg( "[S FLUSH]" );
    p = DynStr_str( s );
    while ( ( p = Utl_getLn( msgId, p ) ) )
    {
        if ( getStat() != STAT_ART_FOLLOWS )
            retrievingFailed( msgId, client.lastStat );
        else if ( ! retrieveAndStoreArt( msgId ) )
            break;
    }
    del_DynStr( s );
}

Bool
Client_changeToGrp( const char* name )
{
    unsigned int stat;
    int estimatedNumb, first, last;

    if ( ! client.connected )
        return FALSE;
    if ( ! Grp_exists( name ) )
        return FALSE;
    if ( ! putCmd( "GROUP %s", name ) )
        return FALSE;
    if ( getStat() != STAT_GRP_SELECTED )
        return FALSE;
    if ( sscanf( client.lastStat, "%u %d %d %d",
                 &stat, &estimatedNumb, &first, &last ) != 4 )
    {
        Log_err( "Bad server response to GROUP: %s", client.lastStat );
        return FALSE;
    }
    Utl_cpyStr( client.grp, name );
    client.rmtFirst = first;
    client.rmtLast = last;
    return TRUE;
}

void
Client_rmtFirstLast( int *first, int *last )
{
    *first = client.rmtFirst;
    *last = client.rmtLast;
}

Bool
Client_postArt( const char *msgId, const char *artTxt,
                    Str errStr )
{
    if ( ! client.connected )
        return FALSE;
    if ( ! putCmd( "POST" ) )
        return FALSE;
    if ( getStat() != STAT_SEND_ART )
    {
        Log_err( "Posting of %s not allowed: %s", msgId, client.lastStat );
        strcpy( errStr, client.lastStat );
        return FALSE;
    }
    putTxtBuf( artTxt );
    putEndOfTxt();
    if ( getStat() != STAT_POST_OK )
    {
        Log_err( "Posting of %s failed: %s", msgId, client.lastStat );
        strcpy( errStr, client.lastStat );
        return FALSE;
    }
    Log_inf( "Posted %s (Status: %s)", msgId, client.lastStat );
    return TRUE;
}
