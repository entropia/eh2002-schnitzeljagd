/*
  noffle.c

  Main program. Implements specified actions, but running as server, which
  is done by Server_run(), declared in server.h.
  
  Locking policy: lock access to databases while noffle is running, but
  not as server. If noffle runs as server, locking is performed while
  executing NNTP commands, but temporarily released if no new command is
  received for some seconds (to allow multiple clients connect at the same
  time).

  $Id: noffle.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <sys/resource.h>
#include <syslog.h>
#include <unistd.h>
#include "client.h"
#include "common.h"
#include "content.h"
#include "control.h"
#include "configfile.h"
#include "database.h"
#include "fetch.h"
#include "fetchlist.h"
#include "group.h"
#include "itemlist.h"
#include "log.h"
#include "online.h"
#include "outgoing.h"
#include "over.h"
#include "pseudo.h"
#include "util.h"
#include "server.h"
#include "request.h"
#include "lock.h"
#include "portable.h"


struct Noffle
{
    Bool queryGrps;
    Bool queryDsc;
    Bool interactive;
} noffle = { FALSE, FALSE, TRUE };

static void
doArt( const char *msgId )
{
    const char *id;

    if ( strcmp( msgId, "all" ) == 0 )
    {
        if ( ! Db_first( &id ) )
            fprintf( stderr, "Database empty.\n" );
        else
            do
            {
                printf( "From %s %s\n"
                        "%s\n"
                        "%s\n",
                        Db_from( id ), Db_date( id ),
                        Db_header( id ),
                        Db_body( id ) );
            }
            while ( Db_next( &id ) );
    }
    else
    {
        if ( ! Db_contains( msgId ) )
            fprintf( stderr, "Not in database.\n" );
        else
            printf( "%s\n%s", Db_header( msgId ), Db_body( msgId ) );
    }
}

static void
doCancel( const char *msgId )
{
    switch( Ctrl_cancel( msgId ) )
    {
    case CANCEL_NO_SUCH_MSG:
	printf( "No such message '%s'.\n", msgId );
	break;

    case CANCEL_OK:
	printf( "Message '%s' cancelled.\n", msgId );
	break;

    case CANCEL_NEEDS_MSG:
	printf( "Message '%s' cancelled in local database only.\n", msgId );
	break;
    }
}

/* List articles requested from one particular server */
static void
listRequested1( const char* serv )
{
  Str msgid;

  if ( ! Req_first( serv, msgid ) )
      return;
  do
      printf( "%s %s\n", msgid, serv );
  while ( Req_next( msgid ) );
}

/* List requested articles. List for all servers if serv = "all" or serv =
   NULL. */
static void
doRequested( const char *arg )
{
    Str serv;
    
    if ( ! arg || ! strcmp( arg, "all" ) )
    {
        Cfg_beginServEnum();   
        while ( Cfg_nextServ( serv ) )
            listRequested1( serv );
    }   
    else
        listRequested1( arg );
}


static void
doDb( void )
{
    const char *msgId;

    if ( ! Db_first( &msgId ) )
        fprintf( stderr, "Database empty.\n" );
    else
        do
            printf( "%s\n", msgId );
        while ( Db_next( &msgId ) );
}

static void
doFetch( void )
{
    Str serv;

    Cfg_beginServEnum();
    while ( Cfg_nextServ( serv ) )
        if ( Fetch_init( serv ) )
        {
            Fetch_postArts();

            Fetch_getNewGrps();

            /* Get overviews of new articles and store IDs of new articles
               that are to be fetched becase of FULL or THREAD mode in the
               request database. */
            Fetch_updateGrps();         

            /* get requested articles */
            Fetch_getReq_();

            Fetch_close();
        }
}

static void
doQuery( void )
{
    Str serv;

    Cfg_beginServEnum();
    while ( Cfg_nextServ( serv ) )
        if ( Fetch_init( serv ) )
        {
            if ( noffle.queryGrps )
                Client_getGrps();
            if ( noffle.queryDsc )
                Client_getDsc();
            Fetch_close();
        }
}

