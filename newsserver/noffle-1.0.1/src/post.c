/*
  post.c

  $Id: post.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "post.h"
#include <string.h>
#include "common.h"
#include "content.h"
#include "database.h"
#include "group.h"
#include "log.h"
#include "over.h"
#include "protocol.h"
#include "util.h"
#include "portable.h"

struct OverInfo
{
    Str subject;
    Str from;
    Str date;
    Str msgId;
    Str ref;
    size_t bytes;
    size_t lines;
};

struct Article
{
    const char * text;
    Bool posted;
    struct OverInfo over;
};

static struct Article article = { NULL, FALSE, { "", "", "", "", "", 0, 0 } };

static void
getOverInfo( struct OverInfo * o )
{
    const char *p = article.text;
    Str line, field, value;
    
    o->bytes = strlen( p );

    while( p != NULL )
    {
        p = Utl_getHeaderLn( line, p );
        if ( line[ 0 ] == '\0' )
	    break;

	/* Look for headers we need to stash. */
        if ( Prt_getField( field, value, line ) )
        {
	    if ( strcmp( field, "subject" ) == 0 )
		Utl_cpyStr( o->subject, value );
	    else if ( strcmp ( field, "from" ) == 0 )
		Utl_cpyStr( o->from, value );
	    else if ( strcmp ( field, "date" ) == 0 )
		Utl_cpyStr( o->date, value );
	    else if ( strcmp ( field, "references" ) == 0 )
		Utl_cpyStr( o->ref, value );
	    else if ( strcmp ( field, "message-id" ) == 0 )
		Utl_cpyStr( o->msgId, value );
	}
    }

    /* Move to start of body and count lines. */
    for ( p++, o->lines = 0; *p != '\0'; p++ )
	if ( *p == '\n' )
	    o->lines++;
}

/* Register an article for posting. */
Bool
Post_open( const char * text )
{
    if ( article.text != NULL )
    {
	Log_err( "Busy article in Post_open." );
	return FALSE;
    }

    memset( &article.over, 0, sizeof( article.over ) );
    article.text = text;
    getOverInfo( &article.over );

    if ( Db_contains( article.over.msgId ) )
    {
	Post_close();
	Log_err( "Duplicate article %s.", article.over.msgId );
	return FALSE;
    }

    return TRUE;
}


/* Add the article to a group. */
Bool
Post_add ( const char * grp )
{
    Over * over;
    const char *msgId;
    
    over = new_Over( article.over.subject,
		     article.over.from,
		     article.over.date,
		     article.over.msgId,
		     article.over.ref,
		     article.over.bytes,
		     article.over.lines );
    
    msgId = article.over.msgId;
    
    Cont_read( grp );
    Cont_app( over );
    Log_dbg( "Added message '%s' to group '%s'.", msgId, grp );

    if ( !article.posted )
    {
        Log_inf( "Added '%s' to database.", msgId );
        if ( ! Db_prepareEntry( over, Cont_grp(), Cont_last() )
	     || ! Db_storeArt ( msgId, article.text ) )
	    return FALSE;
	article.posted = TRUE;
    }
    else
    {
	Str t;
	const char *xref;

	xref = Db_xref( msgId );
	Log_dbg( "Adding '%s' to Xref of '%s'", grp, msgId );
	snprintf( t, MAXCHAR, "%s %s:%i", xref, grp, Ov_numb( over ) );
	Db_setXref( msgId, t );
    }
    
    Cont_write();
    Grp_setFirstLast( Cont_grp(), Cont_first(), Cont_last() );
    return TRUE;
}
   
/* Done with article - tidy up. */
void
Post_close( void )
{
    article.text = NULL;
    article.posted = FALSE;
}

