/*
  protocol.c

  $Id: protocol.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "protocol.h"

#include <stdio.h> 
#include <ctype.h> 
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "common.h"
#include "dynamicstring.h"
#include "log.h"
#include "over.h"
#include "util.h"
#include "portable.h"

static void
readAlarm( int sig )
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

Bool
Prt_getLn( Str line, FILE *f, int timeoutSeconds )
{
    size_t len;
    char *ret;
    sig_t oldHandler = NULL;

    if ( timeoutSeconds >= 0 )
    {
        oldHandler = installSignalHandler( SIGALRM, readAlarm );
        if ( oldHandler == SIG_ERR )
        {
            Log_err( "Prt_getLn: signal failed." );
            return FALSE;
        }
        if ( alarm( timeoutSeconds ) != 0 )
            Log_err( "Prt_getLn: Alarm was already set." );
    }
    /*
      We also accept lines ending with "\n" instead of "\r\n", some
      clients wrongly send such lines.
    */
    ret = fgets( line, MAXCHAR, f );
    if ( timeoutSeconds >= 0 )
    {
        alarm( 0 );
        installSignalHandler( SIGALRM, oldHandler );
    }
    if ( ret == NULL )
        return FALSE;
    len = strlen( line );
    if ( len > 0 && line[ len - 1 ] == '\n' )
    {
        line[ len - 1 ] = '\0';
        if ( len > 1 && line[ len - 2 ] == '\r' )
            line[ len - 2 ] = '\0';
    }
    Log_dbg( "[R] %s", line );
    return TRUE;
}

Bool
Prt_getTxtLn( Str line, Bool *err, FILE *f, int timeoutSeconds )
{
    Str buf;

    if ( ! Prt_getLn( buf, f, timeoutSeconds ) )
    {
        Log_err( "Cannot get text line" );
        *err = TRUE;
        return FALSE;
    }
    *err = FALSE;
    if ( buf[ 0 ] == '.' )
    {
        if ( buf[ 1 ] == 0 )
            return FALSE;
        else
            strcpy( line, buf + 1 );
    }
    else
        strcpy( line, buf );
    return TRUE;
}

Bool
Prt_putTxtLn( const char* line, FILE *f )
{
    if ( line[ 0 ] == '.' )
    {
        Log_dbg( "[S] .%s", line );
        return ( fprintf( f, ".%s\r\n", line ) == (int)strlen( line ) + 3 );
    }
    else
    {
        Log_dbg( "[S] %s", line );
        return ( fprintf( f, "%s\r\n", line ) == (int)strlen( line ) + 2 );
    }
}

Bool
Prt_putEndOfTxt( FILE *f )
{
    Log_dbg( "[S] ." );
    return ( fprintf( f, ".\r\n" ) == 3 );
}

/*
  Write text buffer of lines each ending with '\n'.
  Replace '\n' by "\r\n".
*/
Bool
Prt_putTxtBuf( const char *buf, FILE *f )
{
    Str line;
    const char *pBuf;
    char *pLn;

    pBuf = buf;
    pLn = line;
    while ( *pBuf != '\0' )
    {
        if ( *pBuf == '\n' )
        {
            *pLn = '\0';
            if ( ! Prt_putTxtLn( line, f ) )
                return FALSE;
            pLn = line;
            ++pBuf;
        }
        else if ( pLn - line >= MAXCHAR - 1 )
        {
            /* Put it out raw to prevent String overflow */
            Log_err( "Writing VERY long line" );
            *pLn = '\0';
            if ( fprintf( f, "%s", line ) != (int)strlen( line ) )
                return FALSE;
            pLn = line;
        }   
        else
            *(pLn++) = *(pBuf++);
    }
    return TRUE;
}

Bool
Prt_getField( Str resultField, Str resultValue, const char* line )
{
    char *dst;
    const char *p;
    Str lineLower, t;
    
    Utl_cpyStr( lineLower, line );
    Utl_toLower( lineLower );
    p = Utl_stripWhiteSpace( lineLower );
    dst = resultField;
    while ( ! isspace( *p ) && *p != ':' && *p != '\0' )
        *(dst++) = *(p++);
    *dst = '\0';
    while ( isspace( *p ) )
        ++p;    
    if ( *p == ':' )
    {
        ++p;
        strcpy( t, line + ( p - lineLower ) );
        p = Utl_stripWhiteSpace( t );
        strcpy( resultValue, p );
        return TRUE;
    }
    return FALSE;
}

