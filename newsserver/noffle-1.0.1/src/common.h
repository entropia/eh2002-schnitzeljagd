/*
  common.h

  Common declarations.

  $Id: common.h,v 1.1 2002-03-28 17:41:24 dividuum Exp $
*/

#ifndef COMMON_H
#define COMMON_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FALSE 0
#define TRUE !0
#define MAXCHAR 2048

#ifdef DEBUG
#include <assert.h>
#define ASSERT( x ) \
    if ( ! ( x ) ) \
        Log_err( "ASSERTION FAILED: %s line %i", __FILE__, __LINE__ ); \
    assert( x )
#else
#define ASSERT( x )
#endif

typedef int Bool;
typedef char Str[ MAXCHAR ];

#endif
