/*
  itemlist.c

  $Id: itemlist.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "itemlist.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "log.h"
#include "portable.h"

#if defined(ITEMLIST_TEST)
#define	Log_err	printf
#endif

#define	SEP_CHAR	'\1'	/* Replace all separators with this */

struct ItemList
{
    char *list;
    char *next;
    size_t count;
};

/* Make a new item list. */
ItemList *
new_Itl( const char *list, const char *separators )
{
    ItemList * res;
    char *p;
    Bool inItem;

    res = malloc( sizeof( ItemList ) );
    if ( res == NULL )
    {
	Log_err( "Malloc of ItemList failed." );
	exit( EXIT_FAILURE );
    }
    
    res->list = malloc ( strlen(list) + 2 );
    if ( res->list == NULL )
    {
	Log_err( "Malloc of ItemList.list failed." );
	exit( EXIT_FAILURE );
    }
    strcpy( res->list, list );

    res->count = 0;
    res->next = res->list;

    /* Separate items into strings and have final zero-length string. */
    for( p = res->list, inItem = FALSE; *p != '\0'; p++ )
    {
	Bool isSep = ( strchr( separators, p[ 0 ] ) != NULL );
	
	if ( inItem )
	{
	    if ( isSep )
	    {
		p[ 0 ] = '\0';
		inItem = FALSE;
		res->count++;
	    }
	}
	else
	{
	    if ( isSep )
		p[ 0 ] = SEP_CHAR;
	    else
		inItem = TRUE;
	}
    }
    if ( inItem )
	res->count++;
    p[ 1 ] = '\0';
    return res;
}

/* Delete an item list. */
void
del_Itl( ItemList *self )
{
    if ( self == NULL )
	return;
    free( self->list );
    free( self );
}

/* Get first item. */
const char *
Itl_first( ItemList *self)
{
    self->next = self->list;
    return Itl_next( self );
}

/* Get next item or NULL. */
const char *
Itl_next( ItemList *self )
{
    char *res = self->next;

    if ( res[ 0 ] == '\0' )
	return NULL;

    while ( res[ 0 ] == SEP_CHAR )
	res++;

    if ( res[ 0 ] == '\0' && res[ 1 ] == '\0' )
	return NULL;

    self->next = res + strlen( res ) + 1;
    return res;
}

/* Get count of items in list. */
size_t
Itl_count( const ItemList *self )
{
    return self->count;
}

#if defined(ITEMLIST_TEST)

/* Test code borrowed from wildmat.c. Yep, still uses gets(). */
extern char	*gets();

int
main()
{
    Str line;
    Str seps;
    ItemList * itl;
    int count;
    const char *item;

    printf( "Itemlist tester.  Enter seperators, then strings to test.\n" );
    printf( "A blank line gets prompts for new seperators; blank separators\n" );
    printf( "exits the program.\n" );

    for ( ; ; )
    {
	printf( "\nEnter seperators:  " );
	(void) fflush( stdout );
	if ( gets( seps ) == NULL || seps[0] == '\0' )
	    break;
	for ( ; ; )
	{
	    printf( "Enter line:  " );
	    (void) fflush( stdout );
	    if ( gets( line ) == NULL )
		exit( 0 );
	    if ( line[0] == '\0' )
		break;
	    itl = new_Itl( line, seps );
	    printf( "%d items on list\n", Itl_count( itl ) );
	    count = 0;
	    for ( item = Itl_first( itl );
		  item != NULL;
		  item = Itl_next( itl ) )
		printf( "  Item %d is '%s'\n", ++count, item );
	    if ( count != Itl_count( itl ) )
		printf( "*** Warning - counts don't match ***\n" );
	    del_Itl( itl );
	}
    }

    exit(0);
    /* NOTREACHED */
}
#endif	/* defined(TEST) */
