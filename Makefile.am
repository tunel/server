
bin_PROGRAMS = tlserver

tlserver_SOURCES  = src/main.c \
                    src/server.c \
                    src/server.h \
                    src/perlin.c \
                    src/perlin.h
tlserver_LDADD    = $(AM_LIBS) @TL_SERVER_LIBS@
tlserver_CPPFLAGS = @TL_DEBUG_CPPFLAGS@
tlserver_CFLAGS   = $(AM_CFLAGS) @TL_SERVER_CFLAGS@

ACLOCAL_AMFLAGS = -I build/m4
