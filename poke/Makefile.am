# GNU poke

# Copyright (C) 2019, 2020, 2021, 2022 Jose E. Marchesi

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

BUILT_SOURCES =

EXTRA_DIST =

dist_pkgdata_DATA = pk-cmd.pk pk-dump.pk pk-save.pk pk-copy.pk \
                    pk-extract.pk pk-scrabble.pk poke.pk pk-settings.pk \
                    pk-help.pk pk-map.pk pk-table.pk pk-info.pk

bin_PROGRAMS = poke
poke_SOURCES = poke.c poke.h \
               pk-term.c pk-term.h \
               pk-table.c pk-table.h \
               pk-repl.c pk-repl.h \
               pk-cmd.c pk-cmd.h \
               pk-cmd-ios.c pk-cmd-info.c pk-cmd-misc.c \
               pk-cmd-help.c pk-cmd-def.c pk-cmd-vm.c \
               pk-cmd-set.c pk-cmd-editor.c pk-cmd-map.c \
               pk-ios.c pk-ios.h \
               pk-map.c pk-map.h pk-map-parser.h \
               pk-map-tab.c pk-map-lex.l

poke_SOURCES += ../common/pk-utils.c ../common/pk-utils.h

poke_CPPFLAGS = -I$(top_builddir)/gl -I$(top_srcdir)/gl \
                -I$(top_srcdir)/common \
                -I$(top_srcdir)/libpoke -I$(top_builddir)/libpoke \
                -DJITTER_VERSION=\"$(JITTER_VERSION)\" \
                -DPKGDATADIR=\"$(pkgdatadir)\" \
                -DPKGINFODIR=\"$(infodir)\" \
                -DLOCALEDIR=\"$(localedir)\"
poke_CFLAGS = -Wall
poke_LDADD = $(top_builddir)/gl/libgnu.la \
             $(top_builddir)/libpoke/libpoke.la \
             $(LTLIBREADLINE) \
             $(LTLIBTEXTSTYLE) \
             $(LTLIBTERMCAP)
poke_LDFLAGS =


AM_LFLAGS = -d
# The Automake generated .l.c rule is broken: When executed in a VPATH build,
#   - The .c file gets generated in the build directory. But since it requires
#     special tools to rebuild it, we need to distribute it in the tarballs,
#     and by the GNU Coding Standards
#     <https://www.gnu.org/prep/standards/html_node/Makefile-Basics.html>
#     the file should be generated in the source directory.
#   - The #line directives in the .c file refer to a nonexistent file once it
#     has been moved from the build directory to the source directory. This
#     leads to error if 'lcov' is used later.
# Additionally, here we assume Flex and therefore don't need the ylwrap script.
# Therefore we override this rule.
# Since this is a rule that produces multiple files, we apply the idiom from
# <https://lists.gnu.org/archive/html/bug-make/2020-09/msg00008.html>, so that
# it works also in parallel 'make'.
generate-pk-map-lex:
	$(AM_V_LEX)$(LEX) $(LFLAGS) $(AM_LFLAGS) -t $(srcdir)/pk-map-lex.l > pk-map-lex.c \
	&& test ':' = '$(LEX)' || { \
	  sed -e 's|".*/pk-map-lex\.l"|"pk-map-lex.l"|' \
	      -e 's|"lex\.yy\.c"|"pk-map-lex.c"|' \
	      < pk-map-lex.c > pk-map-lex.c-tmp \
	  && sed -e 's|".*/pk-map-lex\.l"|"pk-map-lex.l"|' \
	         < pk-map-lex.h > pk-map-lex.h-tmp \
	  && rm -f pk-map-lex.c pk-map-lex.h \
	  && mv pk-map-lex.c-tmp $(srcdir)/pk-map-lex.c \
	  && mv pk-map-lex.h-tmp $(srcdir)/pk-map-lex.h; \
	}
.PHONY: generate-pk-map-lex
# The above rule will generate files with time-stamp order
# pk-map-lex.l <= pk-map-lex.c <= pk-map-lex.h.
pk-map-lex.c: pk-map-lex.l
	@{ test -f $(srcdir)/pk-map-lex.c && test ! $(srcdir)/pk-map-lex.c -ot $(srcdir)/pk-map-lex.l; } || $(MAKE) generate-pk-map-lex
pk-map-lex.h: pk-map-lex.c
	@{ test -f $(srcdir)/pk-map-lex.h && test ! $(srcdir)/pk-map-lex.h -ot $(srcdir)/pk-map-lex.c; } || $(MAKE) generate-pk-map-lex
