/*
  pseudo.c
  
  $Id: pseudo.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
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

#include "pseudo.h"

#include <stdio.h>
#include "common.h"
#include "configfile.h"
#include "content.h"
#include "database.h"
#include "group.h"
#include "log.h"
#include "protocol.h"
#include "util.h"
#include "pseudo.h"
#include "portable.h"

static Over *
genOv( const char *rawSubj, const char *rawBody, const char *suffix )
{
    size_t bytes, lines;
    time_t t;
    Str subj, date, msgId;

    snprintf( subj, MAXCHAR, "[ %s ]", rawSubj );
    time( &t );
    Utl_rfc822Date( t, date );
    Prt_genMsgId( msgId, "", suffix );
    bytes = lines = 0;
    while ( *rawBody )
    {
        ++bytes;
        if ( *rawBody == '\n' )
            ++lines;
        ++rawBody;
    }
    return new_Over( subj, "news (\"[ NOFFLE ]\")" , date, msgId, "",
                     bytes, lines );
}

void
Pseudo_appGeneralInfo()
{
    Cont_app( genOv( "General info", Pseudo_generalInfoBody(),
                     "NOFFLE-GENERAL-INFO" ) );
}

Bool
Pseudo_isGeneralInfo( const char *msgId )
{
    return ( strstr( msgId, "NOFFLE-GENERAL-INFO" ) != NULL );
}

const char *
Pseudo_generalInfoHead()
{
    static Str s;

    Over *ov;

    ov = genOv( "General info", Pseudo_generalInfoBody(),
                "NOFFLE-GENERAL-INFO" );
    if ( ov )
    {
        snprintf( s, MAXCHAR,
                  "Message-ID: %s\n"
                  "Subject: %s\n"
                  "From: %s\n"
                  "Date: %s\n"
                  "Bytes: %u\n"
                  "Lines: %u\n",
                  Ov_msgId( ov ),
                  Ov_subj( ov ),
                  Ov_from( ov ),
                  Ov_date( ov ),
                  Ov_bytes( ov ),
                  Ov_lines( ov ) );
        del_Over( ov );
        return s;
    }
    return NULL;
}

const char *
Pseudo_generalInfoBody( void )
{
    if ( Cfg_autoSubscribe() )
        return
            "\n"
            "\t[ NOFFLE INFO: General information ]\n"
            "\n"
            "\t[ This server is running NOFFLE, which is a NNTP server ]\n"
            "\t[ optimized for low speed dial-up Internet connections. ]\n"
            "\n"
            "\t[ By reading this or any other article of this group, ]\n"
            "\t[ NOFFLE has put it on its fetch list and will retrieve ]\n"
            "\t[ articles next time it is online. ]\n"
            "\n"
            "\t[ If you have more questions about NOFFLE please talk ]\n"
            "\t[ to your newsmaster or read the manual page for ]\n"
            "\t[ \"noffle\". ]\n";
    else
        return
            "\n"
            "\t[ NOFFLE INFO: General information ]\n"
            "\n"
            "\t[ This server is running NOFFLE, which is a NNTP server ]\n"
            "\t[ optimized for low speed dial-up Internet connections. ]\n"
            "\n"
            "\t[ This group is presently not on the fetch list. You can ]\n"
            "\t[ put groups on the fetch list by running the \"noffle\" ]\n"
            "\t[ command on the computer where this server is running. ]\n"
            "\n"
            "\t[ If you have more questions about NOFFLE please talk ]\n"
            "\t[ to your newsmaster or read the manual page for ]\n"
            "\t[ \"noffle\". ]\n";
}

const char *
Pseudo_markedBody( void )
{
    return
        "\n"
        "\t[ NOFFLE INFO: Marked for download ]\n"
        "\n"
        "\t[ The body of this article has been marked for download. ]\n";
}

const char *
Pseudo_alreadyMarkedBody( void )
{
    return
        "\n"
        "\t[ NOFFLE INFO: Already marked for download ]\n"
        "\n"
        "\t[ The body of this article has already been marked ]\n"
        "\t[ for download. ]\n";
}

const char *
Pseudo_markingFailedBody( void )
{
    return
        "\n"
        "\t[ NOFFLE ERROR: Marking for download failed ]\n"
        "\n"
        "\t[ Sorry, I could not mark this article for download. ]\n"
        "\t[ Either the database is corrupted, or I was unable to ]\n"
        "\t[ get write access to the request directory. ]\n"
        "\t[ Please contact your newsmaster to remove this problem. ]\n";
}

static void
genPseudo( const char *rawSubj, const char* rawBody )
{
    Over *ov;
    DynStr *body = 0, *artTxt = 0;

    body = new_DynStr( 10000 );
    artTxt = new_DynStr( 10000 );
    DynStr_app( body, "\n\t[ NOFFLE INFO: " );
    DynStr_app( body, rawSubj );
    DynStr_app( body, " ]\n\n" );
    DynStr_app( body, "\t[ " );
    while( *rawBody )
    {
        if ( *rawBody == '\n' )
        {
            DynStr_app( body, " ]\n" );
            if ( *( rawBody + 1 ) == '\n' )
            {
                DynStr_app( body, "\n\t[ " );
                ++rawBody;
            }
            else if ( *( rawBody + 1 ) != '\0' )
                DynStr_app( body, "\t[ " );
        }
        else
            DynStr_appN( body, rawBody, 1 );
        ++rawBody;
    }
    DynStr_appLn( body, "" );    
    DynStr_appLn( artTxt,
                  "Comments: Pseudo article generated by news server NOFFLE" );
    DynStr_appLn( artTxt, "" );
    DynStr_appDynStr( artTxt, body );
    ov = genOv( rawSubj, DynStr_str( body ), "PSEUDO" );
    if ( body && artTxt && ov )
    {
        Cont_app( ov );
        if ( Db_prepareEntry( ov, Cont_grp(), Cont_last() ) )
            Db_storeArt( Ov_msgId( ov ), DynStr_str( artTxt ) );
        Cont_write();
        Grp_setFirstLast( Cont_grp(), Cont_first(), Cont_last() );
    }
    del_DynStr( body );
    del_DynStr( artTxt );
}

void
Pseudo_retrievingFailed( const char *msgId, const char *reason )
{
    DynStr *artTxt = 0;

    if ( ! Db_contains( msgId ) )
    {
        Log_err( "Article %s has no entry in database %s", msgId );
        return;
    }
    artTxt = new_DynStr( 10000 );
    DynStr_appLn( artTxt,
                  "Comments: Pseudo body generated by news server NOFFLE" );
    DynStr_appLn( artTxt, "" );
    DynStr_app( artTxt,
                "\n"
                "\t[ NOFFLE ERROR: Retrieving failed ]\n"
                "\n"
                "\t[ This article could not be retrieved. Maybe ]\n"
                "\t[ it has already expired at the remote server ]\n"
                "\t[ or it has been cancelled by its sender. See ]\n"
                "\t[ the appended status line of the remote ]\n"
                "\t[ server for more information. ]\n"
                "\n"
                "\t[ This message will disappear the next time ]\n"
                "\t[ someone tries to read this article, so that ]\n"
                "\t[ it can be marked for download again. ]\n" );
    DynStr_app( artTxt, "\n\t[ Remote server status: " );
    DynStr_app( artTxt, reason );
    DynStr_app( artTxt, " ]\n" );
    Db_storeArt( msgId, DynStr_str( artTxt ) );
    del_DynStr( artTxt );
}

void
Pseudo_cntInconsistent( const char *grp, int first, int last, int next )
{
    DynStr *info;
    Str s;

    info = new_DynStr( 10000 );
    if ( info )
    {
        DynStr_app( info,
                    "This group's article counter is not \n"
                    "consistent Probably the remote news server\n"
                    "was changed or has reset its article counter\n"
                    "for this group. As a consequence there could\n"
                    "be some articles be duplicated in this group\n" );
        snprintf( s, MAXCHAR, "Group: %s", grp );
        DynStr_appLn( info, s );
        snprintf( s, MAXCHAR, "Remote first article number: %i", first );
        DynStr_appLn( info, s );
        snprintf( s, MAXCHAR, "Remote last article number: %i", last );
        DynStr_appLn( info, s );
        snprintf( s, MAXCHAR, "Remote next article number: %i", next );
        DynStr_appLn( info, s );
        genPseudo( "Article counter inconsistent", DynStr_str( info ) );
    }
    del_DynStr( info );
}

void
Pseudo_missArts( const char *grp, int first, int next )
{
    DynStr *info;
    Str s;

    info = new_DynStr( 5000 );
    if ( info )
    {
        DynStr_app( info,
                    "Some articles could not be retrieved from\n"
                    "the remote server, because it had already\n"
                    "deleted them.\n"
                    "If this group is on the fetch list, then\n"
                    "contact your newsmaster to ensure that\n"
                    "\"noffle\" is fetching news more frequently.\n" );
        snprintf( s, MAXCHAR, "Group: %s", grp );
        DynStr_appLn( info, s );
        snprintf( s, MAXCHAR, "Remote next article number: %i", next );
        DynStr_appLn( info, s );
        snprintf( s, MAXCHAR, "Remote first article number: %i", first );
        DynStr_appLn( info, s );
        genPseudo( "Missing articles", DynStr_str( info ) );
        del_DynStr( info );
    }
}

void
Pseudo_autoUnsubscribed( const char *grp, int days )
{
    DynStr *info;
    Str s;

    info = new_DynStr( 10000 );
    if ( info )
    {
        DynStr_app( info,
                    "NOFFLE has automatically unsubscribed this\n"
                    "group since it has not been accessed for\n"
                    "some time.\n"
                    "Re-subscribing is done either automatically\n"
                    "by NOFFLE (if configured) or by manually\n"
                    "running the 'noffle --subscribe' command\n" );
        snprintf( s, MAXCHAR, "Group: %s", grp );
        DynStr_appLn( info, s );
        snprintf( s, MAXCHAR, "Days without access: %i", days );
        DynStr_appLn( info, s );
        genPseudo( "Auto unsubscribed", DynStr_str( info ) );
    }
    del_DynStr( info );
}

void
Pseudo_autoSubscribed()
{
    DynStr *info;

    info = new_DynStr( 10000 );
    if ( info )
    {
        DynStr_app( info,
                    "NOFFLE has now automatically subscribed to\n"
                    "this group. It will fetch articles next time\n"
                    "it is online.\n" );
        genPseudo( "Auto subscribed", DynStr_str( info ) );
    }
    del_DynStr( info );
}
