#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.50])
AC_INIT([sps-alsa-explore], [1.2.1-dev], [4265913+mikebrady@users.noreply.github.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([sps-alsa-explore.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

PKG_PROG_PKG_CONFIG([0.9.0])

AC_CHECK_PROGS([GIT], [git])
if test -n "$GIT" && test -e ".git/index" ; then
  AC_DEFINE([CONFIG_USE_GIT_VERSION_STRING], 1, [Use the version string produced by running 'git describe --tags --dirty --broken'.])
fi
AM_CONDITIONAL([USE_GIT_VERSION], [test -n "$GIT" && test -e ".git/index" ])

# Checks for libraries.
AC_CHECK_LIB([m],[exp], , AC_MSG_ERROR(maths library needed))
PKG_CHECK_MODULES([ALSA], [alsa], [LIBS="${ALSA_LIBS} ${LIBS}"])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h math.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([memset rint strchr strncasecmp strtol])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
