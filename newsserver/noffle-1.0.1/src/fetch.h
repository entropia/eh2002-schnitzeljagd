/*
  fetch.h

  Do the daily business by using client.c

  $Id: fetch.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef FETCH_H
#define FETCH_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "database.h"
#include "fetchlist.h"

Bool
Fetch_init( const char *serv );

void
Fetch_close( void );

void
Fetch_getNewGrps( void );

void
Fetch_updateGrps( void );

void
Fetch_getReq_( void );

void
Fetch_postArts( void );

/* Get new articles in group "grp", using fetch mode "mode". */
void
Fetch_getNewArts( const char *grp, FetchMode mode );

#endif
