/*
  online.c

  $Id: online.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include "common.h"
#include "configfile.h"
#include "log.h"
#include "online.h"
#include "portable.h"

static void
fileOnline( Str s )
{
    snprintf( s, MAXCHAR, "%s/lock/online", Cfg_spoolDir() );
}

Bool
Online_true( void )
{
    FILE *f;
    Str file;

    fileOnline( file );
    if ( ! ( f = fopen( file, "r" ) ) )
        return FALSE;
    fclose( f );
    return TRUE;
}

void
Online_set( Bool value )
{
    FILE *f;
    Str file;

    fileOnline( file );
    if ( value )
    {
        if ( ! ( f = fopen( file, "a" ) ) )
        {
            Log_err( "Could not create %s", file );
            return;
        }
        fclose( f );
        Log_inf( "NOFFLE is now online" );
    }
    else
    {
        if ( unlink( file ) != 0 )
        {
            Log_err( "Cannot remove %s", file );
            return;
        }
        Log_inf( "NOFFLE is now offline" );
    }
}
