/*
  pseudo.h

  Handling of pseudo articles.

  $Id: pseudo.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef PSEUDO_H
#define PSEUDO_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "over.h"

/*
  General info is a special pseudo message for groups not on fetchlist.
  It is never stored in database, but generated every time a content is read.
  However the group counter is always increased. This ensures that there
  is always at least 1 article visible (even if the user deletes it) for
  using the auto-subscribe option.
*/
Bool
Pseudo_isGeneralInfo( const char *msgId );

void
Pseudo_appGeneralInfo( void );

const char *
Pseudo_generalInfoHead( void );

const char *
Pseudo_generalInfoBody( void );


const char *
Pseudo_markedBody( void );

const char *
Pseudo_alreadyMarkedBody( void );

const char *
Pseudo_markingFailedBody( void );

void
Pseudo_retrievingFailed( const char *msgId, const char *reason );


/*
  Other pseudo articles are stored in database and can contain dynamically
  generated information about the failure.
 */

void
Pseudo_cntInconsistent( const char *grp, int first, int last, int next );

void
Pseudo_missArts( const char *grp, int first, int next );

void
Pseudo_autoUnsubscribed( const char *grp, int days );

void
Pseudo_autoSubscribed( void );

#endif
