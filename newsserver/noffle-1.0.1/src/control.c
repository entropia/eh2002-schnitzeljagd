/*
  control.c

  $Id: control.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "control.h"
#include <stdio.h>
#include "common.h"
#include "content.h"
#include "database.h"
#include "group.h"
#include "itemlist.h"
#include "log.h"
#include "outgoing.h"
#include "portable.h"

int
Ctrl_cancel( const char *msgId )
{
    ItemList *refs;
    const char *ref;
    Str server;
    Bool seen = FALSE;
    int res = CANCEL_OK;

    /* See if in outgoing and zap if so. */
    if ( Out_find( msgId, server ) )
    {
	Out_remove( server, msgId );
	Log_inf( "'%s' cancelled from outgoing queue for '%s'.\n",
		 msgId, server );
	seen = TRUE;
    }

    if ( ! Db_contains( msgId ) )
    {
	Log_inf( "Cancel: '%s' not in database.", msgId );
	return seen ? CANCEL_OK : CANCEL_NO_SUCH_MSG;
    }

    /*
      Retrieve the Xrefs, remove from each group and then
      remove from the database.
     */
    refs = new_Itl( Db_xref( msgId ), " " );
    for( ref = Itl_first( refs ); ref != NULL; ref = Itl_next( refs ) )
    {
	Str grp;
	int no;

	if ( sscanf( ref, "%s:%d", grp, &no ) != 2 )
	    break;
	
	if ( Grp_exists( grp ) )
	{
	    Cont_read( grp );
	    Cont_delete( no );
	    Cont_write();

	    if ( ! Grp_local( grp ) && ! seen )
		res = CANCEL_NEEDS_MSG;

	    Log_dbg( "Removed '%s' from group '%s'.", msgId, grp );
	}
	else
	{
	    Log_inf( "Group '%s' in Xref for '%s' not found.", grp, msgId );
	}
    }
    del_Itl( refs );
    Db_delete( msgId );
    Log_inf( "Message '%s' cancelled.", msgId );
    return res;
}