/* Expire all overviews not in database */
static void
expireContents( void )
{
    const Over *ov;
    int i;
    int cntDel, cntLeft;
    Str grp;
    Bool autoUnsubscribe;
    int autoUnsubscribeDays;
    time_t now = time( NULL ), maxAge = 0;
    const char *msgId;

    autoUnsubscribe = Cfg_autoUnsubscribe();
    autoUnsubscribeDays = Cfg_autoUnsubscribeDays();
    maxAge = Cfg_autoUnsubscribeDays() * 24 * 3600;
    if ( ! Cont_firstGrp( grp ) )
        return;
    Log_inf( "Expiring overviews not in database" );
    Fetchlist_read();
    do
    {
        if ( ! Grp_exists( grp ) )
            Log_err( "Overview file for unknown group %s exists", grp );
        else
        {
            cntDel = cntLeft = 0;
            Cont_read( grp );
            for ( i = Cont_first(); i <= Cont_last(); ++i )
                if ( ( ov = Cont_get( i ) ) )
                {
                    msgId = Ov_msgId( ov );
                    if ( ! Db_contains( msgId ) )
                    {
                        Cont_delete( i );
                        ++cntDel;
                    }
                    else
                        ++cntLeft;
                }
            if ( ! Grp_local( grp )
                 && Fetchlist_contains( grp )
                 && autoUnsubscribe
                 && difftime( now, Grp_lastAccess( grp ) ) > maxAge )
            {
                Log_ntc( "Auto-unsubscribing from %s after %d "
                         "days without access",
                         grp, autoUnsubscribeDays );
                Pseudo_autoUnsubscribed( grp, autoUnsubscribeDays );
                Fetchlist_remove( grp );
            }
            Cont_write();
            Grp_setFirstLast( grp, Cont_first(), Cont_last() );
            Log_inf( "%ld overviews deleted from group %s, %ld left (%ld-%ld)",
                     cntDel, grp, cntLeft, Grp_first( grp ), Grp_last( grp ) );
        }
    }
    while ( Cont_nextGrp( grp ) );
    Fetchlist_write();
}

static void
doExpire( void )
{
    Db_close();
    Db_expire();
    if ( ! Db_open() )
        return;
    expireContents();
}

static void
doCreateLocalGroup( const char * name )
{
    Str grp;

    Utl_cpyStr( grp, name );
    Utl_toLower( grp );
    name = Utl_stripWhiteSpace( grp );
    
    if ( Grp_exists( name ) )
        fprintf( stderr, "'%s' already exists.\n", name );
    else
    {
        Log_inf( "Creating new local group '%s'", name );
        Grp_create( name );
        Grp_setLocal( name );
	printf( "New local group '%s' created.\n", name );
	
	snprintf( grp, MAXCHAR, "%s/groupinfo.lastupdate", Cfg_spoolDir() );
	Utl_stamp( grp );
    }
}

static void
doDeleteLocalGroup( const char * name )
{
    Str grp;

    Utl_cpyStr( grp, name );
    Utl_toLower( grp );
    name = Utl_stripWhiteSpace( grp );
    
    if ( ! Grp_exists( name ) )
        fprintf( stderr, "'%s' does not exist.\n", name );
    else
    {
	int i;
	
        Log_inf( "Deleting group '%s'", name );

	/*
	  Delete all articles that are only in the group. Check the
	  article Xref for more than one group.
	 */
	Cont_read( name );
	for ( i = Cont_first(); i <= Cont_last(); i++ )
	{
	    const Over *over;
	    Bool toDelete;
	    Str msgId;

	    over = Cont_get( i );
	    toDelete = TRUE;
	    if ( over != NULL )
	    {
		ItemList * xref;

		Utl_cpyStr( msgId, Ov_msgId( over ) );
		xref = new_Itl( Db_xref( msgId ), " " );
		if ( Itl_count( xref ) > 1 )
		    toDelete = FALSE;
		del_Itl( xref );
	    }
	    Cont_delete( i );
	    if ( toDelete )
		Db_delete( msgId );
	}
	Cont_write();
	Grp_delete( name );
	printf( "Group '%s' deleted.\n", name );
    }
}

static void
doList( void )
{
    FetchMode mode;
    int i, size;
    const char *name, *modeStr = "";

    Fetchlist_read();
    size = Fetchlist_size();
    if ( size == 0 )
        fprintf( stderr, "Fetch list is empty.\n" );
    else
        for ( i = 0; i < size; ++i )
        {
            Fetchlist_element( &name, &mode, i );
            switch ( mode )
            {
            case FULL:
                modeStr = "full"; break;
            case THREAD:
                modeStr = "thread"; break;
            case OVER:
                modeStr = "over"; break;
            }
            printf( "%s %s %s\n", name, Grp_server( name ), modeStr );
        }
}

