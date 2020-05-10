# tcl.m4 -- Tcl related autoconf macros for GNU poke
#
# This file contains a stripped-down and modified version of the
# tcl.m4 script distributed by the tclconfig project locate at
# https://github.com/tcltk/tclconfig.
#
# Copyright (c) 1999-2000 Ajuba Solutions.
# Copyright (c) 2002-2005 ActiveState Corporation.
# Copyright (c) 2020 Jose E. Marchesi.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

AC_PREREQ(2.57)

AC_DEFUN([POKE_TCLTK],[
  TEA_PATH_TCLCONFIG
  TEA_PATH_TKCONFIG

  if test -n "$TCL_BIN_DIR" -a -n "$TK_BIN_DIR"; then
    has_tcltk=yes

    TEA_LOAD_TCLCONFIG
    TEA_LOAD_TKCONFIG
    TEA_PUBLIC_TCL_HEADERS
    TEA_PUBLIC_TK_HEADERS

    # This should not be necessary
    AC_SUBST([TCL_INCLUDE_SPEC])
    AC_SUBST([TK_INCLUDE_SPEC])
    AC_SUBST([TCL_LIB_SPEC])
    AC_SUBST([TK_LIB_SPEC])
  else
    has_tcltk=no
  fi
])

#------------------------------------------------------------------------
# TEA_PATH_TCLCONFIG --
#
#	Locate the tclConfig.sh file and perform a sanity check on
#	the Tcl compile flags
#
# Arguments:
#	none
#
# Results:
#
#	Adds the following arguments to configure:
#		--with-tcl=...
#
#	Defines the following vars:
#		TCL_BIN_DIR	Full path to the directory containing
#				the tclConfig.sh file
#------------------------------------------------------------------------

