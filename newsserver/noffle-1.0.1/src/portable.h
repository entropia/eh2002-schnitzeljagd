/*
  portable.h

  Compatibility checks and fallback-functions.

  $Id: portable.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef PORTABLE_H
#define PORTABLE_H    /* To stop multiple inclusions. */


#if HAVE_CONFIG_H
#include <config.h>
#endif

#if !defined(HAVE_VSNPRINTF) && defined(HAVE___VSNPRINTF)
#undef vsnprintf
#define vsnprintf __vsnprintf
#define HAVE_VSNPRINTF
#endif

/* This is *not* good, because vsprintf() doesn't do any bounds-checking */
#if !defined(HAVE_VSNPRINTF) && !defined(HAVE___VSNPRINTF)
#define vsnprintf(c, len, fmt, args) vsprintf(c, fmt, args)
#define HAVE_VSNPRINTF
#endif

#if !defined(HAVE_SNPRINTF) && defined(HAVE___SNPRINTF)
#undef snprintf
#define snprintf __snprintf
#define HAVE_SNPRINTF
#endif

/* Indicate deliberately unused argument. Possibly compiler specific. */
#define	UNUSED(x)	{ ( void ) x; }

#endif