BUILT_SOURCES += pk-map-lex.c pk-map-lex.h
MOSTLYCLEANFILES += pk-map-lex.c-tmp pk-map-lex.h-tmp
MAINTAINERCLEANFILES += $(srcdir)/pk-map-lex.c $(srcdir)/pk-map-lex.h
EXTRA_DIST += pk-map-lex.l pk-map-lex.c pk-map-lex.h

AM_YFLAGS = -t --report=state
# The Automake generated .y.c rule is broken: When executed in a VPATH build,
#   - The .c file gets generated in the build directory. But since it requires
#     special tools to rebuild it, we need to distribute it in the tarballs,
#     and by the GNU Coding Standards
#     <https://www.gnu.org/prep/standards/html_node/Makefile-Basics.html>
#     the file should be generated in the source directory.
#   - The #line directives in the .c file refer to a nonexistent file once it
#     has been moved from the build directory to the source directory. This
#     leads to error if 'lcov' is used later.
# Additionally, here we assume GNU Bison and therefore don't need the ylwrap
# script.
# Therefore we override this rule.
# Since this is a rule that produces multiple files, we apply the idiom from
# <https://lists.gnu.org/archive/html/bug-make/2020-09/msg00008.html>, so that
# it works also in parallel 'make'.
generate-pk-map-tab:
	$(AM_V_YACC)$(POKE_BISON) -d $(YFLAGS) $(AM_YFLAGS) $(srcdir)/pk-map-tab.y \
	&& test ':' = '$(POKE_BISON)' || { \
	  sed -e 's|".*/pk-map-tab\.y"|"pk-map-tab.y"|' \
	      -e 's|"pk-map-tab\.tab\.c"|"pk-map-tab.c"|' \
	      -e 's|"pk-map-tab\.tab\.h"|"pk-map-tab.h"|' \
	      < pk-map-tab.tab.c > pk-map-tab.c-tmp \
	  && sed -e 's|".*/pk-map-tab\.y"|"pk-map-tab.y"|' \
	         -e 's|"pk-map-tab\.tab\.h"|"pk-map-tab.h"|' \
	         < pk-map-tab.tab.h > pk-map-tab.h-tmp \
	  && rm -f pk-map-tab.tab.c pk-map-tab.tab.h \
	  && mv pk-map-tab.c-tmp $(srcdir)/pk-map-tab.c \
	  && mv pk-map-tab.h-tmp $(srcdir)/pk-map-tab.h; \
	}
.PHONY: generate-pk-map-tab
# The above rule will generate files with time-stamp order
# pk-map-tab.y <= pk-map-tab.c <= pk-map-tab.h.
pk-map-tab.c: pk-map-tab.y
	@{ test -f $(srcdir)/pk-map-tab.c && test ! $(srcdir)/pk-map-tab.c -ot $(srcdir)/pk-map-tab.y; } || $(MAKE) generate-pk-map-tab
pk-map-tab.h: pk-map-tab.c
	@{ test -f $(srcdir)/pk-map-tab.h && test ! $(srcdir)/pk-map-tab.h -ot $(srcdir)/pk-map-tab.c; } || $(MAKE) generate-pk-map-tab
BUILT_SOURCES += pk-map-tab.c pk-map-tab.h
MOSTLYCLEANFILES += \
  pk-map-tab.tab.c pk-map-tab.tab.h \
  pk-map-tab.c-tmp pk-map-tab.h-tmp \
  pk-map-tab.output
MAINTAINERCLEANFILES += $(srcdir)/pk-map-tab.c $(srcdir)/pk-map-tab.h
EXTRA_DIST += pk-map-tab.y pk-map-tab.c pk-map-tab.h

# Machine interface
if POKE_MI
poke_SOURCES += pk-mi.c pk-mi.h \
                pk-mi-msg.def pk-mi-msg.c pk-mi-msg.h \
                pk-mi-json.c pk-mi-json.h
poke_CFLAGS += $(JSON_C_CFLAGS)
poke_LDADD += $(JSON_C_LIBS)
endif

if HSERVER
  poke_SOURCES += pk-hserver.h pk-hserver.c
  poke_LDADD += -lpthread
endif
# We install pk-hserver.pk even if HSERVER is not enabled.
dist_pkgdata_DATA += pk-hserver.pk

# nodelist is now treated by doc/Makefile.am.
MOSTLYCLEANFILES += nodelist

# End of Makefile.am