AC_DEFUN([TEA_PATH_TCLCONFIG], [
    dnl TEA specific: Make sure we are initialized
    AC_REQUIRE([TEA_INIT])
    #
    # Ok, lets find the tcl configuration
    # First, look for one uninstalled.
    # the alternative search directory is invoked by --with-tcl
    #

    if test x"${no_tcl}" = x ; then
	# we reset no_tcl in case something fails here
	no_tcl=true
	AC_ARG_WITH(tcl,
	    AC_HELP_STRING([--with-tcl],
		[directory containing tcl configuration (tclConfig.sh)]),
	    with_tclconfig="${withval}")
	AC_MSG_CHECKING([for Tcl configuration])
	AC_CACHE_VAL(ac_cv_c_tclconfig,[

	    # First check to see if --with-tcl was specified.
	    if test x"${with_tclconfig}" != x ; then
		case "${with_tclconfig}" in
		    */tclConfig.sh )
			if test -f "${with_tclconfig}"; then
			    AC_MSG_WARN([--with-tcl argument should refer to directory containing tclConfig.sh, not to tclConfig.sh itself])
			    with_tclconfig="`echo "${with_tclconfig}" | sed 's!/tclConfig\.sh$!!'`"
			fi ;;
		esac
		if test -f "${with_tclconfig}/tclConfig.sh" ; then
		    ac_cv_c_tclconfig="`(cd "${with_tclconfig}"; pwd)`"
		else
		    AC_MSG_ERROR([${with_tclconfig} directory doesn't contain tclConfig.sh])
		fi
	    fi

	    # then check for a private Tcl installation
	    if test x"${ac_cv_c_tclconfig}" = x ; then
		for i in \
			../tcl \
			`ls -dr ../tcl[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ../tcl[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../tcl[[8-9]].[[0-9]]* 2>/dev/null` \
			../../tcl \
			`ls -dr ../../tcl[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ../../tcl[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../../tcl[[8-9]].[[0-9]]* 2>/dev/null` \
			../../../tcl \
			`ls -dr ../../../tcl[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ../../../tcl[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../../../tcl[[8-9]].[[0-9]]* 2>/dev/null` ; do
		    if test "${TEA_PLATFORM}" = "windows" \
			    -a -f "$i/win/tclConfig.sh" ; then
			ac_cv_c_tclconfig="`(cd $i/win; pwd)`"
			break
		    fi
		    if test -f "$i/unix/tclConfig.sh" ; then
			ac_cv_c_tclconfig="`(cd $i/unix; pwd)`"
			break
		    fi
		done
	    fi

	    # on Darwin, check in Framework installation locations
	    if test "`uname -s`" = "Darwin" -a x"${ac_cv_c_tclconfig}" = x ; then
		for i in `ls -d ~/Library/Frameworks 2>/dev/null` \
			`ls -d /Library/Frameworks 2>/dev/null` \
			`ls -d /Network/Library/Frameworks 2>/dev/null` \
			`ls -d /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/Library/Frameworks/Tcl.framework 2>/dev/null` \
			`ls -d /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/Network/Library/Frameworks/Tcl.framework 2>/dev/null` \
			`ls -d /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/Tcl.framework 2>/dev/null` \
			; do
		    if test -f "$i/Tcl.framework/tclConfig.sh" ; then
			ac_cv_c_tclconfig="`(cd $i/Tcl.framework; pwd)`"
			break
		    fi
		done
	    fi

	    # TEA specific: on Windows, check in common installation locations
	    if test "${TEA_PLATFORM}" = "windows" \
		-a x"${ac_cv_c_tclconfig}" = x ; then
		for i in `ls -d C:/Tcl/lib 2>/dev/null` \
			`ls -d C:/Progra~1/Tcl/lib 2>/dev/null` \
			; do
		    if test -f "$i/tclConfig.sh" ; then
			ac_cv_c_tclconfig="`(cd $i; pwd)`"
			break
		    fi
		done
	    fi

	    # check in a few common install locations
	    if test x"${ac_cv_c_tclconfig}" = x ; then
		for i in `ls -d ${libdir} 2>/dev/null` \
			`ls -d ${exec_prefix}/lib 2>/dev/null` \
			`ls -d ${prefix}/lib 2>/dev/null` \
			`ls -d /usr/local/lib 2>/dev/null` \
			`ls -d /usr/contrib/lib 2>/dev/null` \
			`ls -d /usr/pkg/lib 2>/dev/null` \
			`ls -d /usr/lib 2>/dev/null` \
			`ls -d /usr/lib64 2>/dev/null` \
			`ls -d /usr/lib/tcl8.6 2>/dev/null` \
			`ls -d /usr/lib/tcl8.5 2>/dev/null` \
			`ls -d /usr/local/lib/tcl8.6 2>/dev/null` \
			`ls -d /usr/local/lib/tcl8.5 2>/dev/null` \
			`ls -d /usr/local/lib/tcl/tcl8.6 2>/dev/null` \
			`ls -d /usr/local/lib/tcl/tcl8.5 2>/dev/null` \
			; do
		    if test -f "$i/tclConfig.sh" ; then
			ac_cv_c_tclconfig="`(cd $i; pwd)`"
			break
		    fi
		done
	    fi

	    # check in a few other private locations
	    if test x"${ac_cv_c_tclconfig}" = x ; then
		for i in \
			${srcdir}/../tcl \
			`ls -dr ${srcdir}/../tcl[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ${srcdir}/../tcl[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ${srcdir}/../tcl[[8-9]].[[0-9]]* 2>/dev/null` ; do
		    if test "${TEA_PLATFORM}" = "windows" \
			    -a -f "$i/win/tclConfig.sh" ; then
			ac_cv_c_tclconfig="`(cd $i/win; pwd)`"
			break
		    fi
		    if test -f "$i/unix/tclConfig.sh" ; then
			ac_cv_c_tclconfig="`(cd $i/unix; pwd)`"
			break
		    fi
		done
	    fi
	])

	if test x"${ac_cv_c_tclconfig}" = x ; then
	    TCL_BIN_DIR=""
	    AC_MSG_RESULT([not found])
	else
	    no_tcl=
	    TCL_BIN_DIR="${ac_cv_c_tclconfig}"
	    AC_MSG_RESULT([found ${TCL_BIN_DIR}/tclConfig.sh])
	fi
    fi
])

#------------------------------------------------------------------------
# TEA_PATH_TKCONFIG --
#
#	Locate the tkConfig.sh file
#
# Arguments:
#	none
#
# Results:
#
#	Adds the following arguments to configure:
#		--with-tk=...
#
#	Defines the following vars:
#		TK_BIN_DIR	Full path to the directory containing
#				the tkConfig.sh file
#------------------------------------------------------------------------

AC_DEFUN([TEA_PATH_TKCONFIG], [
    #
    # Ok, lets find the tk configuration
    # First, look for one uninstalled.
    # the alternative search directory is invoked by --with-tk
    #

    if test x"${no_tk}" = x ; then
	# we reset no_tk in case something fails here
	no_tk=true
	AC_ARG_WITH(tk,
	    AC_HELP_STRING([--with-tk],
		[directory containing tk configuration (tkConfig.sh)]),
	    with_tkconfig="${withval}")
	AC_MSG_CHECKING([for Tk configuration])
	AC_CACHE_VAL(ac_cv_c_tkconfig,[

	    # First check to see if --with-tkconfig was specified.
	    if test x"${with_tkconfig}" != x ; then
		case "${with_tkconfig}" in
		    */tkConfig.sh )
			if test -f "${with_tkconfig}"; then
			    AC_MSG_WARN([--with-tk argument should refer to directory containing tkConfig.sh, not to tkConfig.sh itself])
			    with_tkconfig="`echo "${with_tkconfig}" | sed 's!/tkConfig\.sh$!!'`"
			fi ;;
		esac
		if test -f "${with_tkconfig}/tkConfig.sh" ; then
		    ac_cv_c_tkconfig="`(cd "${with_tkconfig}"; pwd)`"
		else
		    AC_MSG_ERROR([${with_tkconfig} directory doesn't contain tkConfig.sh])
		fi
	    fi

	    # then check for a private Tk library
	    if test x"${ac_cv_c_tkconfig}" = x ; then
		for i in \
			../tk \
			`ls -dr ../tk[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ../tk[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../tk[[8-9]].[[0-9]]* 2>/dev/null` \
			../../tk \
			`ls -dr ../../tk[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ../../tk[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../../tk[[8-9]].[[0-9]]* 2>/dev/null` \
			../../../tk \
			`ls -dr ../../../tk[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ../../../tk[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ../../../tk[[8-9]].[[0-9]]* 2>/dev/null` ; do
		    if test "${TEA_PLATFORM}" = "windows" \
			    -a -f "$i/win/tkConfig.sh" ; then
			ac_cv_c_tkconfig="`(cd $i/win; pwd)`"
			break
		    fi
		    if test -f "$i/unix/tkConfig.sh" ; then
			ac_cv_c_tkconfig="`(cd $i/unix; pwd)`"
			break
		    fi
		done
	    fi

	    # on Darwin, check in Framework installation locations
	    if test "`uname -s`" = "Darwin" -a x"${ac_cv_c_tkconfig}" = x ; then
		for i in `ls -d ~/Library/Frameworks 2>/dev/null` \
			`ls -d /Library/Frameworks 2>/dev/null` \
			`ls -d /Network/Library/Frameworks 2>/dev/null` \
			; do
		    if test -f "$i/Tk.framework/tkConfig.sh" ; then
			ac_cv_c_tkconfig="`(cd $i/Tk.framework; pwd)`"
			break
		    fi
		done
	    fi

	    # check in a few common install locations
	    if test x"${ac_cv_c_tkconfig}" = x ; then
		for i in `ls -d ${libdir} 2>/dev/null` \
			`ls -d ${exec_prefix}/lib 2>/dev/null` \
			`ls -d ${prefix}/lib 2>/dev/null` \
			`ls -d /usr/local/lib 2>/dev/null` \
			`ls -d /usr/contrib/lib 2>/dev/null` \
			`ls -d /usr/pkg/lib 2>/dev/null` \
			`ls -d /usr/lib/tk8.6 2>/dev/null` \
			`ls -d /usr/lib/tk8.5 2>/dev/null` \
			`ls -d /usr/lib 2>/dev/null` \
			`ls -d /usr/lib64 2>/dev/null` \
			`ls -d /usr/local/lib/tk8.6 2>/dev/null` \
			`ls -d /usr/local/lib/tk8.5 2>/dev/null` \
			`ls -d /usr/local/lib/tcl/tk8.6 2>/dev/null` \
			`ls -d /usr/local/lib/tcl/tk8.5 2>/dev/null` \
			; do
		    if test -f "$i/tkConfig.sh" ; then
			ac_cv_c_tkconfig="`(cd $i; pwd)`"
			break
		    fi
		done
	    fi

	    # TEA specific: on Windows, check in common installation locations
	    if test "${TEA_PLATFORM}" = "windows" \
		-a x"${ac_cv_c_tkconfig}" = x ; then
		for i in `ls -d C:/Tcl/lib 2>/dev/null` \
			`ls -d C:/Progra~1/Tcl/lib 2>/dev/null` \
			; do
		    if test -f "$i/tkConfig.sh" ; then
			ac_cv_c_tkconfig="`(cd $i; pwd)`"
			break
		    fi
		done
	    fi

	    # check in a few other private locations
	    if test x"${ac_cv_c_tkconfig}" = x ; then
		for i in \
			${srcdir}/../tk \
			`ls -dr ${srcdir}/../tk[[8-9]].[[0-9]].[[0-9]]* 2>/dev/null` \
			`ls -dr ${srcdir}/../tk[[8-9]].[[0-9]] 2>/dev/null` \
			`ls -dr ${srcdir}/../tk[[8-9]].[[0-9]]* 2>/dev/null` ; do
		    if test "${TEA_PLATFORM}" = "windows" \
			    -a -f "$i/win/tkConfig.sh" ; then
			ac_cv_c_tkconfig="`(cd $i/win; pwd)`"
			break
		    fi
		    if test -f "$i/unix/tkConfig.sh" ; then
			ac_cv_c_tkconfig="`(cd $i/unix; pwd)`"
			break
		    fi
		done
	    fi
	])

	if test x"${ac_cv_c_tkconfig}" = x ; then
	    TK_BIN_DIR=""
	    AC_MSG_RESULT([not found])
	else
	    no_tk=
	    TK_BIN_DIR="${ac_cv_c_tkconfig}"
	    AC_MSG_RESULT([found ${TK_BIN_DIR}/tkConfig.sh])
	fi
    fi
])

#------------------------------------------------------------------------
# TEA_LOAD_TCLCONFIG --
#
#	Load the tclConfig.sh file
#
# Arguments:
#
#	Requires the following vars to be set:
#		TCL_BIN_DIR
#
# Results:
#
#	Substitutes the following vars:
#		TCL_BIN_DIR
#		TCL_SRC_DIR
#		TCL_LIB_FILE
#------------------------------------------------------------------------

AC_DEFUN([TEA_LOAD_TCLCONFIG], [
    AC_MSG_CHECKING([for existence of ${TCL_BIN_DIR}/tclConfig.sh])

    if test -f "${TCL_BIN_DIR}/tclConfig.sh" ; then
        AC_MSG_RESULT([loading])
	. "${TCL_BIN_DIR}/tclConfig.sh"
    else
        AC_MSG_RESULT([could not find ${TCL_BIN_DIR}/tclConfig.sh])
    fi

    # eval is required to do the TCL_DBGX substitution
    eval "TCL_LIB_FILE=\"${TCL_LIB_FILE}\""
    eval "TCL_STUB_LIB_FILE=\"${TCL_STUB_LIB_FILE}\""

    # If the TCL_BIN_DIR is the build directory (not the install directory),
    # then set the common variable name to the value of the build variables.
    # For example, the variable TCL_LIB_SPEC will be set to the value
    # of TCL_BUILD_LIB_SPEC. An extension should make use of TCL_LIB_SPEC
    # instead of TCL_BUILD_LIB_SPEC since it will work with both an
    # installed and uninstalled version of Tcl.
    if test -f "${TCL_BIN_DIR}/Makefile" ; then
        TCL_LIB_SPEC="${TCL_BUILD_LIB_SPEC}"
        TCL_STUB_LIB_SPEC="${TCL_BUILD_STUB_LIB_SPEC}"
        TCL_STUB_LIB_PATH="${TCL_BUILD_STUB_LIB_PATH}"
    elif test "`uname -s`" = "Darwin"; then
	# If Tcl was built as a framework, attempt to use the libraries
	# from the framework at the given location so that linking works
	# against Tcl.framework installed in an arbitrary location.
	case ${TCL_DEFS} in
	    *TCL_FRAMEWORK*)
		if test -f "${TCL_BIN_DIR}/${TCL_LIB_FILE}"; then
		    for i in "`cd "${TCL_BIN_DIR}"; pwd`" \
			     "`cd "${TCL_BIN_DIR}"/../..; pwd`"; do
			if test "`basename "$i"`" = "${TCL_LIB_FILE}.framework"; then
			    TCL_LIB_SPEC="-F`dirname "$i" | sed -e 's/ /\\\\ /g'` -framework ${TCL_LIB_FILE}"
			    break
			fi
		    done
		fi
		if test -f "${TCL_BIN_DIR}/${TCL_STUB_LIB_FILE}"; then
		    TCL_STUB_LIB_SPEC="-L`echo "${TCL_BIN_DIR}"  | sed -e 's/ /\\\\ /g'` ${TCL_STUB_LIB_FLAG}"
		    TCL_STUB_LIB_PATH="${TCL_BIN_DIR}/${TCL_STUB_LIB_FILE}"
		fi
		;;
	esac
    fi

    # eval is required to do the TCL_DBGX substitution
    eval "TCL_LIB_FLAG=\"${TCL_LIB_FLAG}\""
    eval "TCL_LIB_SPEC=\"${TCL_LIB_SPEC}\""
    eval "TCL_STUB_LIB_FLAG=\"${TCL_STUB_LIB_FLAG}\""
    eval "TCL_STUB_LIB_SPEC=\"${TCL_STUB_LIB_SPEC}\""

    AC_SUBST(TCL_VERSION)
    AC_SUBST(TCL_PATCH_LEVEL)
    AC_SUBST(TCL_BIN_DIR)
    AC_SUBST(TCL_SRC_DIR)

    AC_SUBST(TCL_LIB_FILE)
    AC_SUBST(TCL_LIB_FLAG)
    AC_SUBST(TCL_LIB_SPEC)

    AC_SUBST(TCL_STUB_LIB_FILE)
    AC_SUBST(TCL_STUB_LIB_FLAG)
    AC_SUBST(TCL_STUB_LIB_SPEC)

    AC_MSG_CHECKING([platform])
    hold_cc=$CC; CC="$TCL_CC"
    AC_TRY_COMPILE(,[
	    #ifdef _WIN32
		#error win32
	    #endif
	], [
	    # first test we've already retrieved platform (cross-compile), fallback to unix otherwise:
	    TEA_PLATFORM="${TEA_PLATFORM-unix}"
	    CYGPATH=echo
	], [
	    TEA_PLATFORM="windows"
	    AC_CHECK_PROG(CYGPATH, cygpath, cygpath -m, echo)	]
    )
    CC=$hold_cc
    AC_MSG_RESULT($TEA_PLATFORM)

    # The BUILD_$pkg is to define the correct extern storage class
    # handling when making this package
    AC_DEFINE_UNQUOTED(BUILD_${PACKAGE_NAME}, [],
	    [Building extension source?])

    # TEA specific:
    AC_SUBST(TCL_LIBS)
    AC_SUBST(TCL_DEFS)
    AC_SUBST(TCL_EXTRA_CFLAGS)
    AC_SUBST(TCL_LD_FLAGS)
    AC_SUBST(TCL_SHLIB_LD_LIBS)
])

#------------------------------------------------------------------------
# TEA_LOAD_TKCONFIG --
#
#	Load the tkConfig.sh file
#
# Arguments:
#
#	Requires the following vars to be set:
#		TK_BIN_DIR
#
# Results:
#
#	Sets the following vars that should be in tkConfig.sh:
#		TK_BIN_DIR
#------------------------------------------------------------------------

AC_DEFUN([TEA_LOAD_TKCONFIG], [
    AC_MSG_CHECKING([for existence of ${TK_BIN_DIR}/tkConfig.sh])

    if test -f "${TK_BIN_DIR}/tkConfig.sh" ; then
        AC_MSG_RESULT([loading])
	. "${TK_BIN_DIR}/tkConfig.sh"
    else
        AC_MSG_RESULT([could not find ${TK_BIN_DIR}/tkConfig.sh])
    fi

    # eval is required to do the TK_DBGX substitution
    eval "TK_LIB_FILE=\"${TK_LIB_FILE}\""
    eval "TK_STUB_LIB_FILE=\"${TK_STUB_LIB_FILE}\""

    # If the TK_BIN_DIR is the build directory (not the install directory),
    # then set the common variable name to the value of the build variables.
    # For example, the variable TK_LIB_SPEC will be set to the value
    # of TK_BUILD_LIB_SPEC. An extension should make use of TK_LIB_SPEC
    # instead of TK_BUILD_LIB_SPEC since it will work with both an
    # installed and uninstalled version of Tcl.
    if test -f "${TK_BIN_DIR}/Makefile" ; then
        TK_LIB_SPEC="${TK_BUILD_LIB_SPEC}"
        TK_STUB_LIB_SPEC="${TK_BUILD_STUB_LIB_SPEC}"
        TK_STUB_LIB_PATH="${TK_BUILD_STUB_LIB_PATH}"
    elif test "`uname -s`" = "Darwin"; then
	# If Tk was built as a framework, attempt to use the libraries
	# from the framework at the given location so that linking works
	# against Tk.framework installed in an arbitrary location.
	case ${TK_DEFS} in
	    *TK_FRAMEWORK*)
		if test -f "${TK_BIN_DIR}/${TK_LIB_FILE}"; then
		    for i in "`cd "${TK_BIN_DIR}"; pwd`" \
			     "`cd "${TK_BIN_DIR}"/../..; pwd`"; do
			if test "`basename "$i"`" = "${TK_LIB_FILE}.framework"; then
			    TK_LIB_SPEC="-F`dirname "$i" | sed -e 's/ /\\\\ /g'` -framework ${TK_LIB_FILE}"
			    break
			fi
		    done
		fi
		if test -f "${TK_BIN_DIR}/${TK_STUB_LIB_FILE}"; then
		    TK_STUB_LIB_SPEC="-L` echo "${TK_BIN_DIR}"  | sed -e 's/ /\\\\ /g'` ${TK_STUB_LIB_FLAG}"
		    TK_STUB_LIB_PATH="${TK_BIN_DIR}/${TK_STUB_LIB_FILE}"
		fi
		;;
	esac
    fi

    # eval is required to do the TK_DBGX substitution
    eval "TK_LIB_FLAG=\"${TK_LIB_FLAG}\""
    eval "TK_LIB_SPEC=\"${TK_LIB_SPEC}\""
    eval "TK_STUB_LIB_FLAG=\"${TK_STUB_LIB_FLAG}\""
    eval "TK_STUB_LIB_SPEC=\"${TK_STUB_LIB_SPEC}\""

    # TEA specific: Ensure windowingsystem is defined
    if test "${TEA_PLATFORM}" = "unix" ; then
	case ${TK_DEFS} in
	    *MAC_OSX_TK*)
		AC_DEFINE(MAC_OSX_TK, 1, [Are we building against Mac OS X TkAqua?])
		TEA_WINDOWINGSYSTEM="aqua"
		;;
	    *)
		TEA_WINDOWINGSYSTEM="x11"
		;;
	esac
    elif test "${TEA_PLATFORM}" = "windows" ; then
	TEA_WINDOWINGSYSTEM="win32"
    fi

    AC_SUBST(TK_VERSION)
    AC_SUBST(TK_BIN_DIR)
    AC_SUBST(TK_SRC_DIR)

    AC_SUBST(TK_LIB_FILE)
    AC_SUBST(TK_LIB_FLAG)
    AC_SUBST(TK_LIB_SPEC)

    AC_SUBST(TK_STUB_LIB_FILE)
    AC_SUBST(TK_STUB_LIB_FLAG)
    AC_SUBST(TK_STUB_LIB_SPEC)

    # TEA specific:
    AC_SUBST(TK_LIBS)
    AC_SUBST(TK_XINCLUDES)
])

