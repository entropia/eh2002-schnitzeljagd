/*
  over.c

  $Id: over.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
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

#include <errno.h>
#include "configfile.h"
#include "content.h"
#include "database.h"
#include "fetchlist.h"
#include "log.h"
#include "util.h"
#include "protocol.h"
#include "pseudo.h"
#include "portable.h"

struct Over
{
    int numb;            /* message number of the overviewed article */
    char *subj;
    char *from;
    char *date;
    char *msgId;
    char *ref;
    size_t bytes;
    size_t lines;
    time_t time;
};

Over *
new_Over( const char *subj, const char *from,
          const char *date, const char *msgId, const char *ref,
          size_t bytes, size_t lines )
{
    Over *ov;

    if ( ! ( ov = malloc( sizeof( Over ) ) ) )
    {
        Log_err( "Cannot allocate Over" );
        exit( EXIT_FAILURE );
    }
    ov->numb = 0;
    Utl_allocAndCpy( &ov->subj, subj );
    Utl_allocAndCpy( &ov->from, from );
    Utl_allocAndCpy( &ov->date, date );
    Utl_allocAndCpy( &ov->msgId, msgId );
    Utl_allocAndCpy( &ov->ref, ref );
    ov->bytes = bytes;
    ov->lines = lines;
    return ov;
}

void
del_Over( Over *self )
{
    if ( ! self )
        return;
    free( self->subj );
    self->subj = NULL;
    free( self->from );
    self->from = NULL;
    free( self->date );
    self->date = NULL;
    free( self->msgId );
    self->msgId = NULL;
    free( self->ref );
    self->ref = NULL;
    free( self );
}

int
Ov_numb( const Over *self )
{
    return self->numb;
}

const char *
Ov_subj( const Over *self )
{
    return self->subj;
}

const char *
Ov_from( const Over *self )
{
    return self->from;
}

const char *
Ov_date( const Over *self )
{
    return self->date;
}

const char *
Ov_msgId( const Over *self )
{
    return self->msgId;
}

const char *
Ov_ref( const Over *self )
{
    return self->ref;
}

size_t
Ov_bytes( const Over *self )
{
    return self->bytes;
}

size_t
Ov_lines( const Over *self )
{
    return self->lines;
}

void
Ov_setNumb( Over *self, int numb )
{
    self->numb = numb;
}

Bool
Ov_write( const Over *self, FILE *f )
{
    return ( fprintf( f, "%i\t%s\t%s\t%s\t%s\t%s\t%d\t%d\n",
                      self->numb, self->subj,
                      self->from, self->date, self->msgId,
                      self->ref, self->bytes,
                      self->lines ) > 0 );
}

static const char *
readField( Str result, const char *p )
{
    size_t len;
    char *r;

    if ( ! p )
        return NULL;
    r = result;
    *r = '\0';
    len = 0;
    while ( *p != '\t' && *p != '\n' )
    {
        if ( ! *p )
            return p;
        *(r++) = *(p++);
        ++len;
        if ( len >= MAXCHAR - 1 )
        {
            *r = '\0';
            Log_err( "Field in overview too long: %s", r );
            return ++p;
        }
    }
    *r = '\0';
    return ++p;
}

/* read Over-struct from line */
Over *
Ov_read( char *line )
{
    size_t bytes, lines;
    const char *p;
    Over *result;
    int numb;
    Str t, subj, from, date, msgId, ref;
    
    p = readField( t, line );
    if ( sscanf( t, "%i", &numb ) != 1 )
        return NULL;
    p = readField( subj, p );
    p = readField( from, p );
    p = readField( date, p );
    p = readField( msgId, p );
    p = readField( ref, p );
    p = readField( t, p );
    if ( sscanf( t, "%d", &bytes ) != 1 )
        return NULL;
    p = readField( t, p );
    if ( sscanf( t, "%d", &lines ) != 1 )
        return NULL;
    result = new_Over( subj, from, date, msgId, ref, bytes, lines );
    Ov_setNumb( result, numb );
    return result;
}
