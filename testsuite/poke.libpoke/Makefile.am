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
check_PROGRAMS = values api foreign-iod
endif

# Common variables used for all/most test programs.

COMMON_CPPFLAGS = -I$(top_builddir)/gl -I$(top_srcdir)/gl \
                  -I$(top_srcdir)/common \
                  -DTESTDIR=\"$(abs_srcdir)\" \
                  -DPKGDATADIR=\"$(pkgdatadir)\" \
                  -I$(top_srcdir)/libpoke -I$(top_builddir)/libpoke
# Old DejaGnu versions need a specific old interpretation of 'inline'.
COMMON_CFLAGS = -fgnu89-inline
COMMON_LDADD = $(top_builddir)/gl/libgnu.la \
               $(top_builddir)/libpoke/libpoke.la \
               $(LTLIBTEXTSTYLE)

# The test programs.

values_SOURCES = $(COMMON) values.c
values_CPPFLAGS = $(COMMON_CPPFLAGS)
values_CFLAGS = $(COMMON_CFLAGS)
values_LDADD = $(COMMON_LDADD)

api_SOURCES = $(COMMON) api.c
api_CPPFLAGS = $(COMMON_CPPFLAGS)
api_CFLAGS = $(COMMON_CFLAGS)
api_LDADD = $(COMMON_LDADD)

foreign_iod_SOURCES = $(COMMON) foreign-iod.c
foreign_iod_CPPFLAGS = $(COMMON_CPPFLAGS)
foreign_iod_CFLAGS = $(COMMON_CFLAGS)
foreign_iod_LDADD = $(COMMON_LDADD)
