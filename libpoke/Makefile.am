# GNU poke - Makefile.am for libpoke

# Copyright (C) 2019, 2020, 2021 Jose E. Marchesi

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

dist_pkgdata_DATA = pkl-rt.pk std.pk
dist_pkgconfig_lib_DATA = $(pkgconfig_libfile)

lib_LTLIBRARIES = libpoke.la

include_HEADERS = libpoke.h

libpoke_la_SOURCES = libpoke.h libpoke.c \
                     pk-val.c \
                     pkt.h \
                     pkl.h pkl.c \
                     pkl-ast.h pkl-ast.c \
                     pkl-compiler.h \
                     pkl-env.h pkl-env.c \
                     pkl-pass.h pkl-pass.c \
                     pkl-promo.h pkl-promo.c \
                     pkl-fold.h pkl-fold.c \
                     pkl-typify.h pkl-typify.c \
                     pkl-anal.h pkl-anal.c \
                     pkl-trans.h pkl-trans.c \
                     pkl-tab.h pkl-tab.c pkl-lex.l \
                     pkl-gen.h pkl-gen.c \
                     pkl-asm.h pkl-asm.c \
                     pkl-diag.h pkl-diag.c \
                     pkl-parser.h pkl-parser.c \
                     pkl-gen.pks pkl-asm.pks \
                     pkl-gen.pkc pkl-asm.pkc \
                     pkl-insn.def pkl-ops.def pkl-attrs.def \
                     pvm.h pvm.c \
                     pvm-val.c pvm-val.h \
                     pvm-env.c \
                     pvm-alloc.h pvm-alloc.c \
                     pvm-program.h pvm-program.c \
                     pvm-program-point.h \
                     pvm.jitter \
                     ios.c ios.h ios-dev.h \
                     ios-dev-file.c ios-dev-mem.c \
                     ios-dev-zero.c ios-dev-sub.c \
                     ios-buffer.h ios-buffer.c \
                     ios-dev-stream.c

libpoke_la_SOURCES += ../common/pk-utils.c ../common/pk-utils.h

if NBD
libpoke_la_SOURCES += ios-dev-nbd.c
endif NBD

if HAVE_PROC
libpoke_la_SOURCES += ios-dev-proc.c
endif HAVE_PROC

# *.pkc files are generated from *.pks, by using ras and pkl-insn.def.
# Generate them in $(srcdir), since they are distributed in tarballs
# (see <https://www.gnu.org/prep/standards/html_node/Makefile-Basics.html>).
.pks.pkc:
	if test "$(RAS_AWK)" = ":"; then \
	  echo "your AWK is not powerful enough.  Please install gawk" \
          1>&2 && exit 1 || : ; \
	fi
	outfile=`basename "$@"`; \
	srcdir=$(srcdir) $(RAS_AWK) -f $(srcdir)/ras $< > $$outfile.tmp \
	&& mv $$outfile.tmp $(srcdir)/$$outfile

pkl-gen.pkc pkl-asm.pkc: $(srcdir)/ras $(srcdir)/pkl-insn.def
MOSTLYCLEANFILES += pkl-gen.pkc.tmp pkl-asm.pkc.tmp

EXTRA_DIST = ras

BUILT_SOURCES = pkl-gen.pkc pkl-asm.pkc

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
generate-pkl-lex:
	$(AM_V_LEX)$(LEX) $(LFLAGS) $(AM_LFLAGS) -t $(srcdir)/pkl-lex.l > pkl-lex.c \
	&& test ':' = '$(LEX)' || { \
	  sed -e 's|".*/pkl-lex\.l"|"pkl-lex.l"|' \
	      -e 's|"lex\.yy\.c"|"pkl-lex.c"|' \
	      < pkl-lex.c > pkl-lex.c-tmp \
	  && sed -e 's|".*/pkl-lex\.l"|"pkl-lex.l"|' \
	         < pkl-lex.h > pkl-lex.h-tmp \
	  && rm -f pkl-lex.c pkl-lex.h \
	  && mv pkl-lex.c-tmp $(srcdir)/pkl-lex.c \
	  && mv pkl-lex.h-tmp $(srcdir)/pkl-lex.h; \
	}
.PHONY: generate-pkl-lex
# The above rule will generate files with time-stamp order
# pkl-lex.l <= pkl-lex.c <= pkl-lex.h.
pkl-lex.c: pkl-lex.l
	@{ test -f $(srcdir)/pkl-lex.c && test ! $(srcdir)/pkl-lex.c -ot $(srcdir)/pkl-lex.l; } || $(MAKE) generate-pkl-lex
pkl-lex.h: pkl-lex.c
	@{ test -f $(srcdir)/pkl-lex.h && test ! $(srcdir)/pkl-lex.h -ot $(srcdir)/pkl-lex.c; } || $(MAKE) generate-pkl-lex
