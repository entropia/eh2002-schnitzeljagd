/*
  online.h

  Online/offline status.

  $Id: online.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef ONLINE_H
#define ONLINE_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

Bool
Online_true( void );

void
Online_set( Bool value );

#endif