#------------------------------------------------------------------------
# TEA_PUBLIC_TCL_HEADERS --
#
#	Locate the installed public Tcl header files
#
# Arguments:
#	None.
#
# Requires:
#	CYGPATH must be set
#
# Results:
#
#	Adds a --with-tclinclude switch to configure.
#	Result is cached.
#
#	Substitutes the following vars:
#		TCL_INCLUDES
#------------------------------------------------------------------------

AC_DEFUN([TEA_PUBLIC_TCL_HEADERS], [
    AC_MSG_CHECKING([for Tcl public headers])

    AC_ARG_WITH(tclinclude, [  --with-tclinclude       directory containing the public Tcl header files], with_tclinclude=${withval})

    AC_CACHE_VAL(ac_cv_c_tclh, [
	# Use the value from --with-tclinclude, if it was given

	if test x"${with_tclinclude}" != x ; then
	    if test -f "${with_tclinclude}/tcl.h" ; then
		ac_cv_c_tclh=${with_tclinclude}
	    else
		AC_MSG_ERROR([${with_tclinclude} directory does not contain tcl.h])
	    fi
	else
	    list=""
	    if test "`uname -s`" = "Darwin"; then
		# If Tcl was built as a framework, attempt to use
		# the framework's Headers directory
		case ${TCL_DEFS} in
		    *TCL_FRAMEWORK*)
			list="`ls -d ${TCL_BIN_DIR}/Headers 2>/dev/null`"
			;;
		esac
	    fi

	    # Look in the source dir only if Tcl is not installed,
	    # and in that situation, look there before installed locations.
	    if test -f "${TCL_BIN_DIR}/Makefile" ; then
		list="$list `ls -d ${TCL_SRC_DIR}/generic 2>/dev/null`"
	    fi

	    # Check order: pkg --prefix location, Tcl's --prefix location,
	    # relative to directory of tclConfig.sh.

	    eval "temp_includedir=${includedir}"
	    list="$list \
		`ls -d ${temp_includedir}        2>/dev/null` \
		`ls -d ${TCL_PREFIX}/include     2>/dev/null` \
		`ls -d ${TCL_BIN_DIR}/../include 2>/dev/null`"
	    if test "${TEA_PLATFORM}" != "windows" -o "$GCC" = "yes"; then
		list="$list /usr/local/include /usr/include"
		if test x"${TCL_INCLUDE_SPEC}" != x ; then
		    d=`echo "${TCL_INCLUDE_SPEC}" | sed -e 's/^-I//'`
		    list="$list `ls -d ${d} 2>/dev/null`"
		fi
	    fi
	    for i in $list ; do
		if test -f "$i/tcl.h" ; then
		    ac_cv_c_tclh=$i
		    break
		fi
	    done
	fi
    ])

    # Print a message based on how we determined the include path

    if test x"${ac_cv_c_tclh}" = x ; then
	AC_MSG_ERROR([tcl.h not found.  Please specify its location with --with-tclinclude])
    else
	AC_MSG_RESULT([${ac_cv_c_tclh}])
    fi

    # Convert to a native path and substitute into the output files.

    INCLUDE_DIR_NATIVE=`${CYGPATH} ${ac_cv_c_tclh}`

    TCL_INCLUDES=-I\"${INCLUDE_DIR_NATIVE}\"

    AC_SUBST(TCL_INCLUDES)
])