Bool
Prt_searchHeader( const char *artTxt, const char *which, Str result )
{
    const char *src, *p;
    char *dst;
    Str line, whichLower, field;
    int len;
    
    Utl_cpyStr( whichLower, which );
    Utl_toLower( whichLower );
    src = artTxt;
    while ( TRUE )
    {
        dst = line;
        len = 0;
        while ( *src != '\n' && len < MAXCHAR )
        {
            if ( *src == '\0' )
                return FALSE;
            *(dst++) = *(src++);
            ++len;
        }
        if ( *src == '\n' )
            ++src;
        *dst = '\0';
        p = Utl_stripWhiteSpace( line );
        if ( *p == '\0' )
            break;
        if ( Prt_getField( field, result, line )
             && strcmp( field, whichLower ) == 0 )
            return TRUE;
    }
    return FALSE;
}

static Bool
getFQDN( Str result )
{
    struct hostent *myHostEnt;
    struct utsname myName;
    
    if ( uname( &myName ) >= 0
         && ( myHostEnt = gethostbyname( myName.nodename ) ) )
    {
        Utl_cpyStr( result, myHostEnt->h_name );
        return TRUE;
    }
    return FALSE;
}

static void
getDomain( Str domain, const char *from )
{
    const char *addTopLevel, *p1, *p2, *p, *domainStart;
    Str myDomain;

    if ( getFQDN( myDomain ) )
    {
        p = strstr( myDomain, "." );
        if ( p != NULL )
            domainStart = p + 1;
        else
            domainStart = myDomain;
    }
    else /* Take domain of From field */
    {
        myDomain[ 0 ] = '\0';
        p1 = strstr( from, "@" );
        if ( p1 != NULL )
        {
            p2 = strstr( p1, ">" );
            if ( p2 != NULL )
                Utl_cpyStrN( myDomain, p1 + 1, p2 - p1 - 1 );
        }
        if ( myDomain[ 0 ] == '\0' )
            Utl_cpyStr( myDomain, "unknown" );
        domainStart = myDomain;
    }
    /*
      If domain contains no dot (and is probably invalid anyway),
      we add ".local", because some servers insist on domainnames with dot
      in message ID.
    */
    addTopLevel = strstr( domainStart, "." ) == NULL ? ".local" : "";
    snprintf( domain, MAXCHAR, "%s%s", myDomain, addTopLevel );    
}

/* See RFC 850, section 2.1.7 */
Bool
Prt_isValidMsgId( const char *msgId )
{
    Str head, domain;
    int len, headLen;
    const char *p;

    len = strlen( msgId );
    p = strstr( msgId, "@" );
    if ( msgId[ 0 ] != '<' || msgId[ len - 1 ] != '>' || p == NULL )
        return FALSE;
    strcpy( domain, p + 1 );
    domain[ strlen( domain ) - 1 ] = '\0';
    headLen = p - msgId - 1;
    Utl_cpyStrN( head, msgId + 1, headLen );
    head[ headLen ] = '\0';
    /*
      To do: check for special characters in head and domain (non-printable
      or '@', '<', '>'). Maybe compare domain with a config option 
      and replace it by the config option, if not equal.
     */
    if ( strstr( domain, "." ) == NULL )
        return FALSE;
    return TRUE;
}

void
Prt_genMsgId( Str msgId, const char *from, const char *suffix )
{
    Str domain, date;
    time_t t;
    static Bool randSeeded = FALSE;

    if ( ! randSeeded )
    {
	struct timeval tv;
	struct timezone tz;

	if ( gettimeofday( &tv, &tz ) == 0 )
	    srand( (unsigned int) tv.tv_usec );
	randSeeded = TRUE;
    }
    getDomain( domain, from );
    time( &t );
    strftime( date, MAXCHAR, "%Y%m%d%H%M%S", gmtime( &t ) );
    snprintf( msgId, MAXCHAR, "<%s.%X.%s@%s>", date, rand(), suffix, domain );
    ASSERT( Prt_isValidMsgId( msgId ) );
}