BUILT_SOURCES += pkl-lex.c pkl-lex.h
MOSTLYCLEANFILES += pkl-lex.c-tmp pkl-lex.h-tmp
MAINTAINERCLEANFILES += $(srcdir)/pkl-lex.c $(srcdir)/pkl-lex.h
EXTRA_DIST += pkl-lex.l pkl-lex.c pkl-lex.h

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
generate-pkl-tab:
	$(AM_V_YACC)$(POKE_BISON) -d $(YFLAGS) $(AM_YFLAGS) $(srcdir)/pkl-tab.y \
	&& test ':' = '$(POKE_BISON)' || { \
	  sed -e 's|".*/pkl-tab\.y"|"pkl-tab.y"|' \
	      -e 's|"pkl-tab\.tab\.c"|"pkl-tab.c"|' \
	      -e 's|"pkl-tab\.tab\.h"|"pkl-tab.h"|' \
	      < pkl-tab.tab.c > pkl-tab.c-tmp \
	  && sed -e 's|".*/pkl-tab\.y"|"pkl-tab.y"|' \
	         -e 's|"pkl-tab\.tab\.h"|"pkl-tab.h"|' \
	         < pkl-tab.tab.h > pkl-tab.h-tmp \
	  && rm -f pkl-tab.tab.c pkl-tab.tab.h \
	  && mv pkl-tab.c-tmp $(srcdir)/pkl-tab.c \
	  && mv pkl-tab.h-tmp $(srcdir)/pkl-tab.h; \
	}
.PHONY: generate-pkl-tab
# The above rule will generate files with time-stamp order
# pkl-tab.y <= pkl-tab.c <= pkl-tab.h.
pkl-tab.c: pkl-tab.y
	@{ test -f $(srcdir)/pkl-tab.c && test ! $(srcdir)/pkl-tab.c -ot $(srcdir)/pkl-tab.y; } || $(MAKE) generate-pkl-tab
pkl-tab.h: pkl-tab.c
	@{ test -f $(srcdir)/pkl-tab.h && test ! $(srcdir)/pkl-tab.h -ot $(srcdir)/pkl-tab.c; } || $(MAKE) generate-pkl-tab
BUILT_SOURCES += pkl-tab.c pkl-tab.h
MOSTLYCLEANFILES += \
  pkl-tab.tab.c pkl-tab.tab.h \
  pkl-tab.c-tmp pkl-tab.h-tmp \
  pkl-tab.output
MAINTAINERCLEANFILES += $(srcdir)/pkl-tab.c $(srcdir)/pkl-tab.h
EXTRA_DIST += pkl-tab.y pkl-tab.c pkl-tab.h

# Libtool's library version information for libpoke.
# See the libtool documentation, section "Library interface versions".
# Before making a release, use gnulib/build-aux/libtool-next-version.
LTV_CURRENT=0
LTV_REVISION=0
LTV_AGE=0

libpoke_la_CPPFLAGS = -I$(top_builddir)/gl-libpoke -I$(top_srcdir)/gl-libpoke \
                      -I$(top_srcdir)/common \
                      -DPKL_DEBUG \
                      -DPKGDATADIR=\"$(pkgdatadir)\" \
                      -DPKGINFODIR=\"$(infodir)\" \
                      -DLOCALEDIR=\"$(localedir)\" \
                      $(CFLAG_VISIBILITY) \
                      -DBUILDING_LIBPOKE
libpoke_la_CFLAGS = -Wall $(BDW_GC_CFLAGS) $(LIBNBD_CFLAGS)
libpoke_la_LIBADD = ../gl-libpoke/libgnu.la libpvmjitter.la \
                    $(BDW_GC_LIBS) \
                    $(LIBNBD_LIBS)
libpoke_la_LDFLAGS = -version-info $(LTV_CURRENT):$(LTV_REVISION):$(LTV_AGE) \
                     -lc -no-undefined

# Delete a few files created by AX_CREATE_PKGCONFIG_INFO at make
# distclean.

MOSTLYCLEANFILES += \
   poke-uninstalled.pc \
   poke.pc \
   poke-uninstalled.sh \
   poke.pc.in

# Integration with jitter.

noinst_LTLIBRARIES = libpvmjitter.la
libpvmjitter_la_SOURCES = pvm-vm.h pvm-vm1.c pvm-vm2.c
libpvmjitter_la_CPPFLAGS = $(libpoke_la_CPPFLAGS) $(JITTER_CPPFLAGS)
libpvmjitter_la_CFLAGS = $(libpoke_la_CFLAGS) $(JITTER_CFLAGS)
libpvmjitter_la_LDFLAGS = $(JITTER_LDFLAGS)
libpvmjitter_la_LIBADD = $(JITTER_LIBADD)

libpoke_la_CPPFLAGS += $(JITTER_CPPFLAGS)

BUILT_SOURCES += pvm-vm.h pvm-vm1.c pvm-vm2.c
MAINTAINERCLEANFILES += $(srcdir)/pvm-vm.h $(srcdir)/pvm-vm1.c $(srcdir)/pvm-vm2.c

pvm-vm.h pvm-vm1.c pvm-vm2.c: pvm.jitter
	$(JITTER) $(srcdir)/pvm.jitter --output $(builddir) \
	&& { test $(builddir) = $(srcdir) \
	     || { mv pvm-vm.h $(srcdir)/pvm-vm.h \
	          && mv pvm-vm1.c $(srcdir)/pvm-vm1.c \
	          && mv pvm-vm2.c $(srcdir)/pvm-vm2.c; \
	        }; \
	   } \
	|| { rm -f pvm-vm.h pvm-vm1.c pvm-vm2.c; false; }

# End of Makefile.am
