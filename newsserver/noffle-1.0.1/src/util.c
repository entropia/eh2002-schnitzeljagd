/*
  util.c

  $Id: util.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
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

#include "util.h"
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "configfile.h"
#include "log.h"
#include "wildmat.h"
#include "portable.h"

static const char *
nextWhiteSpace( const char *p )
{
    while ( *p && ! isspace( *p ) )
        ++p;
    return p;
}

static const char *
nextNonWhiteSpace( const char *p )
{
    while ( *p && isspace( *p ) )
        ++p;
    return p;
}

const char *
Utl_restOfLn( const char *line, unsigned int token )
{
    unsigned int i;
    const char *p;

    p = line;
    for ( i = 0; i < token; ++i )
    {
        p = nextNonWhiteSpace( p );
        p = nextWhiteSpace( p );
    }
    p = nextNonWhiteSpace( p );
    return p;
}

const char *
Utl_getLn( Str result, const char *pos )
{
    int len = 0;
    const char *p = pos;

    if ( ! p )
        return NULL;
    while ( *p != '\n' )
    {
        if ( *p == '\0' )
        {
            if ( len > 0 )
                Log_err( "Line not terminated by newline: '%s'", pos );
            return NULL;
        }
        *(result++) = *(p++);
        ++len;
        if ( len >= MAXCHAR - 1 )
        {
            *result = '\0';
            Log_err( "Utl_getLn: line too long: %s", result );
            return ++p;
        }
    }
    *result = '\0';
    return ++p;

}

const char *
Utl_ungetLn( const char *str, const char *p )
{
    if ( str == p )
        return FALSE;
    --p;
    if ( *p != '\n' )
    {
        Log_dbg( "Utl_ungetLn: not at beginning of line" );
        return NULL;
    }
    --p;
    while ( TRUE )
    {
        if ( p == str )
            return p;
        if ( *p == '\n' )
            return p + 1;
        --p;
    }
}

const char *
Utl_getHeaderLn( Str result, const char *p )
{
    const char * res = Utl_getLn( result, p );

    /* Look for followon line if this isn't a blank line. */
    if ( res != NULL && result[ 0 ] != '\0' && ! isspace( result[ 0 ] ) )
	while ( res != NULL && res[ 0 ] != '\n' && isspace( res[ 0 ] ) )
	{
	    Str nextLine;
	    const char *here;
	    char *next;

	    here = res;
	    res = Utl_getLn( nextLine, res );
	    next = Utl_stripWhiteSpace( nextLine );

	    if ( next[ 0 ] != '\0' )
	    {
		Utl_catStr( result, " " );
		Utl_catStr( result, next );
	    }
	    else
	    {
		res = here;
		break;
	    }
	}

    return res;
}

void
Utl_toLower( Str line )
{
    char *p;

    p = line;
    while ( *p )
    {
        *p = tolower( *p );
        ++p;
    }
}

char *
Utl_stripWhiteSpace( char *line )
{
    char *p;

    while ( isspace( *line ) )
        ++line;
    p = line + strlen( line ) - 1;
    while ( isspace( *p ) )
    {
        *p = '\0';
        --p;
    }
    return line;
}

void
Utl_stripComment( char *line )
{
    for ( ; *line != '\0'; line++ )
	if ( *line =='#' )
	{
	    *line = '\0';
	    break;
	}
}

void
Utl_cpyStr( Str dst, const char *src )
{
    dst[ 0 ] = '\0';
    strncat( dst, src, MAXCHAR );
}

void
Utl_cpyStrN( Str dst, const char *src, int n )
{
    if ( n > MAXCHAR )
    	n = MAXCHAR;
    dst[ 0 ] = '\0';
    strncat( dst, src, (size_t)n );
}

void
Utl_catStr( Str dst, const char *src )
{
    strncat( dst, src, MAXCHAR - strlen( dst ) );
}

void
Utl_catStrN( Str dst, const char *src, int n )
{
    size_t un;

    ASSERT( n >= 0 );
    un = (size_t)n;
    if ( un > MAXCHAR - strlen( dst ) )
    	 un = MAXCHAR - strlen( dst );
    strncat( dst, src, un );
}

void
Utl_stamp( Str file )
{
    FILE *f;
    time_t t;

    time( &t );
    if ( ! ( f = fopen( file, "w" ) ) )
    {
        Log_err( "Could not open %s for writing (%s)",
                 file, strerror( errno ) );
        return;
    }
    fprintf( f, "%lu\n", t );
    fclose( f );
}

Bool
Utl_getStamp( time_t *result, Str file )
{
    FILE *f;

    if ( ! ( f = fopen( file, "r" ) ) )
        return FALSE;
    if ( fscanf( f, "%lu", result ) != 1 )
    {
        Log_err( "File %s corrupted", file );
        fclose( f );
        return FALSE;
    }
    fclose( f );
    return TRUE;
}

static const char *DOTW[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri",
			      "Sat", NULL };
static const char *MON[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
			     "Aug", "Sep", "Oct", "Nov", "Dec", NULL };

/*
 * Calculate the difference between local time and GMT. This is INN's
 * 'always-works' method. It assumes the time differences is < 24hrs.
 * Sounds reasonable to me. It also assumes it can ignore seconds.
 * Returns GMT - localtime minutes. It will also trash the localtime/
 * gmtime/etc. static buffer.
 */
static int
tzDiff( void )
{
    time_t now;
    struct tm local, gmt, *tm;
    static time_t nextCalc = 0;
    static int res = 0;

    now = time( NULL );
    if ( now < nextCalc )
	return res;
    
    tm = localtime( &now );
    if ( tm == NULL )
	return 0;
    local = *tm;
    tm = gmtime( &now );
    if ( tm == NULL )
	return 0;
    gmt = *tm;

    res = gmt.tm_yday - local.tm_yday;
    if ( res < -1 )
	res = -1;		/* Year rollover? */
    else if ( res > 1 )
	res = 1;

    res *= 24;
    res += gmt.tm_hour - local.tm_hour;
    res *= 60;
    res += gmt.tm_min - local.tm_min;

    /* Need to recalc at start of next hour */
    nextCalc = now + ( 60 - local.tm_sec ) + 60 * ( 59 - local.tm_min );
    
    return res;
}

void
Utl_rfc822Date( time_t t, Str res )
{
    struct tm *local;
    long tzdiff, hoffset, moffset;

    tzdiff = - tzDiff();

    local = localtime( &t );
    if ( local == NULL )
    {
	Utl_cpyStr( res, "** localtime failure **" );
	return;
    }
    
    hoffset = tzdiff / 60;
    moffset = tzdiff % 60;
    if ( moffset < 0 )
	moffset = - moffset;

    sprintf( res, "%s, %d %s %4d %02d:%02d:%02d %+03ld%02ld",
	     DOTW[local->tm_wday], local->tm_mday,
	     MON[local->tm_mon], local->tm_year + 1900,
	     local->tm_hour, local->tm_min, local->tm_sec,
	     hoffset, moffset );
}

void
Utl_allocAndCpy( char **dst, const char *src )
{
    int len = strlen( src );
    if ( ! ( *dst = malloc( (size_t)len + 1 ) ) )
    {
        Log_err( "Cannot allocate string with length %lu", strlen( src ) );
        exit( EXIT_FAILURE );
    }
    memcpy( *dst, src, (size_t)len + 1 );
}