/* A modify command. argc/argv start AFTER '-m'. */
static Bool
doModify( const char *cmd, int argc, char **argv )
{
    const char *grp;

    if ( argc < 2 )
    {
	fprintf( stderr, "Insufficient arguments to -m\n" );
	return FALSE;
	
    }
    else if ( strcmp( cmd, "desc" ) != 0
	      && strcmp( cmd, "post" ) != 0 )
    {
	fprintf( stderr, "Unknown argument -m %s\n", optarg );
	return FALSE;
    }
    
    grp = argv[ 0 ];
    argv++;
    argc--;
    
    if ( strcmp( cmd, "desc" ) == 0 )
    {
	Str desc;

	Utl_cpyStr( desc, *( argv++ ) );
	while ( --argc > 0 )
	{
	    Utl_catStr( desc, " " );
	    Utl_catStr( desc, *( argv++ ) );
	}

	Grp_setDsc( grp, desc );
    }
    else
    {
	char c;

	if ( ! Grp_local( grp ) )
	{
	    fprintf( stderr, "%s is not a local group\n", grp );
	    return FALSE;
	}
	
	c = **argv;
	if ( c == 'y' || c == 'm' || c == 'n' )
	    Grp_setPostAllow( grp, c );
	else
	{
	    fprintf( stderr, "Access must be 'y', 'n' or 'm'" );
	    return FALSE;
	}
    }

    return TRUE;
}

static void
doGrps( void )
{
    const char *g;
    Str dateLastAccess, dateCreated;
    time_t lastAccess, created;

    if ( Grp_firstGrp( &g ) )
        do
        {
            lastAccess = Grp_lastAccess( g );
            created = Grp_created( g );
            ASSERT( lastAccess >= 0 );
            ASSERT( created >= 0 );
            strftime( dateLastAccess, MAXCHAR, "%Y-%m-%d %H:%M:%S",
                      localtime( &lastAccess ) );
            strftime( dateCreated, MAXCHAR, "%Y-%m-%d %H:%M:%S",
                      localtime( &created ) );
            printf( "%s\t%s\t%i\t%i\t%i\t%c\t%s\t%s\t%s\n",
                    g, Grp_server( g ), Grp_first( g ), Grp_last( g ),
                    Grp_rmtNext( g ), Grp_postAllow( g ), dateCreated,
                    dateLastAccess, Grp_dsc( g ) );
        }
        while ( Grp_nextGrp( &g ) );
}

static Bool
doSubscribe( const char *name, FetchMode mode )
{
    if ( ! Grp_exists( name ) )
    {
        fprintf( stderr, "%s is not available at remote servers.\n", name );
        return FALSE;
    }
    Fetchlist_read();
    if ( Fetchlist_add( name, mode ) )
        printf( "Adding %s to fetch list in %s mode.\n",
                name, mode == FULL ? "full" : mode == THREAD ?
                "thread" : "overview" );
    else
        printf( "%s is already in fetch list. Mode is now: %s.\n",
                name, mode == FULL ? "full" : mode == THREAD ?
                "thread" : "overview" );
    if ( ! Fetchlist_write() )
        fprintf( stderr, "Could not save fetchlist.\n" );
    Grp_setLastAccess( name, time( NULL ) );
    return TRUE;
}

static void
doUnsubscribe( const char *name )
{
    Fetchlist_read();
    if ( ! Fetchlist_remove( name ) )
        printf( "%s is not in fetch list.\n", name );
    else
        printf( "%s removed from fetch list.\n", name );
    if ( ! Fetchlist_write() )
        fprintf( stderr, "Could not save fetchlist.\n" );
}

static void
printUsage( void )
{
    static const char *msg =
      "Usage: noffle <option>\n"
      "Option is one of the following:\n"
      " -a | --article <msg id>|all      Show article(s) in database\n"
      " -c | --cancel <msg id>           Remove article from database\n"
      " -C | --create <grp>              Create a local group\n"
      " -d | --database                  Show content of article database\n"
      " -D | --delete <grp>              Delete a group\n"
      " -e | --expire                    Expire articles\n"
      " -f | --fetch                     Get newsfeed from server/post articles\n"
      " -g | --groups                    Show all groups available at server\n"
      " -h | --help                      Show this text\n"
      " -l | --list                      List groups on fetch list\n"
      " -m | --modify desc <grp> <desc>  Modify a group description\n"
      " -m | --modify post <grp> (y|n)   Modify posting status of a local group\n"
      " -n | --online                    Switch to online mode\n"
      " -o | --offline                   Switch to offline mode\n"
      " -q | --query groups              Get group list from server\n"
      " -q | --query desc                Get group descriptions from server\n"
      " -r | --server                    Run as server on stdin/stdout\n"
      " -R | --requested                 List articles marked for download\n"
      " -s | --subscribe-over <grp>      Add group to fetch list (overview)\n"
      " -S | --subscribe-full <grp>      Add group to fetch list (full)\n"
      " -t | --subscribe-thread <grp>    Add group to fetch list (thread)\n"
      " -u | --unsubscribe <grp>         Remove group from fetch list\n"
      " -v | --version                   Print version\n";
    fprintf( stderr, "%s", msg );
}

/*
  Allow core files: Change core limit and change working directory
  to spool directory, where news has write permissions.
*/
static void
enableCorefiles( void )
{
    struct rlimit lim;

    if ( getrlimit( RLIMIT_CORE, &lim ) != 0 )
    {
        Log_err( "Cannot get system core limit: %s", strerror( errno ) );
        return;
    }
    lim.rlim_cur = lim.rlim_max;
    if ( setrlimit( RLIMIT_CORE, &lim ) != 0 )
    {
        Log_err( "Cannot set system core limit: %s", strerror( errno ) );
        return;
    }
    Log_dbg( "Core limit set to %i", lim.rlim_max );
    if ( chdir( Cfg_spoolDir() ) != 0 )
    {
         Log_err( "Cannot change to directory '%s'", Cfg_spoolDir() );
         return;
    }
    Log_dbg( "Changed to directory '%s'", Cfg_spoolDir() );
}

static Bool
initNoffle( Bool interactive )
{
    Log_init( "noffle", interactive, LOG_NEWS );
    Cfg_read();
    Log_dbg( "NOFFLE version %s", Cfg_version() );
    noffle.interactive = interactive;
    if ( interactive )
        if ( ! Lock_openDatabases() )
            return FALSE;
    enableCorefiles();
    return TRUE;
}

static void
closeNoffle( void )
{
    if ( noffle.interactive )
      Lock_closeDatabases();
}

static RETSIGTYPE
bugReport( int sig )
{
    Log_err( "Received SIGSEGV. Please submit a bug report" );
    signal( SIGSEGV, SIG_DFL );
    raise( sig );
}

static RETSIGTYPE
logSignal( int sig )
{
    const char *name;
    Bool err = TRUE;

    switch ( sig )
    {
    case SIGABRT:
        name = "SIGABRT"; break;
    case SIGFPE:
        name = "SIGFPE"; break;
    case SIGILL:
        name = "SIGILL"; break;
    case SIGINT:
        name = "SIGINT"; break;
    case SIGTERM:
        name = "SIGTERM"; break;
    case SIGPIPE:
        name = "SIGPIPE"; err = FALSE; break;
    default:
        name = "?"; break;
    }
    if ( err )
        Log_err( "Received signal %i (%s). Aborting.", sig, name );
    else
        Log_inf( "Received signal %i (%s). Aborting.", sig, name );
    signal( sig, SIG_DFL );
    raise( sig );
}

