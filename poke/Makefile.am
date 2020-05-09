# GNU poke

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

AUTOMAKE_OPTIONS = subdir-objects

MOSTLYCLEANFILES =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =

dist_pkgdata_DATA = pk-cmd.pk pk-dump.pk pk-save.pk pk-copy.pk

bin_PROGRAMS = poke
poke_SOURCES = poke.c poke.h \
               pk-term.c pk-term.h \
               pk-repl.c pk-repl.h \
               pk-cmd.c pk-cmd.h \
               pk-ios.c \
               pk-info.c pk-misc.c pk-help.c pk-vm.c \
               pk-def.c pk-set.c pk-editor.c

poke_SOURCES += ../common/pk-utils.c ../common/pk-utils.h

poke_CPPFLAGS = -I$(top_builddir)/gl -I$(top_srcdir)/gl \
                -I$(top_srcdir)/common \
                -I$(top_srcdir)/libpoke -I$(top_builddir)/libpoke \
                -DJITTER_VERSION=\"$(JITTER_VERSION)\" \
                -DPKGDATADIR=\"$(pkgdatadir)\" \
                -DPKGINFODIR=\"$(infodir)\" \
                -DLOCALEDIR=\"$(localedir)\"
poke_CFLAGS = -Wall
# XXX remove JITTER_LDADD
poke_LDADD = $(top_builddir)/gl/libgnu.la \
             $(top_builddir)/libpoke/libpoke.la \
             $(LTLIBREADLINE) \
             $(JITTER_LDADD) \
             $(LIBTEXTSTYLE)
poke_LDFLAGS =

# Machine inteface
if POKE_MI
poke_SOURCES += pk-mi.c pk-mi.h \
                pk-mi-msg.c pk-mi-msg.h \
                pk-mi-json.c pk-mi-json.h
poke_CFLAGS += $(JSON_C_CFLAGS)
poke_LDADD += $(JSON_C_LIBS)
endif

if HSERVER
  poke_SOURCES += pk-hserver.h pk-hserver.c
  poke_LDADD += -lpthread
endif

# Created by doc/Makefile.am.
MOSTLYCLEANFILES += nodelist

# End of Makefile.am
