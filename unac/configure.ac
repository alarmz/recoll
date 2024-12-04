#
# Copyright (C) 2000, 2001, 2002 Loic Dachary <loic@senga.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
AC_INIT([unac], 1.7.0)
VERSION=`cat ${srcdir}/.version`
MAJOR_VERSION=[`expr $VERSION : '\([0-9][0-9]*\)'`]
AC_SUBST(MAJOR_VERSION)
MINOR_VERSION=[`expr $VERSION : '[0-9][0-9]*\.\([0-9][0-9]*\)'`]
AC_SUBST(MINOR_VERSION)
MICRO_VERSION=[`expr $VERSION : '[0-9][0-9]*\.[0-9][0-9]*\.\([0-9][0-9]*\)'`]
AC_SUBST(MICRO_VERSION)
AM_INIT_AUTOMAKE()
AC_PREREQ(2.13)

AM_CONFIG_HEADER(config.h)

AC_PROG_MAKE_SET

dnl
dnl Unicode version used by unac, as published at http://www.unicode.org/Public/
dnl
UNICODE_VERSION=14.0.0
AC_SUBST(UNICODE_VERSION)

AC_PROG_CC
AC_AIX
AC_COMPILE_WARNINGS

AM_PROG_LIBTOOL

AC_PATH_PROG(PERL, perl, /usr/bin/perl)
AC_PATH_PROG(SH, sh, /bin/sh)

AM_MAINTAINER_MODE

AM_ICONV

AC_CHECK_FUNCS(vsnprintf)

AC_OUTPUT(Makefile unac.pc unac.spec t_unac builder, chmod +x t_unac)
