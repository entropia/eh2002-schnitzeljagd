# This Makefile.am is used by automake to generate the Makefile.in,
# which in turn will be used to generate the Makefile.

bin_PROGRAMS = noffle

noffle_SOURCES = \
client.c client.h \
common.h \
configfile.c configfile.h \
content.c content.h \
control.c control.h \
database.c database.h \
dynamicstring.c dynamicstring.h \
fetch.c fetch.h \
fetchlist.c fetchlist.h \
group.c group.h \
itemlist.c itemlist.h \
lock.c lock.h \
log.c log.h \
noffle.c \
online.c online.h \
outgoing.c outgoing.h \
over.c over.h \
portable.h \
post.c post.h \
protocol.c protocol.h \
pseudo.c pseudo.h \
request.c request.h \
server.c server.h \
util.c util.h \
wildmat.c wildmat.h

noffle_LDADD = -lgdbm

tags:
	etags $(noffle_SOURCES)

