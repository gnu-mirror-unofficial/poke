# GNU poke - Makefile.am for libutils

# Copyright (C) 2019, 2020 Jose E. Marchesi

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

MOSTLYCLEANFILES =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =

lib_LTLIBRARIES = libutils.la

libutils_la_SOURCES = pk-utils.c pk-utils.h
libutils_la_CFLAGS = -Wall -Wextra
libutils_la_CPPFLAGS = -D_GNU_SOURCE \
                       -I$(top_builddir)/gl-libutils -I$(top_srcdir)/gl-libutils
libutils_la_LIBADD = ../gl-libutils/libgnu.la
libutils_la_LDFLAGS =

# End of Makefile.am
