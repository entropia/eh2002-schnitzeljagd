/*
  fetchlist.c

  $Id: fetchlist.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "fetchlist.h"
#include "configfile.h"
#include "log.h"
#include "util.h"
#include "portable.h"

struct Elem
{
    Str name;
    FetchMode mode;
};

static struct Fetchlist
{
    struct Elem *elem;
    int size;
    int max;
} fetchlist = { NULL, 0, 0 };

static const char *
getFile( void )
{
    static Str file;
    snprintf( file, MAXCHAR, "%s/fetchlist", Cfg_spoolDir() );
    return file;
}

static void
clearList( void )
{
    fetchlist.size = 0;
}

static int
compareElem( const void *elem1, const void *elem2 )
{
    const struct Elem* e1 = (const struct Elem*)elem1; 
    const struct Elem* e2 = (const struct Elem*)elem2;
    return strcmp( e1->name, e2->name );
}

static struct Elem *
searchElem( const char *name )
{
    int i;
    
    for ( i = 0; i < fetchlist.size; ++i )
        if ( strcmp( name, fetchlist.elem[ i ].name ) == 0 )
            return &fetchlist.elem[ i ];
    return NULL;
}

static void
appGrp( const char *name, FetchMode mode )
{
    struct Elem elem;

    if ( fetchlist.max < fetchlist.size + 1 )
    {
        if ( ! ( fetchlist.elem
                 = realloc( fetchlist.elem,
                            ( fetchlist.max + 50 )
                            * sizeof( fetchlist.elem[ 0 ] ) ) ) )
        {
            Log_err( "Could not realloc fetchlist" );
            exit( EXIT_FAILURE );
        }
        fetchlist.max += 50;
    }
    strcpy( elem.name, name );
    elem.mode = mode;
    fetchlist.elem[ fetchlist.size++ ] = elem;
}

void
Fetchlist_read( void )
{
    FILE *f;
    const char *file = getFile();
    char *p;
    FetchMode mode = OVER;
    Bool valid;
    int ret;
    Str line, grp, modeStr;

    Log_dbg( "Reading %s", file );
    clearList();
    if ( ! ( f = fopen( file, "r" ) ) )
    {
        Log_inf( "No file %s", file );
        return;
    }
    while ( fgets( line, MAXCHAR, f ) )
    {
        p = Utl_stripWhiteSpace( line );
        if ( *p == '#' || *p == '\0' )
            continue;
        ret = sscanf( p, "%s %s", grp, modeStr );
        valid = TRUE;
        if ( ret < 1 || ret > 2 )
            valid = FALSE;
        else if ( ret >= 2 )
        {
            if ( strcmp( modeStr, "full" ) == 0 )
                mode = FULL;
            else if ( strcmp( modeStr, "thread" ) == 0 )
                mode = THREAD;
            else if ( strcmp( modeStr, "over" ) == 0 )
                mode = OVER;
            else
                valid = FALSE;
        }
        if ( ! valid )
        {
            Log_err( "Invalid entry in %s: %s", file, line );
            continue;
        }
        appGrp( grp, mode );
    }
    fclose( f );
}

Bool
Fetchlist_write( void )
{
    int i;
    FILE *f;
    const char *file = getFile();
    const char *modeStr = "";

    qsort( fetchlist.elem, (size_t)fetchlist.size,
           sizeof( fetchlist.elem[ 0 ] ), compareElem );
    if ( ! ( f = fopen( file, "w" ) ) )
    {
        Log_err( "Could not open %s for writing", file );
        return FALSE;
    }
    for ( i = 0; i < fetchlist.size; ++i )
    {
        switch ( fetchlist.elem[ i ].mode )
        {
        case FULL:
            modeStr = "full"; break;
        case THREAD:
            modeStr = "thread"; break;
        case OVER:
            modeStr = "over"; break;
        }
        fprintf( f, "%s %s\n", fetchlist.elem[ i ].name, modeStr );
    }
    fclose( f );
    return TRUE;
}

int
Fetchlist_size( void )
{
    return fetchlist.size;
}

Bool
Fetchlist_contains( const char *name )
{
    return ( searchElem( name ) != NULL );
}

Bool
Fetchlist_element( const char **name, FetchMode *mode, int idx )
{
    if ( idx < 0 || idx >= fetchlist.size )
        return FALSE;
    *name = fetchlist.elem[ idx ].name;
    *mode = fetchlist.elem[ idx ].mode;
    return TRUE;
}

Bool
Fetchlist_add( const char *name, FetchMode mode )
{
    struct Elem *elem = searchElem( name );
    if ( elem == NULL )
    {
        appGrp( name, mode );
        return TRUE;
    }
    strcpy( elem->name, name );
    elem->mode = mode;
    return FALSE;
}

Bool
Fetchlist_remove( const char *name )
{
    struct Elem *elem = searchElem( name );
    if ( elem == NULL )
        return FALSE;
    *elem = fetchlist.elem[ fetchlist.size - 1 ];
    --fetchlist.size;
    return TRUE;
}
