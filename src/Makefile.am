
lib_LTLIBRARIES = libtlserver.la

libtlserver_la_LDFLAGS  = -version-info @TL_SERVER_LTVERSION@
libtlserver_la_CFLAGS   = @TL_SERVER_CFLAGS@
libtlserver_la_CPPFLAGS = @TL_DEBUG_CPPFLAGS@
libtlserver_la_LIBADD   = $(AM_LIBS) @TL_SERVER_LIBS@
libtlserver_la_SOURCES  = server.c \
                          perlin.c

tlserver_includedir = $(includedir)/tunel/server
tlserver_include_HEADERS = server.h \
                           perlin.h
