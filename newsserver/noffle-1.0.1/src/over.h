/*
  over.h

  Processing of single article overviews. Handling of overview files is in
  content.c. An article overview contains important article properties,
  such as date, from, subject.

  $Id: over.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $ 
*/

#ifndef OVER_H
#define OVER_H

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

struct Over;
typedef struct Over Over;

/*
  Usual fields from overview databases.
  Xref without hostname.
*/
Over *
new_Over( const char *subj, const char *from, const char *date,
          const char *msgId, const char *ref, size_t bytes, size_t lines );


/* free memory */
void
del_Over( Over *self );

/* read Over-struct from line */
Over *
Ov_read( char *line );

/* write struct Over to f as a line */
Bool
Ov_write( const Over *self, FILE *f );

/* Access particular fields in struct over */

int
Ov_numb( const Over *self );

const char *
Ov_subj( const Over *self );

const char *
Ov_from( const Over *self );

const char *
Ov_date( const Over *self );

const char *
Ov_msgId( const Over *self );

const char *
Ov_ref( const Over *self );

size_t
Ov_bytes( const Over *self );

size_t
Ov_lines( const Over *self );

void
Ov_setNumb( Over *self, int numb );

#endif
