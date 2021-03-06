/*
  wildmat.c

  Taken from the INN 2.2 distribution and slightly altered to fit the
  Noffle environment. Changes are:
  o Rename wildmat() to Wld_match().
  o Adjust includes.
  
  $Id: wildmat.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $

  The entire INN distribution is covered by the following copyright
  notice. As this file originated in the INN distribution is it
  subject to the conditions of this notice.
  
    Copyright 1991 Rich Salz.
    All rights reserved.
    $Revision: 1.1 $

    Redistribution and use in any form are permitted provided that the
    following restrictions are are met:
        1.  Source distributions must retain this entire copyright notice
            and comment.
        2.  Binary distributions must include the acknowledgement ``This
            product includes software developed by Rich Salz'' in the
            documentation or other materials provided with the
            distribution.  This must not be represented as an endorsement
            or promotion without specific prior written permission.
        3.  The origin of this software must not be misrepresented, either
            by explicit claim or by omission.  Credits must appear in the
            source and documentation.
        4.  Altered versions must be plainly marked as such in the source
            and documentation and must not be misrepresented as being the
            original software.
    THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
    WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

*/

/*  $Revision: 1.1 $
**
**  Do shell-style pattern matching for ?, \, [], and * characters.
**  Might not be robust in face of malformed patterns; e.g., "foo[a-"
**  could cause a segmentation violation.  It is 8bit clean.
**
**  Written by Rich $alz, mirror!rs, Wed Nov 26 19:03:17 EST 1986.
**  Rich $alz is now <rsalz@osf.org>.
**  April, 1991:  Replaced mutually-recursive calls with in-line code
**  for the star character.
**
**  Special thanks to Lars Mathiesen <thorinn@diku.dk> for the ABORT code.
**  This can greatly speed up failing wildcard patterns.  For example:
**	pattern: -*-*-*-*-*-*-12-*-*-*-m-*-*-*
**	text 1:	 -adobe-courier-bold-o-normal--12-120-75-75-m-70-iso8859-1
**	text 2:	 -adobe-courier-bold-o-normal--12-120-75-75-X-70-iso8859-1
**  Text 1 matches with 51 calls, while text 2 fails with 54 calls.  Without
**  the ABORT code, it takes 22310 calls to fail.  Ugh.  The following
**  explanation is from Lars:
**  The precondition that must be fulfilled is that DoMatch will consume
**  at least one character in text.  This is true if *p is neither '*' nor
**  '\0'.)  The last return has ABORT instead of FALSE to avoid quadratic
**  behaviour in cases like pattern "*a*b*c*d" with text "abcxxxxx".  With
**  FALSE, each star-loop has to run to the end of the text; with ABORT
**  only the last one does.
**
**  Once the control of one instance of DoMatch enters the star-loop, that
**  instance will return either TRUE or ABORT, and any calling instance
**  will therefore return immediately after (without calling recursively
**  again).  In effect, only one star-loop is ever active.  It would be
**  possible to modify the code to maintain this context explicitly,
**  eliminating all recursive calls at the cost of some complication and
**  loss of clarity (and the ABORT stuff seems to be unclear enough by
**  itself).  I think it would be unwise to try to get this into a
**  released version unless you have a good test data base to try it out
**  on.
*/
#include <stdio.h>
#include <sys/types.h>
#include "common.h"
#include "log.h"
#include "wildmat.h"

#define	ABORT		(-1)

/* What character marks an inverted character class? */
#define NEGATE_CLASS		'^'
    /* Is "*" a common pattern? */
#define OPTIMIZE_JUST_STAR
    /* Do tar(1) matching rules, which ignore a trailing slash? */
#undef MATCH_TAR_PATTERN


/*
**  Match text and p, return TRUE, FALSE, or ABORT.
*/
static int DoMatch(const char *text, const char *p)
{
    int	                last;
    int	                matched;
    int	                reverse;

    for ( ; *p; text++, p++) {
	if (*text == '\0' && *p != '*')
	    return ABORT;
	switch (*p) {
	case '\\':
	    /* Literal match with following character. */
	    p++;
	    /* FALLTHROUGH */
	default:
	    if (*text != *p)
		return FALSE;
	    continue;
	case '?':
	    /* Match anything. */
	    continue;
	case '*':
	    while (*++p == '*')
		/* Consecutive stars act just like one. */
		continue;
	    if (*p == '\0')
		/* Trailing star matches everything. */
		return TRUE;
	    while (*text)
		if ((matched = DoMatch(text++, p)) != FALSE)
		    return matched;
	    return ABORT;
	case '[':
	    reverse = p[1] == NEGATE_CLASS ? TRUE : FALSE;
	    if (reverse)
		/* Inverted character class. */
		p++;
	    matched = FALSE;
	    if (p[1] == ']' || p[1] == '-')
		if (*++p == *text)
		    matched = TRUE;
	    for (last = *p; *++p && *p != ']'; last = *p)
		/* This next line requires a good C compiler. */
		if (*p == '-' && p[1] != ']'
		    ? *text <= *++p && *text >= last : *text == *p)
		    matched = TRUE;
	    if (matched == reverse)
		return FALSE;
	    continue;
	}
    }

#ifdef	MATCH_TAR_PATTERN
    if (*text == '/')
	return TRUE;
#endif	/* MATCH_TAR_ATTERN */
    return *text == '\0';
}


/*
**  User-level routine.  Returns TRUE or FALSE.
*/
Bool
Wld_match(const char *text, const char *pattern)
{
#ifdef	OPTIMIZE_JUST_STAR
    if (pattern[0] == '*' && pattern[1] == '\0')
	return TRUE;
#endif	/* OPTIMIZE_JUST_STAR */
    return DoMatch(text, pattern) == TRUE;
}



#if	defined(WILDMAT_TEST)

/* Yes, we use gets not fgets.  Sue me. */
extern char	*gets();


int
main()
{
    char	 p[80];
    char	 text[80];

    printf("Wildmat tester.  Enter pattern, then strings to test.\n");
    printf("A blank line gets prompts for a new pattern; a blank pattern\n");
    printf("exits the program.\n");

    for ( ; ; ) {
	printf("\nEnter pattern:  ");
	(void)fflush(stdout);
	if (gets(p) == NULL || p[0] == '\0')
	    break;
	for ( ; ; ) {
	    printf("Enter text:  ");
	    (void)fflush(stdout);
	    if (gets(text) == NULL)
		exit(0);
	    if (text[0] == '\0')
		/* Blank line; go back and get a new pattern. */
		break;
	    printf("      %s\n", Wld_match(text, p) ? "YES" : "NO");
	}
    }

    exit(0);
    /* NOTREACHED */
}
#endif	/* defined(TEST) */