#------------------------------------------------------------------------
# TEA_PUBLIC_TK_HEADERS --
#
#	Locate the installed public Tk header files
#
# Arguments:
#	None.
#
# Requires:
#	CYGPATH must be set
#
# Results:
#
#	Adds a --with-tkinclude switch to configure.
#	Result is cached.
#
#	Substitutes the following vars:
#		TK_INCLUDES
#------------------------------------------------------------------------

AC_DEFUN([TEA_PUBLIC_TK_HEADERS], [
    AC_MSG_CHECKING([for Tk public headers])

    AC_ARG_WITH(tkinclude, [  --with-tkinclude        directory containing the public Tk header files], with_tkinclude=${withval})

    AC_CACHE_VAL(ac_cv_c_tkh, [
	# Use the value from --with-tkinclude, if it was given

	if test x"${with_tkinclude}" != x ; then
	    if test -f "${with_tkinclude}/tk.h" ; then
		ac_cv_c_tkh=${with_tkinclude}
	    else
		AC_MSG_ERROR([${with_tkinclude} directory does not contain tk.h])
	    fi
	else
	    list=""
	    if test "`uname -s`" = "Darwin"; then
		# If Tk was built as a framework, attempt to use
		# the framework's Headers directory.
		case ${TK_DEFS} in
		    *TK_FRAMEWORK*)
			list="`ls -d ${TK_BIN_DIR}/Headers 2>/dev/null`"
			;;
		esac
	    fi

	    # Look in the source dir only if Tk is not installed,
	    # and in that situation, look there before installed locations.
	    if test -f "${TK_BIN_DIR}/Makefile" ; then
		list="$list `ls -d ${TK_SRC_DIR}/generic 2>/dev/null`"
	    fi

	    # Check order: pkg --prefix location, Tk's --prefix location,
	    # relative to directory of tkConfig.sh, Tcl's --prefix location,
	    # relative to directory of tclConfig.sh.

	    eval "temp_includedir=${includedir}"
	    list="$list \
		`ls -d ${temp_includedir}        2>/dev/null` \
		`ls -d ${TK_PREFIX}/include      2>/dev/null` \
		`ls -d ${TK_BIN_DIR}/../include  2>/dev/null` \
		`ls -d ${TCL_PREFIX}/include     2>/dev/null` \
		`ls -d ${TCL_BIN_DIR}/../include 2>/dev/null`"
	    if test "${TEA_PLATFORM}" != "windows" -o "$GCC" = "yes"; then
		list="$list /usr/local/include /usr/include"
		if test x"${TK_INCLUDE_SPEC}" != x ; then
		    d=`echo "${TK_INCLUDE_SPEC}" | sed -e 's/^-I//'`
		    list="$list `ls -d ${d} 2>/dev/null`"
		fi
	    fi
	    for i in $list ; do
		if test -f "$i/tk.h" ; then
		    ac_cv_c_tkh=$i
		    break
		fi
	    done
	fi
    ])

    # Print a message based on how we determined the include path

    if test x"${ac_cv_c_tkh}" = x ; then
	AC_MSG_ERROR([tk.h not found.  Please specify its location with --with-tkinclude])
    else
	AC_MSG_RESULT([${ac_cv_c_tkh}])
    fi

    # Convert to a native path and substitute into the output files.

    INCLUDE_DIR_NATIVE=`${CYGPATH} ${ac_cv_c_tkh}`

    TK_INCLUDES=-I\"${INCLUDE_DIR_NATIVE}\"

    AC_SUBST(TK_INCLUDES)

    if test "${TEA_WINDOWINGSYSTEM}" != "x11"; then
	# On Windows and Aqua, we need the X compat headers
	AC_MSG_CHECKING([for X11 header files])
	if test ! -r "${INCLUDE_DIR_NATIVE}/X11/Xlib.h"; then
	    INCLUDE_DIR_NATIVE="`${CYGPATH} ${TK_SRC_DIR}/xlib`"
	    TK_XINCLUDES=-I\"${INCLUDE_DIR_NATIVE}\"
	    AC_SUBST(TK_XINCLUDES)
	fi
	AC_MSG_RESULT([${INCLUDE_DIR_NATIVE}])
    fi
])

