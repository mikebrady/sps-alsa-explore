#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.50])
AC_INIT([alsaexplore], [1.0.3d4], [4265913+mikebrady@users.noreply.github.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([alsaexplore.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

PKG_PROG_PKG_CONFIG([0.9.0])

# Checks for libraries.
PKG_CHECK_MODULES([ALSA], [alsa], [LIBS="${ALSA_LIBS} ${LIBS}"])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([memset rint strchr strncasecmp strtol])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
