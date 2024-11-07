#                                               -*- Autoconf -*-
# configure.ac -- Autoconf script for libdrmutils
#

# Process this file with autoconf to produce a configure script

# Requires autoconf tool later than 2.61
AC_PREREQ(2.61)
# Initialize the libdrmutils package version 1.0.0
AC_INIT([libdrmutils],1.0.0)
# Does not strictly follow GNU Coding standards
AM_INIT_AUTOMAKE([foreign])
# Disables auto rebuilding of configure, Makefile.ins
AM_MAINTAINER_MODE

m4_include([../common.m4])
COMMON_CONFIGS

AC_SUBST([AM_CPPFLAGS], [--std=c++11])

# Add project version as per MAJOR.MINOR.PATCH strategy
# Increment MAJOR version when you make incompatible API changes
#           MINOR version when you add functionality in a backward compatible manner
#           PATCH version when you make backward compatible bug fixes
m4_define([L_MAJOR], [1])
m4_define([L_MINOR], [0])
m4_define([L_PATCH], [0])
AC_SUBST([L_VERSION], L_MAJOR.L_MINOR.L_PATCH)
AC_SUBST([L_VERSION_NUMBER], L_MAJOR:L_MINOR:L_PATCH)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_SUBST([CFLAGS])
AC_SUBST([CC])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT