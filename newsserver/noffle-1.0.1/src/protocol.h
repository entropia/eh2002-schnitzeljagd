/*
  protocol.h

  Functions related with the NNTP protocol which are useful for both
  the server and the client.

  $Id: protocol.h,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#ifndef PRT_H
#define PRT_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "dynamicstring.h"
#include "over.h"

#define STAT_HELP_FOLLOWS        100
#define STAT_DEBUG_FOLLOWS       199

#define STAT_READY_POST_ALLOW    200
#define STAT_READY_NO_POST_ALLOW 201
#define STAT_CMD_OK              202
#define STAT_GOODBYE             205
#define STAT_GRP_SELECTED        211
#define STAT_GRPS_FOLLOW         215
#define STAT_ART_FOLLOWS         220
#define STAT_HEAD_FOLLOWS        221
#define STAT_BODY_FOLLOWS        222
#define STAT_ART_RETRIEVED       223
#define STAT_OVERS_FOLLOW        224
#define STAT_NEW_GRP_FOLLOW      231
#define STAT_POST_OK             240
#define STAT_AUTH_ACCEPTED       281

#define STAT_SEND_ART            340
#define STAT_MORE_AUTH_REQUIRED  381

#define STAT_NO_SUCH_GRP         411
#define STAT_NO_GRP_SELECTED     412
#define STAT_NO_ART_SELECTED     420
#define STAT_NO_NEXT_ART         421
#define STAT_NO_PREV_ART         422
#define STAT_NO_SUCH_NUMB        423
#define STAT_NO_SUCH_ID          430
#define STAT_ART_REJECTED        437
#define STAT_POST_FAILED         441
#define STAT_AUTH_REQUIRED       480
#define STAT_AUTH_REJECTED       482

#define STAT_NO_SUCH_CMD         500
#define STAT_SYNTAX_ERR          501
#define STAT_NO_PERMISSION       502
#define STAT_PROGRAM_FAULT       503

/* 
   Read next line from f into Str, up to "\n" or "\r\n". Don't save "\n"
   or "\r\n" in line. Terminate with '\0'. 
   If timeoutSeconds < 0, no timeout alarm is used.
*/
Bool
Prt_getLn( Str line, FILE *f, int timeoutSeconds );

/*
  Read a text line from server. Returns TRUE if line != ".", removes
  leading '.' otherwise.
  If timeoutSeconds < 0, no timeout alarm is used.
*/
Bool
Prt_getTxtLn( Str line, Bool *err, FILE *f, int timeoutSeconds );

/*
  Write text line to f. Escape "." at the beginning with another ".".
  Terminate with "\r\n".
*/
Bool
Prt_putTxtLn( const char* line, FILE *f );

/*
  Write text buffer of lines each ending with '\n'.
  Replace '\n' by "\r\n".
*/
Bool
Prt_putTxtBuf( const char *buf, FILE *f );

/* 
   Write text-ending "."-line to f
*/
Bool
Prt_putEndOfTxt( FILE *f );

/*
  Splits line in field and value. Field is converted to lower-case. 
*/
Bool
Prt_getField( Str resultField, Str resultValue, const char* line );

/* Search header. Works only with single line headers (ignores continuation
   lines */
Bool
Prt_searchHeader( const char *artTxt, const char* which, Str result );

Bool
Prt_isValidMsgId( const char *msgId );

void
Prt_genMsgId( Str msgId, const char *from, const char *suffix );

#endif
