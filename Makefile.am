SUBDIRS = src

bin_PROGRAMS = tlserver

tlserver_SOURCES  = src/main.c
tlserver_LDADD    = src/libtlserver.la @TL_SERVER_LIBS@
tlserver_CPPFLAGS = @TL_DEBUG_CPPFLAGS@
tlserver_CFLAGS   = @TL_SERVER_CFLAGS@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = tlserver.pc

ACLOCAL_AMFLAGS = -I build/m4
