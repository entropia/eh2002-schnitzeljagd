/*
  fetch.c

  $Id: fetch.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "fetch.h"
#include <errno.h>

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

#include <signal.h>
#include "client.h"
#include "configfile.h"
#include "content.h"
#include "dynamicstring.h"
#include "fetchlist.h"
#include "request.h"
#include "group.h"
#include "log.h"
#include "outgoing.h"
#include "protocol.h"
#include "pseudo.h"
#include "util.h"
#include "portable.h"

struct Fetch
{
    Bool ready;
    Str serv;
} fetch = { FALSE, "" };

static Bool
connectToServ( const char *name )
{
    Log_inf( "Fetch from '%s'", name );
    if ( ! Client_connect( name ) )
    {
        Log_err( "Could not connect to %s", name );
        return FALSE;
    }
    return TRUE;
}

void
Fetch_getNewGrps( void )
{
    time_t t;
    Str file;

    ASSERT( fetch.ready );
    snprintf( file, MAXCHAR, "%s/lastupdate.%s",
	      Cfg_spoolDir(), fetch.serv );
    if ( ! Utl_getStamp( &t, file ) )
    {
        Log_err( "Cannot read %s. Please run noffle --query groups", file );
        return;
    }
    Log_inf( "Updating groupinfo" );
    Client_getNewgrps( &t );
}

void
Fetch_getNewArts( const char *name, FetchMode mode )
{
    int next, first, last, oldLast;

    if ( ! Client_changeToGrp( name ) )
    {
        Log_err( "Could not change to group %s", name );
        return;
    }
    Cont_read( name );
    Client_rmtFirstLast( &first, &last );
    next = Grp_rmtNext( name );
    oldLast = Cont_last();
    if ( next == last + 1 )
    {
        Log_inf( "No new articles in %s", name );
        Cont_write();
        Grp_setFirstLast( name, Cont_first(), Cont_last() );
        return;
    }
    if ( first == 0 && last == 0 )
    {
        Log_inf( "No articles in %s", name );
        Cont_write();
        Grp_setFirstLast( name, Cont_first(), Cont_last() );
        return;
    }
    if ( next > last + 1 )
    {
        Log_err( "Article number inconsistent (%s rmt=%lu-%lu, next=%lu)",
                 name, first, last, next );
        Pseudo_cntInconsistent( name, first, last, next );
    }
    else if ( next < first )
    {
        Log_inf( "Missing articles (%s first=%lu next=%lu)",
                 name, first, next );
        Pseudo_missArts( name, first, next );
    }
    else
        first = next;
    if ( last - first > Cfg_maxFetch() )
    {
        Log_ntc( "Cutting number of overviews to %lu", Cfg_maxFetch() );
        first = last - Cfg_maxFetch() + 1;
    }
    Log_inf( "Getting remote overviews %lu-%lu for group %s",
             first, last, name );
    Client_getOver( first, last, mode );
    Cont_write();
    Grp_setFirstLast( name, Cont_first(), Cont_last() );
}

void
Fetch_updateGrps( void )
{
    FetchMode mode;
    int i, size;
    const char* name;

    ASSERT( fetch.ready );
    Fetchlist_read();
    size = Fetchlist_size();
    for ( i = 0; i < size; ++i )
    {
        Fetchlist_element( &name, &mode, i );
        if ( strcmp( Grp_server( name ), fetch.serv ) == 0 )
            Fetch_getNewArts( name, mode );
    }
}

void
Fetch_getReq_( void )
{
    Str msgId;
    DynStr *list;
    const char *p;
    int count = 0;

    ASSERT( fetch.ready );
    Log_dbg( "Retrieving articles marked for download" );
    list = new_DynStr( 10000 );
    if ( Req_first( fetch.serv, msgId ) )
        do
        {
            DynStr_appLn( list, msgId );
            if ( ++count % 20 == 0 ) /* Send max. 20 ARTICLE cmds at once */
            {
                p = DynStr_str( list );
                Client_retrieveArtList( p );
                while ( ( p = Utl_getLn( msgId, p ) ) )
                    Req_remove( fetch.serv, msgId );
                DynStr_clear( list );
            }
        }
        while ( Req_next( msgId ) );
    p = DynStr_str( list );
    Client_retrieveArtList( p );
    while ( ( p = Utl_getLn( msgId, p ) ) )
        Req_remove( fetch.serv, msgId );
    del_DynStr( list );
}

static void
returnArticleToSender( const char *sender, const char *reason,
                       const char *article )
{
    /* FW: Sowas brauchen wir nicht */
    return;
    /*
    int ret;
    Str cmd;
    FILE *f;
    sig_t lastHandler;

    Log_err( "Return article to '%s' by mail", sender );
    snprintf( cmd, MAXCHAR, "%s -t -oi", SENDMAILPROG);
    lastHandler = signal( SIGPIPE, SIG_IGN );
    f = popen( cmd, "w" );
    if ( f == NULL )
        Log_err( "Invocation of '%s' failed (%s)", cmd, strerror( errno ) );
    else
    {
        fprintf( f,
                 "To: %s\n"
                 "Subject: [ NOFFLE: Posting failed ]\n"
                 "\n"
                 "\t[ NOFFLE: POSTING OF ARTICLE FAILED ]\n"
                 "\n"
                 "\t[ The posting of your article failed. ]\n"
                 "\t[ Reason of failure at remote server: ]\n"
                 "\n"
                 "\t[ %s ]\n"
                 "\n"
                 "\t[ Full article text has been appended. ]\n"
                 "\n"
                 "%s"
                 ".\n",
                 sender,
                 reason, article );
        ret = pclose( f );
        if ( ret != EXIT_SUCCESS )
            Log_err( "'%s' exit value %d", cmd, ret );
        signal( SIGPIPE, lastHandler );
    }
    */
}

void
Fetch_postArts( void )
{
    DynStr *s;
    Str msgId, errStr, sender;
    const char *txt;

    s = new_DynStr( 10000 );
    if ( Out_first( fetch.serv, msgId, s ) )
    {
        Log_inf( "Posting articles" );
        do
        {
            txt = DynStr_str( s );
            Out_remove( fetch.serv, msgId );
            if ( ! Client_postArt( msgId, txt, errStr ) )
            {
                Utl_cpyStr( sender, Cfg_mailTo() );
                if ( strcmp( sender, "" ) == 0
                     && ! Prt_searchHeader( txt, "SENDER", sender )
                     && ! Prt_searchHeader( txt, "X-NOFFLE-X-SENDER",
                                            sender ) /* see server.c */
                     && ! Prt_searchHeader( txt, "FROM", sender ) )
                    Log_err( "Article %s has no From/Sender/X-Sender field",
                             msgId );
                else
                    returnArticleToSender( sender, errStr, txt );
            }
        }
        while ( Out_next( msgId, s ) );
    }
    del_DynStr( s );
}

Bool
Fetch_init( const char *serv )
{
    if ( ! connectToServ( serv ) )
        return FALSE;
    Utl_cpyStr( fetch.serv, serv );
    fetch.ready = TRUE;
    return TRUE;
}

void
Fetch_close()
{
    Client_disconnect();
    fetch.ready = FALSE;
    Log_inf( "Fetch from '%s' finished", fetch.serv );
}
