/*
  wildmat.h

  Noffle header file for wildmat.

  $Id: wildmat.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
 */

#ifndef WILDMAT_H
#define WILDMAT_H

/*
  See if test is matched by pattern p. Return TRUE if so.
 */
Bool
Wld_match(const char *text, const char *pattern);

#endif
