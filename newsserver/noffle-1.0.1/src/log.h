/*
  log.h

  Print log messages to syslog, stdout/stderr.

  $Id: log.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef LOG_H
#define LOG_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

/*
  Initialise logging (required before using any log functions).
  name: program name for syslog
  interactive: print messages also to stderr/stdout
  facility: like syslog
*/
void
Log_init( const char *name, Bool interactive, int facility );

/* Log level info */
void
Log_inf( const char *fmt, ... );

/* Log level error */
void
Log_err( const char *fmt, ... );

/* Check for cond being true. Otherwise log an error, and return 1. */
int 
Log_check(int cond, const char *fmt, ... );

/* Log level notice */
void
Log_ntc( const char *fmt, ... );

/* Log only if DEBUG is defined. */
void
Log_dbg( const char *fmt, ... );

#endif
