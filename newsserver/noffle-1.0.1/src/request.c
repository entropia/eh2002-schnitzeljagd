/*
  request.c

  Collection of articles that are marked for download.

  $Id: request.c,v 1.1 2002-03-28 17:41:25 dividuum Exp $
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include "request.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include "configfile.h"
#include "log.h"
#include "util.h"
#include "portable.h"


/* This struct keeps record of the message IDs that are to be fetched from
   one particular host. Several of these are chained together via the
   "next" pointer, if we have several servers.
*/

struct Reqserv;
typedef struct Reqserv Reqserv;

struct Reqserv {
  char*    serv;                /* Server the messages are to be requested
                                   from */
  char**   reql;                /* List of message IDs of requested
                                   messages. Some entries (that have been
                                   deleted) may be NULL */
  int      reql_length;         /* Number of string pointers in reql,
                                   including NULL entries */
  int      reql_capacity;       /* maximum number of string pointers reql
                                   can hold */
  Bool     dirty;               /* whether the request list needs to be
                                   rewritten to disk */
  Reqserv* next;                /* next Reqserv in list */
  time_t   mtime;               /* last modification time of request file */ 
};

/* List of servers */
static Reqserv* reqserv = 0;

/* sanity check */
static Bool is_open = FALSE;

/* for Req_first/Req_next */
static char** iterator = 0;
static char** iterator_end = 0;


/* local functions */
static Reqserv* newReqserv      (const char* serv);
static Bool     getReqserv      (const char* serv, Reqserv** rsz);
static void     fileRequest     (Str file, const char *serv);
static char**   searchMsgId     (const Reqserv * rs, const char *msgId);
static void     storeMsgId      (Reqserv* rs, const char* msgId);
static Bool     readRequestfile (const char* serv, Reqserv** rsz);
static time_t   get_mtime       (const char* serv);

/* read modification time of request file */
static time_t get_mtime(const char* serv)
{
  Str filename;
  struct stat stat1;

  fileRequest(filename, serv);
  stat(filename, &stat1);
  return stat1.st_mtime;
}


/* create new Reqserv and queue it */
static Reqserv* newReqserv(const char* serv)
{
  Reqserv* rs = malloc(sizeof(Reqserv));
  rs->serv = strcpy(malloc(strlen(serv)+1), serv);
  rs->reql = 0;
  rs->reql_length = 0;
  rs->reql_capacity = 0;
  rs->next = reqserv;
  rs->dirty = FALSE;
  rs->mtime = 0;
  reqserv = rs;
  return rs;
}


/* get Reqserv for given server, and save it in "rsz". Load from file as
   necessary. Return TRUE on success. Otherwise log errors and return
   FALSE. (details in errno)
*/
static Bool getReqserv(const char* serv, Reqserv** rsz)
{
  Reqserv* rs;
  for (rs = reqserv; rs; rs = rs->next)
    if (!strcmp(serv, rs->serv)) {
      *rsz = rs;
      return TRUE;
    }
  return readRequestfile(serv, rsz);
}


/* Delete Reqserv from cache, if not up-to-date */
static void
cleanupReqserv( void )
{
  Reqserv *rs, *prev, *next;

  rs = reqserv;
  prev = NULL;
  while ( rs != NULL )
  {      
      ASSERT( ! rs->dirty );
      next = rs->next;
      if ( get_mtime( rs->serv ) != rs->mtime )
      {
          if ( prev != NULL )
              prev->next = next;
          else
              reqserv = next;
          free( rs->serv );
          rs->serv = NULL;
          free( rs->reql );
          rs->reql = NULL;
          free( rs );
      }
      prev = rs;
      rs = next;
  }
}

/* Save name of file storing requests from server "serv" in "file" */
static void fileRequest( Str file, const char *serv)
{
  snprintf( file, MAXCHAR, "%s/requested/%s", Cfg_spoolDir(), serv);
}


/* Search for msgid in Reqserv. Return pointer to list entry. Return 0 if
   list does not contain msgid. */
static char** searchMsgId(const Reqserv * rs, const char *msgId )
{
  char** rz;
  ASSERT(rs != 0);

  if (!rs->reql)
    return 0;

  for (rz = rs->reql; rz < rs->reql + rs->reql_length; rz++)
    if (*rz && !strcmp(*rz, msgId))
      return rz;

  return 0;
}


Bool
Req_contains(const char *serv, const char *msgId)
{
  Reqserv* rs;
  ASSERT( is_open );
  if (getReqserv(serv, &rs) == FALSE) 
    return FALSE;
  return searchMsgId(rs, msgId) ? TRUE : FALSE;
}


static void storeMsgId(Reqserv* rs, const char* msgId)
{
  char* msgid;

  if (searchMsgId(rs, msgId))
    /* already recorded */
    return;

  msgid = strcpy(malloc(strlen(msgId)+1), msgId);

  if (rs->reql_length >= rs->reql_capacity) {
    int c1 = rs->reql_capacity*2 + 10;
    rs->reql = (char**) realloc(rs->reql, c1*sizeof(char*));
    rs->reql_capacity = c1;
  }

  *(rs->reql + rs->reql_length++) = msgid;
  rs->dirty = TRUE;
}


