/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */
/* For use by autoheader */

#ifndef CONFIG_H
#define CONFIG_H    /* To stop multiple inclusions. */

/* The NOFFLE config-file */
#define CONFIGFILE "/tmp/noffle/etc/noffle.conf"

/* The default NOFFLE spool-directory */
#define SPOOLDIR "/tmp/noffle/spool/noffle"

/* Enable GNU extensions. */
#define _GNU_SOURCE 1

/* Define to 'unsigned int' if not already defined by the system. */
/* #undef socklen_t */

/* path to sendmail binary */
#define SENDMAILPROG "/usr/sbin/sendmail"


/* autoheader generated things inserted here. */

/* Define if you have the <dirent.h> header file, and it defines `DIR'. */
#define HAVE_DIRENT_H 1

/* Define if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define if you have the `fdopen' function. */
#define HAVE_FDOPEN 1

/* Define if your system has a working `fnmatch' function. */
/* #undef HAVE_FNMATCH */

/* Define if you have the <fnmatch.h> header file. */
#define HAVE_FNMATCH_H 1

/* Define if you have the <gdbm.h> header file. */
/* #undef HAVE_GDBM_H */

/* Define if you have the `gethostname' function. */
#define HAVE_GETHOSTNAME 1

/* Define if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if you have the `gdbm' library (-lgdbm). */
/* #undef HAVE_LIBGDBM */

/* Define if you have the `nsl' library (-lnsl). */
/* #undef HAVE_LIBNSL */

/* Define if you have the `socket' library (-lsocket). */
/* #undef HAVE_LIBSOCKET */

/* Define if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define if you have the `mkdir' function. */
#define HAVE_MKDIR 1

/* Define if you have the `mktime' function. */
#define HAVE_MKTIME 1

/* Define if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define if you have the `select' function. */
#define HAVE_SELECT 1

/* Define if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define if you have the `strftime' function. */
#define HAVE_STRFTIME 1

/* Define if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1

/* Define if you have the <sys/dir.h> header file, and it defines `DIR'. */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <sys/utsname.h> header file. */
#define HAVE_SYS_UTSNAME_H 1

/* Define if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define if you have the `uname' function. */
#define HAVE_UNAME 1

/* Define if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define if you have the `__snprintf' function. */
/* #undef HAVE___SNPRINTF */

/* Define if you have the `__vsnprintf' function. */
#define HAVE___VSNPRINTF 1

/* Name of package */
#define PACKAGE "noffle"

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Define if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Version number of package */
#define VERSION "1.0.1"

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* # undef _ALL_SOURCE */
#endif

/* Define if on MINIX. */
/* #undef _MINIX */

/* Define if the system does not provide POSIX.1 features except with this
   defined. */
/* #undef _POSIX_1_SOURCE */

/* Define if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #undef size_t */


#endif /* CONFIG_H */

