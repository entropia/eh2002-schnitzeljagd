/*
  content.h

  Contents of a newsgroup
  - list of article overviews for selected group.

  The overviews of all articles of a group are stored in an overview file,
  filename SPOOLDIR/overview/GROUPNAME. One entire overview file is read
  and cached in memory, at a time.

  $Id: content.h,v 1.1 2002-03-28 17:41:24 dividuum Exp $ 
*/

#ifndef CONT_H
#define CONT_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "over.h"

/*
  Try to read overviews from overview file for group <grp>.
  Fill with fake articles, if something goes wrong.
*/
void
Cont_read( const char *grp );

/*
  Append overview to current list and increment the current
  group's last article counter. Ownership of the ptr is transfered
  to content
*/
void
Cont_app( Over *ov );

/* Write content */
void
Cont_write( void );

Bool
Cont_validNumb( int numb );

const Over *
Cont_get( int numb );

void
Cont_delete( int numb );

int
Cont_first( void );

int
Cont_last( void );

int
Cont_find( const char *msgId );

const char *
Cont_grp( void );

Bool
Cont_nextGrp( Str result );

Bool
Cont_firstGrp( Str result );

void
Cont_expire( void );

#endif
