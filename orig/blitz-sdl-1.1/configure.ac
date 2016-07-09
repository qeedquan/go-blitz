#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([blitz-sdl],[1.1],[craig.jmcp@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_CONFIG_SRCDIR([src/blitz-sdl.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_CHECK_PROG([SDL_CFLAGS], [sdl-config], [`sdl-config --cflags`])
AC_CHECK_PROG([SDL_LIBS], [sdl-config], [`sdl-config --libs`])

if test -z "$SDL_CFLAGS"; then
	AC_MSG_ERROR([sdl-config --cflags did not return expected flags.])
fi

# Checks for libraries.
AC_CHECK_LIB([SDL], [SDL_GetError])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h SDL/SDL.h])

AC_CONFIG_FILES([Makefile src/Makefile data/Makefile])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([atexit strrchr])

CFLAGS="$CFLAGS $SDL_CFLAGS -O3"
LIBS="$LIBS $SDL_LIBS"

AC_OUTPUT
