/*
  server.c

  $Id: server.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

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

#include <stdio.h>
#include "server.h"
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include "client.h"
#include "common.h"
#include "configfile.h"
#include "content.h"
#include "control.h"
#include "database.h"
#include "dynamicstring.h"
#include "fetch.h"
#include "fetchlist.h"
#include "group.h"
#include "itemlist.h"
#include "lock.h"
#include "log.h"
#include "online.h"
#include "outgoing.h"
#include "over.h"
#include "post.h"
#include "protocol.h"
#include "pseudo.h"
#include "request.h"
#include "util.h"
#include "wildmat.h"
#include "portable.h"

struct
{
    Bool running;
    int artPtr;
    Str grp; /* selected group, "" if none */
    Bool readAlarmFlag;
} server = { FALSE, 0, "", FALSE };

typedef struct Cmd
{
    const char *name;
    const char *syntax;
    /* Returns false, if quit cmd */
    Bool (*cmdProc)( char *arg, const struct Cmd *cmd );
}
Cmd;

static Bool doArt( char *arg, const Cmd *cmd );
static Bool doBody( char *arg, const Cmd *cmd );
static Bool doGrp( char *arg, const Cmd *cmd );
static Bool doHead( char *arg, const Cmd *cmd );
static Bool doHelp( char *arg, const Cmd *cmd );
static Bool doIhave( char *arg, const Cmd *cmd );
static Bool doLast( char *arg, const Cmd *cmd );
static Bool doList( char *arg, const Cmd *cmd );
static Bool doListgrp( char *arg, const Cmd *cmd );
static Bool doMode( char *arg, const Cmd *cmd );
static Bool doNewgrps( char *arg, const Cmd *cmd );
static Bool doNext( char *arg, const Cmd *cmd );
static Bool doPost( char *arg, const Cmd *cmd );
static Bool doSlave( char *arg, const Cmd *cmd );
static Bool doStat( char *arg, const Cmd *cmd );
static Bool doQuit( char *arg, const Cmd *cmd );
static Bool doXhdr( char *arg, const Cmd *cmd );
static Bool doXpat( char *arg, const Cmd *cmd );
static Bool doXOver( char *arg, const Cmd *cmd );
static Bool notImplemented( char *arg, const Cmd *cmd );
static void putStat( unsigned int stat, const char *fmt, ... );

Cmd commands[] =
{
    { "article", "ARTICLE [msg-id|n]", &doArt },
    { "body", "BODY [msg-id|n]", &doBody },
    { "head", "HEAD [msg-id|n]", &doHead },
    { "group", "GROUP grp", &doGrp },
    { "help", "HELP", &doHelp },
    { "ihave", "IHAVE (ignored)", &doIhave },
    { "last", "LAST", &doLast },
    { "list", "LIST [ACTIVE [pat]]|ACTIVE.TIMES [pat]|"
      "EXTENSIONS|NEWSGROUPS [pat]|OVERVIEW.FMT", &doList },
    { "listgroup", "LISTGROUP grp", &doListgrp },
    { "mode", "MODE (ignored)", &doMode },
    { "newgroups", "NEWGROUPS [xx]yymmdd hhmmss [GMT]", &doNewgrps },
    { "newnews", "NEWNEWS (not implemented)", &notImplemented },
    { "next", "NEXT", &doNext },
    { "post", "POST", &doPost },
    { "quit", "QUIT", &doQuit },
    { "slave", "SLAVE (ignored)", &doSlave },
    { "stat", "STAT [msg-id|n]", &doStat },
    { "xhdr", "XHDR over-field [msg-id|m[-[n]]]", &doXhdr },
    { "xpat", "XPAT over-field msg-id|m[-[n]] pat", &doXpat },
    { "xover", "XOVER [m[-[n]]]", &doXOver }
};

/*
  Notice interest in reading this group.
  Automatically subscribe if option set in config file.
*/
static void
noteInterest( void )
{
    FetchMode mode;

    Grp_setLastAccess( server.grp, time( NULL ) );
    if ( ! Grp_local ( server.grp ) && ! Online_true() )
    {
        Fetchlist_read();
        if ( ! Fetchlist_contains( server.grp ) )
	{
	    if ( Cfg_autoSubscribe() )
	    {
		if ( strcmp( Cfg_autoSubscribeMode(), "full" ) == 0 )
		    mode = FULL;
		else if ( strcmp( Cfg_autoSubscribeMode(), "thread" ) == 0 )
		    mode = THREAD;
		else
		    mode = OVER;
		Fetchlist_add( server.grp, mode );
		Fetchlist_write();
		Pseudo_autoSubscribed();
	    }
	    else if ( Cfg_infoAlways() )
	    {
		int first, last;

		/* Ensure new gen info for next time */
		first = Cont_first();
		last = Cont_last();

		if ( first == last )
		    first = last + 1;
		
		Grp_setFirstLast( Cont_grp(), first, last );
	    }
	}
    }
}

static void
putStat( unsigned int stat, const char *fmt, ... )
{
    Str s, line;
    va_list ap;

    ASSERT( stat <= 999 );
    va_start( ap, fmt );
    vsnprintf( s, MAXCHAR, fmt, ap );
    va_end( ap );
    snprintf( line, MAXCHAR, "%u %s", stat, s );
    Log_dbg( "[S] %s", line );
    printf( "%s\r\n", line );
}

static void
putTxtLn( const char *fmt, ... )
{
    Str line;
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( line, MAXCHAR, fmt, ap );
    va_end( ap );
    Prt_putTxtLn( line, stdout );
}

static void
putTxtBuf( const char *buf )
{
    if ( buf )
        Prt_putTxtBuf( buf, stdout );
}

static void
putEndOfTxt( void )
{
    Prt_putEndOfTxt( stdout );
}

static void
putSyntax( const Cmd *cmd )
{
    putStat( STAT_SYNTAX_ERR, "Syntax error. Usage: %s", cmd->syntax );
}

static void
readAlarm( int sig )
{
    UNUSED( sig );

    server.readAlarmFlag = TRUE;
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

/* Returns: < 0 on error, 0 on timeout, > 0 on success */
static int
waitCmdLn( Str line, int timeoutSeconds )
{
    sig_t oldHandler;
    int r;

    server.readAlarmFlag = FALSE;
    oldHandler = installSignalHandler( SIGALRM, readAlarm );
    if ( oldHandler == SIG_ERR )
    {
        Log_err( "server.c:waitCmdLn: signal failed." );
        return FALSE;
    }
    if ( alarm( ( unsigned int ) timeoutSeconds ) != 0 )
        Log_err( "server.c:waitCmdLn: Alarm was already set." );
    r = Prt_getLn( line, stdin, -1 );
    alarm( 0 );
    installSignalHandler( SIGALRM, oldHandler );
    if ( server.readAlarmFlag )
        return 0;
    else if ( r )
        return 1;
    return -1;
}

static Bool
getTxtLn( Str line, Bool *err )
{
    return Prt_getTxtLn( line, err, stdin, -1 );
}

static Bool
notImplemented( char *arg, const Cmd *cmd )
{
    UNUSED( arg );
    UNUSED( cmd );
    
    putStat( STAT_NO_PERMISSION, "Command not implemented" );
    return TRUE;
}

static void
checkNewArts( const char *grp )
{
    if ( ! Online_true()
         || strcmp( grp, server.grp ) == 0
         || Grp_local( grp )
         || time( NULL ) - Grp_lastAccess( server.grp ) < 1800 )
        return;
    if ( Fetch_init( Grp_server( grp ) ) )
    {
        Fetch_getNewArts( grp, OVER );
        Fetch_close();
    }
}

static void
postArts( void )
{
    Str s;

    Cfg_beginServEnum();
    while ( Cfg_nextServ( s ) )
        if ( Fetch_init( s ) )
        {
            Fetch_postArts();
            Fetch_close();
        }
}

static Bool
needsPseudoGenInfo( const char *grp )
{
    return ! ( Grp_local( grp )
	       || Fetchlist_contains( grp )
	       || Online_true() );
}

static void
readCont( const char *name )
{
    Fetchlist_read();
    Cont_read( name );
    if ( needsPseudoGenInfo( name ) )
    { 
        Pseudo_appGeneralInfo();
	/* This adds the pseudo message to the overview contents
	   but NOT to the group info. If an article gets added,
	   the group info will get updated then from the
	   contents, so the article number will be preserved.
	   Otherwise it will be lost when the content is discarded. */
    }
}

static void
changeToGrp( const char *grp )
{
    checkNewArts( grp );
    Utl_cpyStr( server.grp, grp );
    readCont( grp );
    server.artPtr = Cont_first();
}

static Bool
doGrp( char *arg, const Cmd *cmd )
{
    int first, last, numb;

    if ( arg[ 0 ] == '\0' )
        putSyntax( cmd );
    else if ( ! Grp_exists( arg ) )
        putStat( STAT_NO_SUCH_GRP, "No such group" );
    else
    {
        changeToGrp( arg );
        first = Cont_first();
        last = Cont_last();
	if ( ( first == 0 && last == 0 )
	     || first > last )
            first = last = numb = 0;
	else
	    numb = last - first + 1;
        putStat( STAT_GRP_SELECTED, "%lu %lu %lu %s selected",
                 numb, first, last, arg );
    }
    return TRUE;
}

static Bool
testGrpSelected( void )
{
    if ( *server.grp == '\0' )
    {
        putStat( STAT_NO_GRP_SELECTED, "No group selected" );
        return FALSE;
    }
    return TRUE;
}

static void
findServer( const char *msgId, Str result )
{
    const char *p, *pColon, *srv;
    Str s, grp;

    Utl_cpyStr( result, "(unknown)" );
    if ( Db_contains( msgId ) )
    {
        Utl_cpyStr( s, Db_xref( msgId ) );
        p = strtok( s, " \t" );
        if ( p )
            do
            {
                pColon = strstr( p, ":" );
                if ( pColon )
                {
                    Utl_cpyStrN( grp, p, pColon - p );
                    srv = Grp_server( grp );
                    if ( Cfg_servIsPreferential( srv, result ) )
                        Utl_cpyStr( result, srv );
                }
            }
            while ( ( p = strtok( NULL, " \t" ) ) );
    }
}

static Bool
retrieveArt( const char *msgId )
{
    Str s;

    findServer( msgId, s );    
    if ( strcmp( s, "(unknown)" ) == 0 
         || strcmp( s, GRP_LOCAL_SERVER_NAME ) == 0 )
        return FALSE;        
    if ( ! Client_connect( s ) )
        return FALSE;
    Client_retrieveArt( msgId );
    Client_disconnect();
    return TRUE;
}

static Bool
checkNumb( int numb )
{
    if ( ! testGrpSelected() )
        return FALSE;
    if ( ! Cont_validNumb( numb ) )
    {
        putStat( STAT_NO_SUCH_NUMB, "No such article" );
        return FALSE;
    }
    return TRUE;
}

/*
  Parse arguments for ARTICLE, BODY, HEAD, STAT commands.
  Return message-ID and article number (0 if unknown).
*/
static Bool
whichId( const char **msgId, int *numb, char *arg )
{
    const Over *ov;
    int n;
    Bool byMsgId = FALSE;

    if ( sscanf( arg, "%d", &n ) == 1 )
    {
        if ( ! checkNumb( n ) )
            return FALSE;
        server.artPtr = n;
        ov = Cont_get( n );
        *msgId = Ov_msgId( ov );
        *numb = n;
    }
    else if ( strcmp( arg, "" ) == 0 )
    {
        if ( ! checkNumb( server.artPtr ) )
            return FALSE;
        ov = Cont_get( server.artPtr );
        *msgId = Ov_msgId( ov );
        *numb =  server.artPtr;
    }
    else
    {
        *msgId = arg;
        *numb = 0;
        byMsgId = TRUE;
    }
    if ( ! Pseudo_isGeneralInfo( *msgId ) && ! Db_contains( *msgId ) )
    {
        if ( byMsgId )
            putStat( STAT_NO_SUCH_ID, "No such article" );
        else
            putStat( STAT_NO_SUCH_NUMB, "No such article" );
        return FALSE;
    }
    return TRUE;
}

static void
touchArticle( const char *msgId )
{
    int status = Db_status( msgId );
    status |= DB_INTERESTING;
    Db_setStatus( msgId, status );
    Db_updateLastAccess( msgId );
}

static void
touchReferences( const char *msgId )
{
    Str s;
    int len;
    char *p;
    const char *ref = Db_ref( msgId );

    while ( TRUE )
    {
        p = s;
        while ( *ref != '<' )
            if ( *(ref++) == '\0' )
                return;
        len = 0;
        while ( *ref != '>' )
        {
            if ( *ref == '\0' || ++len >= MAXCHAR - 1 )
                return;
            *(p++) = *(ref++);
        }
        *(p++) = '>';
        *p = '\0';
        if ( Db_contains( s ) )
            touchArticle( s );
    }
}

static void
doBodyInDb( const char *msgId )
{
    int stat;
    Str srv;

    touchArticle( msgId );
    touchReferences( msgId );
    stat = Db_status( msgId );
    if ( Online_true() && ( stat & DB_NOT_DOWNLOADED ) )
    {
        retrieveArt( msgId );
        stat = Db_status( msgId );
    }
    if ( stat & DB_RETRIEVING_FAILED )
    {
        Db_setStatus( msgId, stat & ~DB_RETRIEVING_FAILED );
        putTxtBuf( Db_body( msgId ) );
    }
    else if ( stat & DB_NOT_DOWNLOADED )
    {
        findServer( msgId, srv );
        if ( Req_contains( srv, msgId ) )
            putTxtBuf( Pseudo_alreadyMarkedBody() );
        else if ( strcmp( srv, "(unknown)" ) != 0 && 
		  strcmp( srv, GRP_LOCAL_SERVER_NAME ) != 0 && 
		  Req_add( srv, msgId ) )
            putTxtBuf( Pseudo_markedBody() );
        else
            putTxtBuf( Pseudo_markingFailedBody() );
    }
    else
        putTxtBuf( Db_body( msgId ) );
}

static Bool
doBody( char *arg, const Cmd *cmd )
{
    const char *msgId;
    int numb;

    UNUSED( cmd );
    
    if ( ! whichId( &msgId, &numb, arg ) )
        return TRUE;
    putStat( STAT_BODY_FOLLOWS, "%ld %s Body", numb, msgId );
    if ( Pseudo_isGeneralInfo( msgId ) )
        putTxtBuf( Pseudo_generalInfoBody() );
    else
        doBodyInDb( msgId );
    putEndOfTxt();
    noteInterest();
    return TRUE;
}

static void
doHeadInDb( const char *msgId )
{
    putTxtBuf( Db_header( msgId ) );
}

static Bool
doHead( char *arg, const Cmd *cmd )
{
    const char *msgId;
    int numb;

    UNUSED( cmd );
    
    if ( ! whichId( &msgId, &numb, arg ) )
        return TRUE;
    putStat( STAT_HEAD_FOLLOWS, "%ld %s Head", numb, msgId );
    if ( Pseudo_isGeneralInfo( msgId ) )
        putTxtBuf( Pseudo_generalInfoHead() );
    else
        doHeadInDb( msgId );
    putEndOfTxt();
    return TRUE;
}

static void
doArtInDb( const char *msgId )
{
    doHeadInDb( msgId );
    putTxtLn( "" );
    doBodyInDb( msgId );
}

static Bool
doArt( char *arg, const Cmd *cmd )
{
    const char *msgId;
    int numb;
    
    UNUSED( cmd );
    
    if ( ! whichId( &msgId, &numb, arg ) )
        return TRUE;
    putStat( STAT_ART_FOLLOWS, "%ld %s Article", numb, msgId );
    if ( Pseudo_isGeneralInfo( msgId ) )
    {
        putTxtBuf( Pseudo_generalInfoHead() );
        putTxtLn( "" );
        putTxtBuf( Pseudo_generalInfoBody() );
    }
    else
        doArtInDb( msgId );
    putEndOfTxt();
    noteInterest();
    return TRUE;
}

static Bool
doHelp( char *arg, const Cmd *cmd )
{
    unsigned int i;

    UNUSED( arg );
    UNUSED( cmd );

    putStat( STAT_HELP_FOLLOWS, "Help" );
    putTxtBuf( "\nCommands:\n\n" );
    for ( i = 0; i < sizeof( commands ) / sizeof( commands[ 0 ] ); ++i )
        putTxtLn( "%s", commands[ i ].syntax );
    putEndOfTxt();
    return TRUE;
}

static Bool
doIhave( char *arg, const Cmd *cmd )
{
    UNUSED( arg );
    UNUSED( cmd );
    
    putStat( STAT_ART_REJECTED, "Command not used" );
    return TRUE;
}

static Bool
doLast( char *arg, const Cmd *cmd )
{
    int n;

    UNUSED( arg );
    UNUSED( cmd );

    if ( testGrpSelected() )
    {
        n = server.artPtr;
        if ( ! Cont_validNumb( n ) )
            putStat( STAT_NO_ART_SELECTED, "No article selected" );
        else
        {
            while ( ! Cont_validNumb( --n ) && n >= Cont_first() );
            if ( ! Cont_validNumb( n ) )
                putStat( STAT_NO_PREV_ART, "No previous article" );
            else
            {
                putStat( STAT_ART_RETRIEVED, "%ld %s selected",
                         n, Ov_msgId( Cont_get( n ) ) );
                server.artPtr = n;
            }
        }
    }
    return TRUE;
}

static Bool
containsWildcards( const char *pattern )
{
    return ( strpbrk( pattern, "?*[\\" ) == NULL ? FALSE : TRUE );
}

static void
printGroups( const char *pat, void (*printProc)( Str, const char* ) )
{
    Str line;
    const char *g;
    FILE *f;
    sig_t lastHandler;
    int ret;

    putStat( STAT_GRPS_FOLLOW, "Groups" );
    fflush( stdout );
    Log_dbg( "[S FLUSH]" );
    if ( containsWildcards( pat ) )
    {
        lastHandler = signal( SIGPIPE, SIG_IGN );
        f = popen( "sort", "w" );
        if ( f == NULL )
        {
            Log_err( "Cannot open pipe to 'sort'" );
            if ( Grp_firstGrp( &g ) )
                do
                    if ( Wld_match( g, pat ) )
                    {
                        (*printProc)( line, g );
                        if ( ! Prt_putTxtLn( line, stdout ) )
                            Log_err( "Writing to stdout failed." );
                    }
                while ( Grp_nextGrp( &g ) );
        }
        else
        {
            if ( Grp_firstGrp( &g ) )
                do
                    if ( Wld_match( g, pat ) )
                    {
                        (*printProc)( line, g );
                        if ( ! Prt_putTxtLn( line, f ) )
                        {
                            Log_err( "Writing to 'sort' pipe failed." );
                            break;
                        }                    
                    }
                while ( Grp_nextGrp( &g ) );
            ret = pclose( f );
            if ( ret != EXIT_SUCCESS )
                Log_err( "sort command returned %d", ret );
            fflush( stdout );
            Log_dbg( "[S FLUSH]" );
            signal( SIGPIPE, lastHandler );
        }
    }
    else if ( Grp_exists( pat ) )
    {
        (*printProc)( line, pat );
        if ( ! Prt_putTxtLn( line, stdout ) )
            Log_err( "Writing to stdout failed." );
    }                    
    putEndOfTxt();
}

static void
printActiveTimes( Str result, const char *grp )
{
    snprintf( result, MAXCHAR, "%s %ld", grp, Grp_created( grp ) );
}

static void
doListActiveTimes( const char *pat )
{
    printGroups( pat, &printActiveTimes );
}

static void
printActive( Str result, const char *grp )
{
    int last;
    
    /* If there will be a pseudo gen info message waiting when we join
       this group, fiddle the group numbers to show it. */
    last = Grp_last( grp );
    if ( needsPseudoGenInfo( grp ) )
	last++;
    
    snprintf( result, MAXCHAR, "%s %d %d %c",
              grp, last, Grp_first( grp ), Grp_postAllow( grp ) );
}

static void
doListActive( const char *pat )
{
    /* We need to read the fetchlist so we know whether a pseudo
       gen info article needs to be faked when printing the group
       last. */
    Fetchlist_read();
    printGroups( pat, &printActive );
}

static void
printNewsgrp( Str result, const char *grp )
{
    snprintf( result, MAXCHAR, "%s %s", grp, Grp_dsc( grp ) );
}

static void
doListNewsgrps( const char *pat )
{
    printGroups( pat, &printNewsgrp );
}

static void
putGrp( const char *name )
{
    putTxtLn( "%s %lu %lu y", name, Grp_last( name ), Grp_first( name ) );
}

static void
doListOverFmt( void )
{
    putStat( STAT_GRPS_FOLLOW, "Overview format" );
    putTxtBuf( "Subject:\n"
               "From:\n"
               "Date:\n"
               "Message-ID:\n"
               "References:\n"
               "Bytes:\n"
               "Lines:\n" );
    putEndOfTxt();
}

static void
doListExtensions( void )
{
    putStat( STAT_CMD_OK, "Extensions" );
    putTxtBuf( " LISTGROUP\n"
               " XOVER\n" );
    putEndOfTxt();    
}

static Bool
doList( char *line, const Cmd *cmd )
{
    Str s, arg;
    const char *pat;

    if ( sscanf( line, "%s", s ) != 1 )
        doListActive( "*" );
    else
    {
        Utl_toLower( s );
        strcpy( arg, Utl_restOfLn( line, 1 ) );
        pat = Utl_stripWhiteSpace( arg );
        if ( pat[ 0 ] == '\0' )
            pat = "*";
        if ( strcmp( "active", s ) == 0 )
            doListActive( pat );
        else if ( strcmp( "overview.fmt", s ) == 0 )
            doListOverFmt();
        else if ( strcmp( "newsgroups", s ) == 0 )
            doListNewsgrps( pat );
        else if ( strcmp( "active.times", s ) == 0 )
            doListActiveTimes( pat );
        else if ( strcmp( "extensions", s ) == 0 )
            doListExtensions();
        else
            putSyntax( cmd );
    }
    return TRUE;
}

static Bool
doListgrp( char *arg, const Cmd *cmd )
{
    const Over *ov;
    int first, last, i;

    UNUSED( cmd );

    if ( ! Grp_exists( arg ) )
        putStat( STAT_NO_SUCH_GRP, "No such group" );
    else
    {
        changeToGrp( arg );
        first = Cont_first();
        last = Cont_last();
        putStat( STAT_GRP_SELECTED, "Article list" );
        for ( i = first; i <= last; ++i )
            if ( ( ov = Cont_get( i ) ) )
                putTxtLn( "%lu", i );
        putEndOfTxt();
    }
    return TRUE;
}

static Bool
doMode( char *arg, const Cmd *cmd )
{
    UNUSED( arg );
    UNUSED( cmd );
    
    putStat( STAT_READY_POST_ALLOW, "Ok" );
    return TRUE;
}

/* Can return -1, if date is outside the range of time_t. */
static time_t
getTimeInSeconds( int year, int mon, int day, int hour, int min, int sec )
{
    struct tm t;
    time_t result;

    ASSERT( year >= 1900 );
    ASSERT( mon >= 1 );
    ASSERT( mon <= 12 );
    ASSERT( day >= 1 );
    ASSERT( day <= 31 );
    ASSERT( hour >= 0 );
    ASSERT( hour <= 23 );
    ASSERT( min >= 0 );
    ASSERT( min <= 59 );
    ASSERT( sec >= 0 );
    ASSERT( sec <= 59 );
    memset( &t, 0, sizeof( t ) );
    t.tm_year = year - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;
    result = mktime( &t );
    return result;
}


static Bool
doNewgrps( char *arg, const Cmd *cmd )
{
    time_t t, now, lastUpdate, nextCentBegin;
    int year, mon, day, hour, min, sec, cent, len;
    const char *g;
    Str date, timeofday, file;

    if ( sscanf( arg, "%s %s", date, timeofday ) != 2 ) 
    {
        putSyntax( cmd );
        return TRUE;
    }
    len = strlen( date );
    switch ( len )
    {
    case 6:
        if ( sscanf( date, "%2d%2d%2d", &year, &mon, &day ) != 3 )
        {
            putSyntax( cmd );
            return TRUE;
        }
        now = time( NULL );
        cent = 1900;
        nextCentBegin = getTimeInSeconds( cent + 100, 1, 1, 0, 0, 0 );
        while ( nextCentBegin != (time_t)-1 && now != (time_t)-1 
                && now > nextCentBegin )
        {
            cent += 100;
            nextCentBegin = getTimeInSeconds( cent + 100, 1, 1, 0, 0, 0 );
        }
        year += cent;
        break;
    case 8:
        if ( sscanf( date, "%4d%2d%2d", &year, &mon, &day ) != 3 )
        {
            putSyntax( cmd );
            return TRUE;
        }
        break;
    default:
        putSyntax( cmd );
        return TRUE;
    }
    if ( sscanf( timeofday, "%2d%2d%2d", &hour, &min, &sec ) != 3 )
    {
        putSyntax( cmd );
        return TRUE;
    }
    if ( year < 1970 || mon < 1 || mon > 12 || day < 1 || day > 31
         || hour < 0 || hour > 23 || min < 0 || min > 59
         || sec < 0 || sec > 60 )
    {
        putSyntax( cmd );
        return TRUE;
    }
    snprintf( file, MAXCHAR, "%s/groupinfo.lastupdate", Cfg_spoolDir() );
    t = getTimeInSeconds( year, mon, day, hour, min, sec );
    putStat( STAT_NEW_GRP_FOLLOW, "New groups since %s", arg );

    if ( ! Utl_getStamp( &lastUpdate, file )
         || t == (time_t)-1 || t <= lastUpdate )
    {
        if ( Grp_firstGrp( &g ) )
            do
                if ( Grp_created( g ) > t )
                    putGrp( g );
            while ( Grp_nextGrp( &g ) );
    }
    putEndOfTxt();
    return TRUE;
}

static Bool
doNext( char *arg, const Cmd *cmd )
{
    int n;

    UNUSED(arg);
    UNUSED(cmd);

    if ( testGrpSelected() )
    {
        n = server.artPtr;
        if ( ! Cont_validNumb( n ) )
            putStat( STAT_NO_ART_SELECTED, "No article selected" );
        else
        {
            while ( ! Cont_validNumb( ++n ) && n <= Cont_last() );
            if ( ! Cont_validNumb( n ) )
                putStat( STAT_NO_NEXT_ART, "No next article" );
            else
            {
                putStat( STAT_ART_RETRIEVED, "%ld %s selected",
                         n, Ov_msgId( Cont_get( n ) ) );
                server.artPtr = n;
            }
        }
    }
    return TRUE;
}

/* Cancel and return TRUE if need to send cancel message on to server. */
static Bool
controlCancel( const char *cancelId )
{
    return ( Ctrl_cancel( cancelId ) == CANCEL_NEEDS_MSG );
}

/*
  It's a control message. Currently we only know about 'cancel'
  messages; others are passed on for outside groups, and logged
  as ignored for local groups.
 */
static Bool
handleControl( ItemList *control, ItemList *newsgroups,
	       const char *msgId, const DynStr *art )
{
    const char *grp;
    const char *op;
    Bool err = FALSE;
    Bool localDone = FALSE;

    op = Itl_first( control );
    if ( op == NULL )
    {
	Log_err( "Malformed control line." );
	return TRUE;
    }
    else if ( strcasecmp( op, "cancel" ) == 0 )
    {
	if ( controlCancel( Itl_next( control ) ) )
	    localDone = TRUE;
	else
	    return err;
    }

    /* Pass on for outside groups. */
    for( grp = Itl_first( newsgroups );
	 grp != NULL;
	 grp = Itl_next( newsgroups ) )
    {
	if ( Grp_exists( grp ) && ! Grp_local( grp ) )
	{
	    if ( ! Out_add( Grp_server( grp ), msgId, art ) )
	    {
		Log_err( "Cannot add posted article to outgoing directory" );
		err = TRUE;
	    }
	    break;
	}
    }

    if ( localDone )
	return err;

    /* Log 'can't do' for internal groups. */
    for( grp = Itl_first( newsgroups );
	 grp != NULL;
	 grp = Itl_next( newsgroups ) )
    {
	if ( Grp_exists( grp ) && Grp_local( grp ) )
	    Log_inf( "Ignoring control '%s' for '%s'.", op, grp );
    }

    return err;
}

static Bool
postArticle( ItemList *newsgroups, const char *msgId, const DynStr *art )
{
    const char *grp;
    Bool err;
    Bool oneLocal;
    Str serversSeen;

    err = oneLocal = FALSE;

    /*
     * Run round first doing all local groups.
     * Remember, we've already checked it is OK to post to them all.
     */ 
    for ( grp = Itl_first( newsgroups );
	  grp != NULL;
	  grp = Itl_next( newsgroups ) )
    {
	if ( Grp_local( grp ) )
	{
	    if ( ! oneLocal )
	    {
		if ( ! Post_open( DynStr_str( art ) ) )
		{
		    err = TRUE;
		    break;
		}
		else
		    oneLocal = TRUE;
	    }

	    if ( ! Post_add( grp ) )
		err = TRUE;
	}
    }
    if ( oneLocal )
	Post_close();

    /*
     * For each external group, send to that group's server if it has
     * not seen the post already.
     */
    serversSeen[ 0 ] = '\0';
    
    for( grp = Itl_first( newsgroups );
	 grp != NULL;
	 grp = Itl_next( newsgroups ) )
    {
	if ( Grp_exists( grp ) && ! Grp_local( grp ) )
	{
	    const char * servName = Grp_server( grp );

	    if ( strstr( serversSeen, servName ) != NULL )
		continue;
	    
	    if ( ! Out_add( servName, msgId, art ) )
	    {
		Log_err( "Cannot add posted article to outgoing directory" );
		err = TRUE;
	    }
	    Utl_catStr( serversSeen, " " );
	    Utl_catStr( serversSeen, servName );
	}
    }

    return err;
}

static Bool
doPost( char *arg, const Cmd *cmd )
{
    Bool err, replyToFound, dateFound, inHeader, approved;
    DynStr *s;
    Str line, field, val, msgId, from;
    const char* p;
    ItemList * newsgroups, *control;

    UNUSED(arg);
    UNUSED(cmd);
    
    /*
      Get article and make following changes to the header:
      - add/replace/cut Message-ID depending on config options
      - add Reply-To with content of From, if missing
      (some providers overwrite From field)
      - rename X-Sender header to X-NOFFLE-X-Sender
      (some providers want to insert their own X-Sender)

      For doing this, it is not necessary to parse multiple-line
      headers.
    */
    putStat( STAT_SEND_ART, "Continue (end with period)" );
    fflush( stdout );
    Log_dbg( "[S FLUSH]" );
    s = new_DynStr( 10000 );
    msgId[ 0 ] = '\0';
    from[ 0 ] = '\0';
    newsgroups = control = NULL;
    replyToFound = dateFound = approved = FALSE;
    inHeader = TRUE;
    while ( getTxtLn( line, &err ) )
    {
        if ( inHeader )
        {
            p = Utl_stripWhiteSpace( line );
            if ( *p == '\0' )
            {
                inHeader = FALSE;
                if ( from[ 0 ] == '\0' )
                    Log_err( "Posted message has no From field" );
                if ( Cfg_replaceMsgId() )
                {
                    Prt_genMsgId( msgId, from, "NOFFLE" );
                    Log_dbg( "Replacing Message-ID with '%s'", msgId );
                }
                else if ( msgId[ 0 ] == '\0' )
                {
                    Prt_genMsgId( msgId, from, "NOFFLE" );
                    
                    Log_inf( "Adding missing Message-ID '%s'", msgId );
                }
                DynStr_app( s, "Message-ID: " );
                DynStr_appLn( s, msgId );
                if ( ! replyToFound && from[ 0 ] != '\0' )
                {
                    Log_dbg( "Adding Reply-To field to posted message." );
                    DynStr_app( s, "Reply-To: " );
                    DynStr_appLn( s, from );
                }
		if ( ! dateFound )
		{
		    time_t t;

		    time( &t );
		    Utl_rfc822Date( t, val );
		    DynStr_app( s, "Date: " );
		    DynStr_appLn( s, val );
		}
                DynStr_appLn( s, p );
            }
            else if ( Prt_getField( field, val, p ) )
            {
                if ( strcmp( field, "message-id" ) == 0 )
                    strcpy( msgId, val );
                else if ( strcmp( field, "from" ) == 0 )
                {
                    strcpy( from, val );
                    DynStr_appLn( s, p );
                }
                else if ( strcmp( field, "newsgroups" ) == 0 )
                {
		    Utl_toLower( val );
		    newsgroups = new_Itl ( val, " ," );
                    DynStr_appLn( s, p );
                }
                else if ( strcmp( field, "control" ) == 0 )
                {
		    control = new_Itl ( val, " " );
                    DynStr_appLn( s, p );
                }
                else if ( strcmp( field, "reply-to" ) == 0 )
                {
                    replyToFound = TRUE;
                    DynStr_appLn( s, p );
                }
                else if ( strcmp( field, "approved" ) == 0 )
                {
                    approved = TRUE;
                    DynStr_appLn( s, p );
                }
                else if ( strcmp( field, "date" ) == 0 )
                {
                    dateFound = TRUE;
                    DynStr_appLn( s, p );
                }
                else if ( strcmp( field, "x-sender" ) == 0 )
                {
                    DynStr_app( s, "X-NOFFLE-X-Sender: " );
                    DynStr_appLn( s, val );
                }
                else
                    DynStr_appLn( s, p );
            }
            else
                DynStr_appLn( s, line );
        }
        else
            DynStr_appLn( s, line );
    }
    if ( inHeader )
        Log_err( "Posted message has no body" );
    if ( ! err )
    {
        if ( newsgroups == NULL || Itl_count( newsgroups ) == 0 )
        {
            Log_err( "Posted message has no valid Newsgroups header field" );
            err = TRUE;
        }
        else
	{
	    const char *grp;
	    Bool knownGrp = FALSE;
	    Bool postAllowedGrp = TRUE;

	    /*
	     * Check all known groups are writeable, and there is
	     * at least one known group.
	     */
	    for( grp = Itl_first( newsgroups );
		 postAllowedGrp && grp != NULL;
		 grp = Itl_next( newsgroups ) )
	    {
		if ( Grp_exists( grp ) )
		{
		    knownGrp = TRUE;
		    switch( Grp_postAllow( grp ) )
		    {
		    case 'n':
			postAllowedGrp = FALSE;
			break;
		    case 'y':
			break;
		    case 'm':
			/*
			 * Can post to moderated group if either
			 * 1. It is external, or
			 * 2. The article is approved.
			 */
			postAllowedGrp =
			    ! Grp_local( grp ) ||
			    approved;
			break;
		    default:
			/*
			 * Unknown mode for local groups. Forward
			 * to server for external groups; presumably the
			 * server knows what to do.
			 */
			postAllowedGrp = ! Grp_local( grp );
			break;
		    }
		}
	    }
	    
	    if ( ! knownGrp )
	    {

		Log_err( "No known group in Newsgroups header field" );
		err = TRUE;
	    }
	    else if ( ! postAllowedGrp )
	    {

		Log_err( "A group does not permit posting" );
		err = TRUE;
	    }
	    else
	    {
		err = ( control == NULL )
		    ? postArticle( newsgroups, msgId, s )
		    : handleControl( control, newsgroups, msgId, s );
	    }	    
	}
    }
    if ( err )
        putStat( STAT_POST_FAILED, "Posting failed" );
    else
    {
        putStat( STAT_POST_OK, "Message posted" );
        if ( Online_true() )
            postArts();
    }
    del_Itl( newsgroups );
    del_Itl( control );
    del_DynStr( s );
    return TRUE;
}

static void
parseRange( const char *s, int *first, int *last, int *numb )
{
    int r, i;
    char* p;
    Str t;

    Utl_cpyStr( t, s );
    p = Utl_stripWhiteSpace( t );
    r = sscanf( p, "%d-%d", first, last );
    if ( r < 1 )
    {
        *first = server.artPtr;
        *last = server.artPtr;
    }
    else if ( r == 1 )
    {
        if ( p[ strlen( p ) - 1 ] == '-' )
            *last = Cont_last();
        else
            *last = *first;
    }    
    if ( *first < Cont_first() )
        *first = Cont_first();
    if ( *last > Cont_last() )
        *last = Cont_last();
    if ( *first > Cont_last() ||  *last < Cont_first() )
        *last = *first - 1;
    *numb = 0;
    for ( i = *first; i <= *last; ++i )
        if ( Cont_validNumb( i ) )
            ++(*numb);
}

enum XhdrType { SUBJ, FROM, DATE, MSG_ID, REF, BYTES, LINES, XREF, UNKNOWN };

static enum XhdrType
whatXhdrField( const char * fieldname )
{
    Str name;

    Utl_cpyStr( name, fieldname );
    Utl_toLower( name );
    if ( strcmp( name, "subject" ) == 0 )
        return SUBJ;
    else if ( strcmp( name, "from" ) == 0 )
        return FROM;
    else if ( strcmp( name, "date" ) == 0 )
        return DATE;
    else if ( strcmp( name, "message-id" ) == 0 )
        return MSG_ID;
    else if ( strcmp( name, "references" ) == 0 )
        return REF;
    else if ( strcmp( name, "bytes" ) == 0 )
        return BYTES;
    else if ( strcmp( name, "lines" ) == 0 )
        return LINES;
    else if ( strcmp( name, "xref" ) == 0 )
        return XREF;
    else
	return UNKNOWN;
}

static void
getXhdrField( enum XhdrType what, const Over * ov, Str res )
{
    const char * msgId;
    Str host;
    
    switch ( what )
    {
    case SUBJ:
	Utl_cpyStr( res, Ov_subj( ov ) );
	break;
    case FROM:
	Utl_cpyStr( res, Ov_from( ov ) );
	break;
    case DATE:
	Utl_cpyStr( res, Ov_date( ov ) );
	break;
    case MSG_ID:
	Utl_cpyStr( res, Ov_msgId( ov ) );
	break;
    case REF:
	Utl_cpyStr( res, Ov_ref( ov ) );
	break;
    case BYTES:
	snprintf( res, MAXCHAR, "%d", Ov_bytes( ov ) );
	break;
    case LINES:
	snprintf( res, MAXCHAR, "%d", Ov_lines( ov ) );
	break;
    case XREF:
	msgId = Ov_msgId( ov );
	/*
	 * Gen info messages don't have an Xref header. When INN is asked
	 * for a header that doesn't exist in a message, it reports the
	 * header value as '(none)', so do the same.
	 */
	if ( Pseudo_isGeneralInfo( msgId ) )
	    Utl_cpyStr( res, "none" );
	else
	{
	    gethostname( host, MAXCHAR );
	    snprintf( res, MAXCHAR, "%s %s", host, Db_xref( msgId ) );
	}
	break;
    default:
	ASSERT( FALSE );
    }
}

/*
  Note this only handles a subset of headers. But they are all
  the headers any newsreader should need to work properly.
 
  That last sentence will at some stage be proved wrong.
 */
static Bool
doXhdr( char *arg, const Cmd *cmd )
{
    enum XhdrType what;
    const char *p;
    Str whatStr;

    if ( sscanf( arg, "%s", whatStr ) != 1 )
    {
        putSyntax( cmd );
        return TRUE;
    }
    what = whatXhdrField( whatStr );
    if ( what == UNKNOWN )
    {
        putStat( STAT_HEAD_FOLLOWS, "Unknown header (empty list follows)" );
        putEndOfTxt();
        return TRUE;
    }
    p = Utl_restOfLn( arg, 1 );
    if ( p[ 0 ] == '<' )
    {
	Over * ov;
	Str field;
	
	/* Argument is message ID */
	ov = Db_over( p );
	if ( ov == NULL )
	{
	    putStat( STAT_NO_SUCH_ID, "No such article" );
	    return TRUE;
	}
        putStat( STAT_HEAD_FOLLOWS, "%s header %s", whatStr, p ) ;
	getXhdrField( what, ov, field );
	putTxtLn( "%s %s", p, field );
	del_Over( ov );
    }
    else
    {
	const Over * ov;
	int first, last, i, n, numb;
	Str field;
	
	/* Argument is article no. or range */
	if ( ! testGrpSelected() )
	    return TRUE;
	parseRange( p, &first, &last, &numb );
	if ( numb == 0 )
	{
	    putStat( STAT_NO_ART_SELECTED, "No articles selected" );
	    return TRUE;
	}
        putStat( STAT_HEAD_FOLLOWS, "%s header %lu-%lu",
		 whatStr, first, last ) ;
        for ( i = first; i <= last; ++i )
            if ( ( ov = Cont_get( i ) ) )
            {
                n = Ov_numb( ov );
		getXhdrField( what, ov, field );
		putTxtLn( "%lu %s", n, field );
            }
    }
    putEndOfTxt();
    return TRUE;
}

static Bool
doXpat( char *arg, const Cmd *cmd )
{
    enum XhdrType what;
    Str whatStr, articles, pat;

    if ( sscanf( arg, "%s %s %s", whatStr, articles, pat ) != 3 )
    {
	putSyntax( cmd );
	return TRUE;
    }
    what = whatXhdrField( whatStr );
    if ( what == UNKNOWN )
    {
        putStat( STAT_HEAD_FOLLOWS, "Unknown header (empty list follows)" );
        putEndOfTxt();
        return TRUE;
    }
    if ( articles[ 0 ] == '<' )
    {
	Over * ov;
	Str field;
	
	/* Argument is message ID */
	ov = Db_over( articles );
	if ( ov == NULL )
	{
	    putStat( STAT_NO_SUCH_ID, "No such article" );
	    return TRUE;
	}
        putStat( STAT_HEAD_FOLLOWS, "%s header %s", whatStr, articles ) ;
	getXhdrField( what, ov, field );
	if ( Wld_match( field, pat ) )
	    putTxtLn( "%s %s", articles, field );
	del_Over( ov );
    }
    else
    {
	const Over * ov;
	Str field;
	int first, last, i, n, numb;

	/* Argument is article no. or range */
	if ( ! testGrpSelected() )
	    return TRUE;
	parseRange( articles, &first, &last, &numb );
	if ( numb == 0 )
	{
	    putStat( STAT_NO_ART_SELECTED, "No articles selected" );
	    return TRUE;
	}
        putStat( STAT_HEAD_FOLLOWS, "%s header %lu-%lu",
		 whatStr, first, last ) ;
        for ( i = first; i <= last; ++i )
            if ( ( ov = Cont_get( i ) ) )
            {
                n = Ov_numb( ov );
		getXhdrField( what, ov, field );
		if ( Wld_match( field, pat ) )
		    putTxtLn( "%lu %s", n, field );
            }
    }
    putEndOfTxt();
    return TRUE;
}

static Bool
doSlave( char *arg, const Cmd *cmd )
{
    UNUSED( arg );
    UNUSED( cmd );
    
    putStat( STAT_CMD_OK, "Ok" );
    return TRUE;
}

static Bool
doStat( char *arg, const Cmd *cmd )
{
    const char *msgId;
    int numb;

    UNUSED( cmd );
    
    if ( ! whichId( &msgId, &numb, arg ) )
        return TRUE;
    if ( numb > 0 )
        putStat( STAT_ART_RETRIEVED, "%ld %s selected",
                 numb, msgId );
    else
        putStat( STAT_ART_RETRIEVED, "0 %s selected", msgId );
    return TRUE;
}

static Bool
doQuit( char *arg, const Cmd *cmd )
{
    UNUSED( arg );
    UNUSED( cmd );
    
    putStat( STAT_GOODBYE, "Goodbye" );
    return FALSE;
}

static Bool
doXOver( char *arg, const Cmd *cmd )
{
    int first, last, i, n;
    const Over *ov;

    UNUSED( cmd );
    
    if ( ! testGrpSelected() )
        return TRUE;
    parseRange( arg, &first, &last, &n );
    if ( n == 0 )
	first = last = server.artPtr;
    putStat( STAT_OVERS_FOLLOW, "Overview %ld-%ld", first, last );
    for ( i = first; i <= last; ++i )
	if ( ( ov = Cont_get( i ) ) )
	    putTxtLn( "%lu\t%s\t%s\t%s\t%s\t%s\t%d\t%d\t",
		      Ov_numb( ov ), Ov_subj( ov ), Ov_from( ov ),
		      Ov_date( ov ), Ov_msgId( ov ), Ov_ref( ov ),
		      Ov_bytes( ov ), Ov_lines( ov ) );
    putEndOfTxt();
    return TRUE;
}

static void
putFatal( const char *fmt, ... )
{
    va_list ap;
    Str s;

    va_start( ap, fmt );
    vsnprintf( s, MAXCHAR, fmt, ap );
    va_end( ap );
    Log_err( s );
    putStat( STAT_PROGRAM_FAULT, "%s", s );
    fflush( stdout );
    Log_dbg( "[S FLUSH]" );
}

/* Parse line, execute command and return FALSE, if it was the quit command. */
static Bool
parseAndExecute( Str line )
{
    unsigned int i, n;
    Cmd *c;
    Str s, arg;
    Bool ret;

    if ( sscanf( line, "%s", s ) == 1 )
    {
        Utl_toLower( s );
        strcpy( arg, Utl_restOfLn( line, 1 ) );
        n = sizeof( commands ) / sizeof( commands[ 0 ] );
        for ( i = 0, c = commands; i < n; ++i, ++c )
            if ( strcmp( c->name, s ) == 0 )
            {
                ret = c->cmdProc( Utl_stripWhiteSpace( arg ), c );
                fflush( stdout );
                Log_dbg( "[S FLUSH]" );
                return ret;
            }
    }
    putStat( STAT_NO_SUCH_CMD, "Command not recognized" );
    fflush( stdout );
    Log_dbg( "[S FLUSH]" );
    return TRUE;
}

static void
putWelcome( void )
{
    putStat( STAT_READY_POST_ALLOW, "NNTP server NOFFLE %s",
             Cfg_version() );
    fflush( stdout );
    Log_dbg( "[S FLUSH]" );
}

static Bool
initServer( void )
{
    ASSERT( ! server.running );
    if ( ! Lock_openDatabases() )
      return FALSE;
    server.running = TRUE;
    return TRUE;
}

static void
closeServer( void )
{
    ASSERT( server.running );
    server.running = FALSE;
    Lock_closeDatabases();
}

void
Server_run( void )
{
    Bool done;
    int r;
    Str line;

    putWelcome();
    done = FALSE;
    while ( ! done )
    {
        r = waitCmdLn( line, 5 );
        if ( r < 0 )
        {
            Log_inf( "Client disconnected. Terminating." );
            done = TRUE;
        }
        else if ( r == 0 )
        {
            if ( server.running )
                closeServer();
        }
        else /* ( r > 0 ) */
        {
            if ( ! server.running )
            {
                if ( ! initServer() )
                {
                    putFatal( "Cannot init server" );
                    done = TRUE;
                }
            }
            if ( ! parseAndExecute( line ) )
                done = TRUE;
        }
    }
    if ( server.running )
        closeServer();
}
