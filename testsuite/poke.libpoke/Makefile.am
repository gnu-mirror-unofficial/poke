# Copyright (C) 2020, 2021 Jose E. Marchesi
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

EXTRA_DIST = \
  libpoke.exp

COMMON = term-if.h

if HAVE_DEJAGNU
check_PROGRAMS = values api
endif

values_SOURCES = $(COMMON) values.c

values_CPPFLAGS = -I$(top_builddir)/gl -I$(top_srcdir)/gl \
                  -I$(top_srcdir)/common \
                  -DTESTDIR=\"$(abs_srcdir)\" \
                  -DPKGDATADIR=\"$(pkgdatadir)\" \
                  -I$(top_srcdir)/libpoke -I$(top_builddir)/libpoke

# Old DejaGnu versions need a specific old interpretation of 'inline'.
values_CFLAGS = -fgnu89-inline

values_LDADD = $(top_builddir)/gl/libgnu.la \
               $(top_builddir)/libpoke/libpoke.la \
               $(LTLIBTEXTSTYLE)

api_SOURCES = $(COMMON) api.c

api_CPPFLAGS = -I$(top_builddir)/gl -I$(top_srcdir)/gl \
                  -I$(top_srcdir)/common \
                  -DTESTDIR=\"$(abs_srcdir)\" \
                  -DPKGDATADIR=\"$(pkgdatadir)\" \
                  -I$(top_srcdir)/libpoke -I$(top_builddir)/libpoke

# Old DejaGnu versions need a specific old interpretation of 'inline'.
api_CFLAGS = -fgnu89-inline

api_LDADD = $(top_builddir)/gl/libgnu.la \
               $(top_builddir)/libpoke/libpoke.la \
               $(LTLIBTEXTSTYLE)
