/*
  log.c

  $Id: log.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <syslog.h>
#include <stdarg.h>
#include "common.h"
#include "log.h"
#include "portable.h"

#define MAXLENGTH 240

struct
{
    Bool interactive;
} log = { FALSE };

void
Log_init( const char *name, Bool interactive, int facility )
{
    int option = LOG_PID | LOG_CONS;

    log.interactive = interactive;
    openlog( name, option, facility );
}

#define DO_LOG( LEVEL )               \
    va_list ap;                       \
    Str t;                            \
                                      \
    va_start( ap, fmt );              \
    vsnprintf( t, MAXCHAR, fmt, ap ); \
    if ( MAXLENGTH < MAXCHAR )        \
        t[ MAXLENGTH ] = '\0';        \
    syslog( LEVEL, "%s", t );         \
    if ( log.interactive )            \
        fprintf( stderr, "%s\n", t );   \
    va_end( ap );

void
Log_inf( const char *fmt, ... )
{
    DO_LOG( LOG_INFO );
}

void
Log_err( const char *fmt, ... )
{
    DO_LOG( LOG_ERR );
}

/* Ensure the condition "cond" is true; otherwise log an error and return 1 */
int 
Log_check(int cond, const char *fmt, ... )
{
  if (!cond) {
    DO_LOG( LOG_ERR );
    return 1;
  }
  return 0;
}

void
Log_ntc( const char *fmt, ... )
{
    DO_LOG( LOG_NOTICE );
}

void
Log_dbg( const char *fmt, ... )
{
#ifdef DEBUG
    DO_LOG( LOG_DEBUG );
#endif
}
