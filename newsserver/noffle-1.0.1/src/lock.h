/*
  lock.h

  Opening/Closing of the various databases: article overview database,
  articla database, groups database, outgoing articles database, requests
  database. Handles global lock.

  $Id: lock.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef LOCK_H
#define LOCK_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

/* Open all databases and set global lock. */
Bool
Lock_openDatabases( void );

/* Close all databases and release global lock. */
void
Lock_closeDatabases( void );

#endif
