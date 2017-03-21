AUTOMAKE_OPTS = gnu

AM_CXXFLAGS = $(GLOBALINCLUDE) -D__STDC_CONSTANT_MACROS
AM_LDFLAGS = -L/usr/lib

noinst_LTLIBRARIES = libadefs.la
libadefs_la_SOURCES = \
adefs.cpp \
package_fs.cpp \
package_gcf.cpp \
package_zip.cpp

noinst_HEADERS = \
adefs.h \
package_fs.h \
package_gcf.h \
package_zip.h

