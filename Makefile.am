SUBDIRS = src

bin_PROGRAMS = tlserver

tlserver_SOURCES = src/main.c
tlserver_LDADD   = src/libtlserver.la @TL_SERVER_LIBS@
tlserver_CFLAGS  = @TL_SERVER_CFLAGS@

pkgconfig_DATA = tlserver.pc

ACLOCAL_AMFLAGS = -I build/m4
