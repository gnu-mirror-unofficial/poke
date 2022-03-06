# poked Makefile.am

# Copyright (C) 2022 Jose E. Marchesi

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

AUTOMAKE_OPTIONS = subdir-objects

appdir = $(pkgdatadir)/poked

dist_app_DATA = poked.pk pk-dump-minimal.pk

bin_PROGRAMS = poked
poked_SOURCES = poked.c \
                usock-buf.c \
                usock-buf.h \
                usock-buf-priv.h \
                usock.c \
                usock.h

poked_SOURCES += ../common/pk-utils.c ../common/pk-utils.h

poked_CPPFLAGS = -I$(top_srcdir)/common \
                 -I$(top_builddir)/gl-poked -I$(top_srcdir)/gl-poked \
                 -I$(top_srcdir)/libpoke -I$(top_builddir)/libpoke \
                 -DPKGDATADIR=\"$(pkgdatadir)\"
poked_CFLAGS = -Wall
poked_LDADD = $(top_builddir)/gl-poked/libgnu.la \
              $(top_builddir)/libpoke/libpoke.la \
              -lpthread
poked_LDFLAGS =