/* Add request for message "msgIg" from server "serv". Return TRUE iff
   successful. 
*/
Bool Req_add(const char *serv, const char *msgId)
{
    Reqserv* rs;
    ASSERT( is_open );
    Log_dbg( "Marking %s on %s for download", msgId, serv );

    if (getReqserv(serv, &rs) == FALSE) 
      return FALSE;
    storeMsgId(rs, msgId);
    return TRUE;
}

static Bool
readLn( Str line, FILE* f )
{
    size_t len;

    if ( ! fgets( line, MAXCHAR, f ) )
        return FALSE;
    len = strlen( line );
    if ( line[ len - 1 ] == '\n' )
        line[ len - 1 ] = '\0';
    return TRUE;
}

/* Read request file into new, non-queued Reqserv. Save new Reqserv in
   "rsz" and return TRUE on success. Returns FALSE on failure, see errno.
   If the file doesn't exist, an empty Reqserv is returned.
*/
static Bool readRequestfile(const char* serv, Reqserv** rsz)
{
  Str           filename;
  Str           line;
  FILE*         file;
  Reqserv*      rs;

  fileRequest(filename, serv);
  Log_dbg("reading request file %s", filename);

  file = fopen(filename, "r");
  if (!file && (errno == ENOENT)) {
    *rsz = newReqserv(serv);
    (*rsz)->mtime = get_mtime(serv);
    return TRUE;
  }
  if (Log_check(file != 0,
            "could not open %s for reading: %s", 
            filename, strerror(errno)))
    return FALSE;

  rs = *rsz = newReqserv(serv);

  while( readLn(line, file) == TRUE) {
    char* line1 = Utl_stripWhiteSpace(line);
    if (*line1)
      storeMsgId(rs, line1);
  }

  rs->dirty = FALSE;

  if (Log_check(fclose(file) != EOF, 
            "could not close %s properly: %s\n", 
            filename, strerror(errno)))
    return FALSE;

  return TRUE;
}


/* Write out request file for given Reqserv. Return TRUE on success. If an
   I/O error occurs, it is logged, and FALSE is returned.
*/
static Bool writeRequestfile(Reqserv* rs)
{
  Str    filename;
  FILE*  file;
  char** z;

  fileRequest(filename, rs->serv);
  Log_dbg("writing request file %s", filename);

  if (Log_check((file = fopen(filename, "w")) != 0,
            "could not open %s for writing: %s", 
            filename, strerror(errno)))
    return FALSE;

  if (rs->reql)
    for (z = rs->reql; z < rs->reql+rs->reql_length; z++)
      if (*z) {
        if (Log_check(   fputs(*z, file) != EOF
                      && fputs("\n", file) != EOF,
                  "write error: %s", strerror(errno)))
          return FALSE;
      }
  
  if (Log_check(fclose(file) != EOF, 
                "could not close %s properly: %s\n", 
                filename, strerror(errno)))
    return FALSE;
            
  rs->dirty = FALSE;
  rs->mtime = get_mtime(rs->serv);

  return TRUE;
}


void
Req_remove( const char *serv, const char *msgId )
{
    Reqserv* rs;
    char** z;
    
    ASSERT( is_open );
    Log_dbg("Req_remove(\"%s\", \"%s\")", serv, msgId);
    
    if (getReqserv(serv, &rs) == FALSE) 
        return;
    
    z = searchMsgId(rs, msgId);
    if ( ! z )
        return;
    
    free(*z);
    *z = 0;
    rs->dirty = TRUE;
}


Bool
Req_first( const char *serv, Str msgId )
{
  Reqserv* rs;

  ASSERT( is_open );
  ASSERT( !iterator && !iterator_end );

  if (getReqserv(serv, &rs) == FALSE)
    return FALSE;

  if (!rs->reql) 
    return FALSE;

  iterator = rs->reql - 1;
  iterator_end = rs->reql + rs->reql_length;

  return Req_next(msgId);
}


Bool
Req_next( Str msgId )
{
  ASSERT( is_open );
  ASSERT(iterator && iterator_end);

  if (iterator >= iterator_end)
      return FALSE;
  iterator++;

  while (iterator < iterator_end) {
    if (!*iterator)
      iterator++;
    else {
      Utl_cpyStr(msgId, *iterator);
      return TRUE;
    }
  }

  iterator = iterator_end = 0;
  return FALSE;
}


/* Get exclusive access to all request files. Maybe we already have had it,
   and the cache is outdated. So we delete request files, which have
   changed recently, from cache. These files will be reread on demand.
*/
Bool
Req_open(void)
{
  Log_dbg("opening request database");
  ASSERT(is_open == FALSE);
  cleanupReqserv();
  is_open = TRUE;
  return TRUE;
}


/* Do not occupy the request files any longer. Write any changes to disk.
   Return TRUE on success, FALSE if an IO error occurs. */
void Req_close(void) 
{
  Bool ret = TRUE;
  Reqserv* rs;
  Log_dbg("closing request database, writing changes to disk");
  ASSERT(is_open == TRUE);

  for (rs = reqserv; rs; rs = rs->next) {
    if (rs->dirty == TRUE) {
      if (!writeRequestfile(rs))
        ret = FALSE;
    }
  }

  is_open = FALSE;
}