int main ( int argc, char **argv )
{
    int c, result;
    struct option longOptions[] =
    {
        { "article",          required_argument, NULL, 'a' },
        { "cancel",           required_argument, NULL, 'c' },
        { "create",           required_argument, NULL, 'C' },
        { "database",         no_argument,       NULL, 'd' },
        { "delete",           required_argument, NULL, 'D' },
        { "expire",           no_argument,       NULL, 'e' },
        { "fetch",            no_argument,       NULL, 'f' },
        { "groups",           no_argument,       NULL, 'g' },
        { "help",             no_argument,       NULL, 'h' },
        { "list",             no_argument,       NULL, 'l' },
        { "modify",           required_argument, NULL, 'm' },
        { "offline",          no_argument,       NULL, 'o' },
        { "online",           no_argument,       NULL, 'n' },
        { "query",            required_argument, NULL, 'q' },
        { "server",           no_argument,       NULL, 'r' },
        { "requested",        no_argument,       NULL, 'R' },
        { "subscribe-over",   required_argument, NULL, 's' },
        { "subscribe-full",   required_argument, NULL, 'S' },
        { "subscribe-thread", required_argument, NULL, 't' },
        { "unsubscribe",      required_argument, NULL, 'u' },
        { "version",          no_argument,       NULL, 'v' },
        { NULL, 0, NULL, 0 }
    };
    
    signal( SIGSEGV, bugReport );
    signal( SIGABRT, logSignal );
    signal( SIGFPE, logSignal );
    signal( SIGILL, logSignal );
    signal( SIGINT, logSignal );
    signal( SIGTERM, logSignal );
    signal( SIGPIPE, logSignal );
    c = getopt_long( argc, argv, "a:c:C:dD:efghlm:onq:rRs:S:t:u:v",
                     longOptions, NULL );
    if ( ! initNoffle( c != 'r' ) )
        return EXIT_FAILURE;
    result = EXIT_SUCCESS;
    switch ( c )
    {
    case 0:
        /* Options that set a flag. */
        break;
    case 'a':
        if ( ! optarg )
        {
            fprintf( stderr, "Option -a needs argument.\n" );
            result = EXIT_FAILURE;
        }
        else
            doArt( optarg );
        break;
    case 'c':
        if ( ! optarg )
        {
            fprintf( stderr, "Option -c needs argument.\n" );
            result = EXIT_FAILURE;
        }
        else
            doCancel( optarg );
        break;
    case 'C':
        if ( ! optarg )
        {
            fprintf( stderr, "Option -C needs argument.\n" );
            result = EXIT_FAILURE;
        }
        else
            doCreateLocalGroup( optarg );
        break;
    case 'd':
        doDb();
        break;
    case 'D':
        if ( ! optarg )
        {
            fprintf( stderr, "Option -D needs argument.\n" );
            result = EXIT_FAILURE;
        }
        else
            doDeleteLocalGroup( optarg );
        break;
    case 'e':
	doExpire();
	break;
    case 'f':
        doFetch();
        break;
    case 'g':
        doGrps();
        break;
    case -1:
    case 'h':
        printUsage();
        break;
    case 'l':
        doList();
        break;
    case 'm':
        if ( ! optarg )
        {
            fprintf( stderr, "Option -m needs argument.\n" );
            result = EXIT_FAILURE;
        }
        else
	    if ( ! doModify( optarg, argc - optind, &argv[ optind ] ) )
		result = EXIT_FAILURE;
        break;
    case 'n':
        if ( Online_true() )
            fprintf( stderr, "NOFFLE is already online\n" );
        else
            Online_set( TRUE );
        break;
    case 'o':
        if ( ! Online_true() )
            fprintf( stderr, "NOFFLE is already offline\n" );
        else
            Online_set( FALSE );
        break;
    case 'q':
        if ( ! optarg )
        {
            fprintf( stderr, "Option -q needs argument.\n" );
            result = EXIT_FAILURE;
        }
        else
        {
            if ( strcmp( optarg, "groups" ) == 0 )
                noffle.queryGrps = TRUE;
            else if ( strcmp( optarg, "desc" ) == 0 )
                noffle.queryDsc = TRUE;
            else
            {
                fprintf( stderr, "Unknown argument -q %s\n", optarg );
                result = EXIT_FAILURE;
            }
            doQuery();
        }
        break;
    case 'r':
        Log_inf( "Starting as server" );
        Server_run();
        break;
    case 'R':
        doRequested( optarg );
        break;
    case 's':
        if ( ! optarg )
        {
            fprintf( stderr, "Option -s needs argument.\n" );
            result = EXIT_FAILURE;
        }
        else
            result = doSubscribe( optarg, OVER );
        break;
    case 'S':
        if ( ! optarg )
        {
            fprintf( stderr, "Option -S needs argument.\n" );
            result = EXIT_FAILURE;
        }
        else
            doSubscribe( optarg, FULL );
        break;
    case 't':
        if ( ! optarg )
        {
            fprintf( stderr, "Option -t needs argument.\n" );
            result = EXIT_FAILURE;
        }
        else
            result = doSubscribe( optarg, THREAD );
        break;
    case 'u':
        if ( ! optarg )
        {
            fprintf( stderr, "Option -u needs argument.\n" );
            result = EXIT_FAILURE;
        }
        else
            doUnsubscribe( optarg );
        break;
    case '?':
        /* Error message already printed by getopt_long */
        result = EXIT_FAILURE;
        break;
    case 'v':
        printf( "NNTP server NOFFLE, version %s.\n", Cfg_version() );
        break;
    default:
        abort(); /* Never reached */
    }
    closeNoffle();
    return result;
}
