/*
  util.h

  Miscellaneous helper functions.

  $Id: util.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef UTL_H
#define UTL_H

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

#include "common.h"

/*
  Find first non-whitespace character after <token> tokens in string <line>.
  Return pointer to end of string, if parsing failed.
*/
const char *
Utl_restOfLn( const char *line, unsigned int token );

void
Utl_toLower( Str line );

/*
  Read a line from string.
  Return NULL if pos == NULL or no more line to read
*/
const char *
Utl_getLn( Str result, const char *p );

/*
  Go back to last line.
*/
const char *
Utl_ungetLn( const char *str, const char *p );

/*
  Read a header line from string. Reads continuation lines if
  necessary. Return NULL if pos == NULL or no more line to read
*/
const char *
Utl_getHeaderLn( Str result, const char *p );

/*
  Strip white spaces from left and right side.
  Return pointer to new start. This is within line.
*/
char *
Utl_stripWhiteSpace( char *line );

/* Strip comment from a line. Comments start with '#'. */
void
Utl_stripComment( char *line );

/* Write timestamp into <file>. */
void
Utl_stamp( Str file );

/* Get time stamp from <file> */
Bool
Utl_getStamp( time_t *result, Str file );

/* Put RFC822-compliant date string into res. */
void
Utl_rfc822Date( time_t t, Str res );

void
Utl_cpyStr( Str dst, const char *src );

void
Utl_cpyStrN( Str dst, const char *src, int n );

void
Utl_catStr( Str dst, const char *src );

void
Utl_catStrN( Str dst, const char *src, int n );

/* String allocation and copying. */
void
Utl_allocAndCpy( char **dst, const char *src );

#endif
