/*
  control.h

  Control actions needed by server and command line.

  $Id: control.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef CONTROL_H
#define CONTROL_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#define	CANCEL_OK		0
#define	CANCEL_NO_SUCH_MSG	1
#define	CANCEL_NEEDS_MSG	2

/*
   Cancel a message. Return CANCEL_OK if completely cancelled,
   CANCEL_NO_SUCH_MSG if no message with that ID exists, and
   CANCEL_NEEDS_MSG if a 'cancel' message should be propagated upstream
   to cancel the message elsewhere.
 */
int
Ctrl_cancel( const char *msgId );

#endif
