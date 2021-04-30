dnl configure.ac for GNU poke
dnl
dnl Please process this file with autoconf to get a 'configure'
dnl script.

dnl Copyright (C) 2019, 2020, 2021 Jose E. Marchesi

dnl This program is free software: you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation, either version 3 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see <http://www.gnu.org/licenses/>.

AC_INIT([GNU poke], [1.2], [poke-devel@gnu.org], [poke],
        [http://www.jemarch.net/poke.html])

AC_CONFIG_AUX_DIR([build-aux])
dnl In order to have a custom INSTALL file, we need Automake option 'foreign'.
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS(poke/config.h)
AC_CONFIG_MACRO_DIRS([m4])

# Include the Autoconf macros from Jitter.
m4_include([m4/jitter.m4])


dnl Autobuild
dnl AB_INIT

AC_DEFINE([PACKAGE_BUGZILLA],["https://sourceware.org/bugzilla/describecomponents.cgi?product=poke"],[URL for entering bugs in the project's bugzilla])

AC_PROG_CC
gl_EARLY
libpoke_EARLY
gui_EARLY

LT_INIT
AC_PROG_CC_C99
AM_PROG_CC_C_O

AC_ARG_VAR([LEX], [The flex implementation to use.])
if test -n "$LEX"; then
  AC_MSG_CHECKING([for flex])
  AC_MSG_RESULT([$LEX])
else
  AC_PATH_PROG([LEX], [flex], [:])
fi

gl_PROG_BISON([POKE_BISON],[3.6])

dnl System
AC_CANONICAL_HOST
canonical=$host

dnl The proc IOD is currently Linux-specific, since it relies on the
dnl availability of the /proc file-system.

case "${host_os}" in
  linux*)
    have_proc=yes ;;
  *)
    have_proc=no ;;
esac

if test "x$have_proc" = "xyes"; then
  AC_DEFINE([HAVE_PROC], 1,
            [Defined if the host system features a proc directory.])
fi
AM_CONDITIONAL([HAVE_PROC], [test "x$have_proc" = "xyes"])

gl_INIT
libpoke_INIT
gui_INIT

# Try to find a libtextstyle with hyperlinks support.
gl_LIBTEXTSTYLE_OPTIONAL([0.20.5])

# GNU help2man creates man pages from --help output; in many cases, this
# is sufficient, and obviates the need to maintain man pages separately.
# However, this means invoking executables, which we generally cannot do
# when cross-compiling, so we test to avoid that (the variable
# "cross_compiling" is set by AC_PROG_CC).
if test $cross_compiling = no; then
  AM_MISSING_PROG(HELP2MAN, help2man)
else
  HELP2MAN=:
fi

dnl recfix is part of the GNU recutils suite, and is used in cfg.mk to
dnl implement a syntax-check for etc/poke.rec.  If recfix is not
dnl present we set RECFIX to : so the rule wont fail.
AC_CHECK_PROGS([RECFIX],[recfix],[:])

dnl i18n with gettext
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

dnl RAS requires a version of awk that supports gensub().  However,
dnl since we are including the RAS-generated .pkc files in the
dnl tarball, this should not be a hard dependency here: people
dnl building poke from released tarballs are not required to have any
dnl particular version of awk installed.  We therefore set RAS_AWK
dnl here, using the value ':' for when no suitable awk is found.  Then
dnl in the `.pkc : .pks' rule in Makefile.am an error message is
dnl emitted in case no proper AWK is present.

AX_CHECK_AWK_GENSUB([RAS_AWK=$AWK], [RAS_AWK=:])
AC_SUBST([RAS_AWK])

dnl The Boehm-Weiser garbage collector

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES(BDW_GC,[bdw-gc],[have_gc=yes],[have_gc=no])

dnl The JSON-C library for the machine-interface

AC_ARG_ENABLE([mi],
              AS_HELP_STRING([--enable-mi],
                             [Enable the poke Machine-Interface (default is YES)]),
              [mi_enabled=$enableval], [mi_enabled=yes])

if test "x$mi_enabled" = "xyes"; then
  PKG_CHECK_MODULES(JSON_C,[json-c],[],[mi_enabled=no])
fi

AM_CONDITIONAL([POKE_MI], [test "x$mi_enabled" = "xyes"])
if test "x$mi_enabled" = "xyes"; then
  AC_DEFINE([POKE_MI], 1,
            [Defined if building poke with the machine-interface enabled])
fi

dnl Jitter

AC_JITTER_SUBPACKAGE([jitter])

dnl Profiling in the PVM

AC_ARG_ENABLE([pvm-profiling],
              AS_HELP_STRING([--enable-pvm-profiling],
                             [Enable profiling in the PVM (default is NO)]),
              [profiling_enabled=$enableval], [profiling_enabled=no])

if test "x$profiling_enabled" = "xyes"; then
  AC_DEFINE([PVM_PROFILE_COUNT], [1], [use count-based profiling in the PVM])
  AC_DEFINE([PVM_PROFILE_SAMPLE], [1], [use sample-based profiling in the PVM])
fi

dnl libnbd for nbd:// io spaces (optional). Testing it also requires
dnl nbdkit

AC_ARG_ENABLE([libnbd],
              AS_HELP_STRING([--enable-libnbd],
                             [Enable building with NBD support (default is YES)]),
              [libnbd_enabled=$enableval], [libnbd_enabled=yes])
if test "x$libnbd_enabled" = "xyes"; then
  PKG_CHECK_MODULES([LIBNBD], [libnbd], [
    AC_SUBST([LIBNBD_CFLAGS])
    AC_SUBST([LIBNBD_LIBS])
    AC_DEFINE([HAVE_LIBNBD], [1], [libnbd found at compile time])
    AC_PATH_PROGS([NBDKIT], [nbdkit], [no], [$PATH:/usr/local/sbin:/usr/sbin])
  ], [libnbd_enabled=no NBDKIT=no])
else
  NBDKIT=no
fi
AM_CONDITIONAL([NBD], [test "x$libnbd_enabled" = "xyes"])

dnl Used in Makefile.am.  See the note there.
WITH_JITTER=$with_jitter
AC_SUBST([WITH_JITTER])

dnl We need to determine the endianness of the host system.  The
dnl following macro is also supposed to work when cross-compiling.

AC_C_BIGENDIAN

dnl The following M4 macro from gnulib sets HOST_CPU_C_ABI_32BIT to
dnl 'yes' if the C language ABI is a 32-bit one, to 'no' if it is
dnl 64-bit, or to 'unknown'.
dnl
dnl We set POKE_HOST_32BIT accordingly, and warn if the 32bitness of
dnl the host cannot be determined.

gl_HOST_CPU_C_ABI_32BIT
case "$HOST_CPU_C_ABI_32BIT" in
  yes)
    AC_DEFINE([POKE_HOST_32BIT],[1],[whether the host C ABI is 32-bit]);;
  unknown)
    AC_MSG_WARN([unable to determine whether the host C ABI is 32-bit]);;
  *) ;;
esac

dnl Use libtextstyle if available.  Otherwise, use the dummy header
dnl file provided by gnulib's libtextstyle-optional module.

gl_LIBTEXTSTYLE_OPTIONAL
AX_LIBTEXTSTYLE_HYPERLINK

if test $HAVE_LIBTEXTSTYLE = yes; then
    AC_CHECK_LIB([textstyle], [term_styled_ostream_get_destination], [
      AC_DEFINE([HAVE_TEXTSTYLE_ACCESSORS_SUPPORT], 1,
                [Defined if libtextstyle has support for terminal hyperlinks])
    ])
fi

dnl The hyperlinks server is optional.  It is enabled by default if
dnl there is a libtextstyle with hyperlinks support available.

AC_ARG_ENABLE([hserver],
              AS_HELP_STRING([--enable-hserver],
                             [Enable the terminal hyperlinks server (default is YES)]),
              [hserver_enabled=$enableval], [hserver_enabled=$has_hyper_support])

if test "x$has_hyper_support" = "xno"; then
  hserver_enabled=no
fi

AM_CONDITIONAL([HSERVER], [test "x$hserver_enabled" = "xyes"])
if test "x$hserver_enabled" = "xyes"; then
  AC_DEFINE([HAVE_HSERVER], 1,
            [Defined if the hyperlinks server is available])
fi

dnl The GUI is optional, and depends on the availability of Tcl and
dnl Tk.

AC_ARG_ENABLE([gui],
              AS_HELP_STRING([--enable-gui],
                             [Enable the GUI (default is YES)]),
              [gui_enabled=$enableval], [gui_enabled=yes])

if test "x$gui_enabled" = "xyes"; then
 POKE_TCLTK
 gui_enabled=$has_tcltk
fi

AM_CONDITIONAL([GUI], [test "x$gui_enabled" = "xyes"])

dnl The test framework is optional.
AC_CHECK_HEADERS([dejagnu.h])
AM_CONDITIONAL([HAVE_DEJAGNU], [test "$ac_cv_header_dejagnu_h" = yes])

dnl editor syntax definitions
AC_ARG_WITH([vimdir],
 [AS_HELP_STRING([--with-vimdir],
                 [override the default vim directory])],
 [ vimdir="$withval"
   AC_MSG_CHECKING([where vim files should go])
   AC_MSG_RESULT([$vimdir])],
 [ vimdir='${datadir}/vim/vimfiles' ]
)
AC_SUBST([vimdir])

AM_PATH_LISPDIR

dnl Generate output files
AC_CONFIG_FILES(Makefile
                gl/Makefile
                gl-libpoke/Makefile
                gl-gui/Makefile
                libpoke/Makefile
                poke/Makefile
                gui/Makefile
                utils/Makefile
                pickles/Makefile
                maps/Makefile
                doc/Makefile
                man/Makefile
                po/Makefile.in
                etc/Makefile
                etc/vim/Makefile
                testsuite/Makefile
                testsuite/poke.libpoke/Makefile
                testsuite/poke.mi-json/Makefile)
AC_CONFIG_FILES([run],
                [chmod +x,-w run])

dnl pkg-config description
AX_CREATE_PKGCONFIG_INFO('libpoke/poke.pc', , , 'libpoke')

AC_OUTPUT

dnl Report warnings

if test "x$HAVE_LIBTEXTSTYLE" = "xno"; then
   AC_MSG_WARN([libtextstyle was not found in the system. Poke's output won't be styled.])
fi

if test "x$hserver_enabled" = "xno"; then
   AC_MSG_WARN([building poke without terminal hyperlinks server support.
     Install a hyperlinks-capable libtextstyle and use --enable-hserver to activate it.])
fi

if test "x$libnbd_enabled" != "xyes"; then
   AC_MSG_WARN([building poke without NBD io space support.
     Install libnbd to use it.])
fi

if test "x$mi_enabled" = "xno"; then
   AC_MSG_WARN([building poke without the machine interface support.
     Install libjson-c and use --enable-mi to activate it.])
fi

if test "x$gui_enabled" = "xno"; then
   AC_MSG_WARN([building poke without GUI support.
     Install tcl-dev and tk-dev and use --enable-gui to activate it.])
fi

dnl Report errors

if test "x$have_gc" = "xno"; then
   AC_MSG_ERROR([can't find the Boehm GC library.  Please install it.])
fi

if test "x$gl_cv_lib_readline" = "xno"; then
   AC_MSG_ERROR([can't find an usable libreadline.  Please install one.])
fi

dnl End of configure.ac
