/*
  dynamicstring.c

  $Id: dynamicstring.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "dynamicstring.h"

#include <sys/types.h>
#include "log.h"
#include "portable.h"

struct DynStr
{
    int len; /* Current length (without trailing '\0') */
    int max; /* Max length that fits into buffer (incl. trailing '\0') */
    char *str;
};

static void
reallocStr( DynStr *self, int max )
{
    ASSERT( max >= 0 );
    if ( max <= self->max )
        return;
    if ( ! ( self->str = (char *)realloc( self->str, (size_t)max ) ) )
    {
        Log_err( "Realloc of DynStr failed" );
        exit( EXIT_FAILURE );
    } 
    if ( self->max == 0 ) /* First allocation? */
        *(self->str) = '\0';
    self->max = max;
}

DynStr *
new_DynStr( int reserve )
{
    DynStr *s;
    
    if ( ! ( s = malloc( sizeof( DynStr ) ) ) )
    {
        Log_err( "Allocation of DynStr failed" );
        exit( EXIT_FAILURE );
    }
    s->len = 0;
    s->max = 0;
    s->str = NULL;
    if ( reserve > 0 )
        reallocStr( s, reserve + 1 );
    return s;
}

void
del_DynStr( DynStr *self )
{
    if ( ! self )
        return;
    free( self->str );
    self->str = NULL;
    free( self );
}

int
DynStr_len( const DynStr *self )
{
    return self->len;
}

const char *
DynStr_str( const DynStr *self )
{
    return self->str;
}

void
DynStr_app( DynStr *self, const char *s )
{
    int len;

    len = strlen( s );
    if ( self->len + len + 1 > self->max )
        reallocStr( self, self->len * 2 + len + 1 );
    strcpy( self->str + self->len, s );
    self->len += len;
}

void
DynStr_appDynStr( DynStr *self, const DynStr *s )
{
    if ( self->len + s->len + 1 > self->max )
        reallocStr( self, self->len * 2 + s->len + 1 );
    memcpy( self->str + self->len, s->str, (size_t)s->len + 1 );
    self->len += s->len;
}

void
DynStr_appLn( DynStr *self, const char *s )
{
    DynStr_app( self, s );
    DynStr_app( self, "\n" );
}

void
DynStr_appN( DynStr *self, const char *s, int n )
{
    int len = self->len;

    ASSERT( n >= 0 );
    if ( len + n + 1 > self->max )
        reallocStr( self, len * 2 + n + 1 );
    strncat( self->str + len, s, (size_t)n );
    self->len = len + strlen( self->str + len );
}

void
DynStr_clear( DynStr *self )
{
    self->len = 0;
    if ( self->max > 0 )
        *(self->str) = '\0';
}

