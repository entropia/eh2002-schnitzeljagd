/*
  content.c

  $Id: content.c,v 1.1 2002-03-28 17:41:24 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "common.h"
#include "configfile.h"
#include "group.h"
#include "log.h"
#include "over.h"
#include "pseudo.h"
#include "util.h"
#include "content.h"
#include "portable.h"

struct
{
    DIR *dir;           /* Directory for browsing through all
                           groups */
    int vecFirst;	/* First article number in vector */
    int first;		/* First live article number */
    int last;		/* Last article number */
    int size;           /* Number of overviews. */
    int max;            /* Size of elem. */
    Over **elem;        /* Ptr to array with ptrs to overviews.
                           NULL entries for non-existing article numbers
                           in group. */
    Str name;
    Str file;
} cont = { NULL, 1, 1, 0, 0, 0, NULL, "", "" };

void
Cont_app( Over *ov )
{
    if ( cont.max < cont.size + 1 )
    {
        if ( ! ( cont.elem = realloc( cont.elem,
                                      ( cont.max + 500 )
                                      * sizeof( cont.elem[ 0 ] ) ) ) )
        {
            Log_err( "Could not realloc overview list" );
            exit( EXIT_FAILURE );
        }
        cont.max += 500;
    }
    ASSERT( cont.vecFirst > 0 );
    if ( ov )
        Ov_setNumb( ov, cont.vecFirst + cont.size );
    cont.elem[ cont.size++ ] = ov;
    cont.last = cont.vecFirst + cont.size - 1;
}

Bool
Cont_validNumb( int n )
{
    return ( n != 0 && n >= cont.first && n <= cont.last
             && cont.elem[ n - cont.vecFirst ] );
}

void
Cont_delete( int n )
{
    Over **ov;

    if ( ! Cont_validNumb( n ) )
        return;
    ov = &cont.elem[ n - cont.vecFirst ];
    free( *ov );
    *ov = NULL;
}

/* Remove all overviews from content. */
static void
clearCont( void )
{
    int i;

    for ( i = 0; i < cont.size; ++i )
        del_Over( cont.elem[ i ] );
    cont.size = 0;
}

static void
setupEmpty( const char *name )
{
    cont.last = Grp_last( name );
    cont.first = cont.vecFirst = cont.last + 1;
    ASSERT( cont.first > 0 );
}

/* Extend content list to size "cnt" and append NULL entries. */
static void
extendCont( int cnt )
{
    int i, n;
    
    if ( cont.size < cnt )
    {
        n = cnt - cont.size;
        for ( i = 0; i < n; ++i )
            Cont_app( NULL );
    }
}

/* Discard all cached overviews, and read in the overviews of a new group
   from its overviews file. */
void
Cont_read( const char *name )
{
    FILE *f;
    Over *ov;
    int numb;
    Str line;

    /* Delete old overviews and make room for new ones. */
    cont.vecFirst = 0;
    cont.first = 0;
    cont.last = 0;
    Utl_cpyStr( cont.name, name );
    clearCont();

    /* read overviews from overview file and store them in the overviews
       list */
    snprintf( cont.file, MAXCHAR, "%s/overview/%s", Cfg_spoolDir(), name ); 
    f = fopen( cont.file, "r" );
    if ( ! f )
    {
        Log_dbg( "No group overview file: %s", cont.file );
	setupEmpty( name );
        return;
    }
    Log_dbg( "Reading %s", cont.file );
    while ( fgets( line, MAXCHAR, f ) )
    {
        if ( ! ( ov = Ov_read( line ) ) )
        {
            Log_err( "Overview corrupted in %s: %s", name, line );
            continue;
        }
        numb = Ov_numb( ov );
        if ( numb < cont.first )
        {
            Log_err( "Wrong ordering in %s: %s", name, line );
            continue;
        }
        if ( cont.first == 0 )
            cont.first = cont.vecFirst = numb;
        cont.last = numb;
        extendCont( numb - cont.first + 1 );
        cont.elem[ numb - cont.first ] = ov;
    }
    fclose( f );

    if ( cont.first == 0 )
	setupEmpty( name );		/* Corrupt overview file recovery */
    else
    {
	int grpLast;

	/*
	  Check for end article(s) being cancelled. Need to ensure we
	  don't re-use and article number.
	 */
	grpLast = Grp_last( name );
	if ( cont.last < grpLast )
	    extendCont( grpLast - cont.first + 1 );
    }
}

void
Cont_write( void )
{
    Bool anythingWritten;
    int i;
    FILE *f;
    const Over *ov, *ov_next;

    /* Save the overview */
    if ( ! ( f = fopen( cont.file, "w" ) ) )
    {
        Log_err( "Could not open %s for writing", cont.file );
        return;
    }
    Log_dbg( "Writing %s (%lu)", cont.file, cont.size );
    anythingWritten = FALSE;
    cont.first = -1;
    for ( i = 0; i < cont.size; ++i )
    {
	ov = cont.elem[ i ];
        if ( ov )
        {
	    if ( i + 1 < cont.size )
		ov_next = cont.elem[ i + 1 ];
	    else
		ov_next = NULL;
	
	    /*
	      Preserve gen info if it is followed immediately by an
	      article with the next number. In practice, this means
	      that when in auto-subscribed mode, the gen info will
	      remain until the 'group now subscribed' message is
	      expired.
	     */
            if ( ! Pseudo_isGeneralInfo( Ov_msgId( ov ) )
		 || ( ov_next != NULL &&
		      Ov_numb( ov_next ) - Ov_numb( ov ) == 1 ) )
            {
                if ( ! Ov_write( ov, f ) )
                {
                    Log_err( "Writing of overview line failed" );
                    break;
                }
                else
		{
                    anythingWritten = TRUE;
		    if  ( cont.first < 0 )
			cont.first = cont.vecFirst + i;
		}
            }
        }
    }
    fclose( f );

    /*
      If empty, remove the overview file and set set first to one
      beyond last to flag said emptiness.
     */
    if ( ! anythingWritten )
    {
	unlink( cont.file );
	cont.first = cont.last + 1;
    }
}

const Over *
Cont_get( int numb )
{
    if ( ! Cont_validNumb( numb ) )
        return NULL;
    return cont.elem[ numb - cont.vecFirst ];
}

int
Cont_first( void ) { return cont.first; }

int
Cont_last( void ) { return cont.last; }

int
Cont_find( const char *msgId )
{
    int i;
    const Over *ov;
    
    for ( i = 0; i < cont.size; i++ )
    {
        if ( ( ov = cont.elem[ i ] )
	     && strcmp( Ov_msgId( ov ), msgId ) ==  0 )
	    return i + cont.vecFirst;
    }

    return -1;
}

const char *
Cont_grp( void ) { return cont.name; }

Bool
Cont_nextGrp( Str result )
{
    struct dirent *d;
    
    ASSERT( cont.dir );
    if ( ! ( d = readdir( cont.dir ) ) )
    {
        cont.dir = NULL;
        return FALSE;
    }
    if ( ! d->d_name )
        return FALSE;
    Utl_cpyStr( result, d->d_name );
    result[ MAXCHAR - 1 ] = '\0';
    return TRUE;
}

Bool
Cont_firstGrp( Str result )
{
    Str name;

    snprintf( name, MAXCHAR, "%s/overview", Cfg_spoolDir() );
    if ( ! ( cont.dir = opendir( name ) ) )
    {
        Log_err( "Cannot open %s", name );
        return FALSE;
    }
    Cont_nextGrp( result ); /* "."  */
    Cont_nextGrp( result ); /* ".." */
    return Cont_nextGrp( result );
}
