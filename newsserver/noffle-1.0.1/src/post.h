/*
  post.h

  Take a single article received in its entirety without an overview
  (i.e. received via at the server via a POST), and add it to the database
  and (possibly multiple) group(s).

  $Id: post.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef POST_H
#define POST_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

/* Register an article for posting. */
Bool
Post_open( const char * text );

/* Add the article to a group. */
Bool
Post_add ( const char * grp );
   
/* Done with article - tidy up. */
void
Post_close( void );

#endif