##
## Here ends the standard Tcl configuration bits and starts the
## TEA specific functions
##

#------------------------------------------------------------------------
# TEA_INIT --
#
#	Init various Tcl Extension Architecture (TEA) variables.
#	This should be the first called TEA_* macro.
#
# Arguments:
#	none
#
# Results:
#
#	Defines and substs the following vars:
#		CYGPATH
#	Defines only:
#		TEA_INITED
#		TEA_PLATFORM (windows or unix)
#
# "cygpath" is used on windows to generate native path names for include
# files. These variables should only be used with the compiler and linker
# since they generate native path names.
#------------------------------------------------------------------------

AC_DEFUN([TEA_INIT], [
    # If the user did not set CFLAGS, set it now to keep macros
    # like AC_PROG_CC and AC_TRY_COMPILE from adding "-g -O2".
    if test "${CFLAGS+set}" != "set" ; then
	CFLAGS=""
    fi

    case "`uname -s`" in
	*win32*|*WIN32*|*MINGW32_*|*MINGW64_*|*MSYS_*)
	    AC_CHECK_PROG(CYGPATH, cygpath, cygpath -m, echo)
	    TEA_PLATFORM="windows"
	    ;;
	*CYGWIN_*)
	    # CYGPATH and TEA_PLATFORM are determined later in LOAD_TCLCONFIG
	    ;;
	*)
	    CYGPATH=echo
	    # Maybe we are cross-compiling....
	    case ${host_alias} in
		*mingw32*)
		TEA_PLATFORM="windows"
		;;
	    *)
		TEA_PLATFORM="unix"
		;;
	    esac
	    ;;
    esac

    # Check if exec_prefix is set. If not use fall back to prefix.
    # Note when adjusted, so that TEA_PREFIX can correct for this.
    # This is needed for recursive configures, since autoconf propagates
    # $prefix, but not $exec_prefix (doh!).
    if test x$exec_prefix = xNONE ; then
	exec_prefix_default=yes
	exec_prefix=$prefix
    fi

    AC_MSG_NOTICE([configuring ${PACKAGE_NAME} ${PACKAGE_VERSION}])

    AC_SUBST(CYGPATH)

    # This package name must be replaced statically for AC_SUBST to work
    AC_SUBST(PKG_LIB_FILE)
    # Substitute STUB_LIB_FILE in case package creates a stub library too.
    AC_SUBST(PKG_STUB_LIB_FILE)

    # We AC_SUBST these here to ensure they are subst'ed,
    # in case the user doesn't call TEA_ADD_...
    AC_SUBST(PKG_STUB_SOURCES)
    AC_SUBST(PKG_STUB_OBJECTS)
    AC_SUBST(PKG_TCL_SOURCES)
    AC_SUBST(PKG_HEADERS)
    AC_SUBST(PKG_INCLUDES)
    AC_SUBST(PKG_LIBS)
    AC_SUBST(PKG_CFLAGS)
])

# Local Variables:
# mode: autoconf
# End:
