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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#

#
# Documentation
#
man_MANS = unaccent.1 unac.3
AM_CPPFLAGS = -DUNAC_VERSION=\"$(VERSION)\"

bin_PROGRAMS = unaccent
unaccent_SOURCES = unaccent.c 
unaccent_LDFLAGS = $(PROFILING)
unaccent_LDADD = @LIBOBJS@ libunac.la

#
# Library and headers
#
lib_LTLIBRARIES = libunac.la
libunac_la_LDFLAGS = -version-info $(MAJOR_VERSION):$(MINOR_VERSION):$(MICRO_VERSION)
libunac_la_SOURCES = unac.c

include_HEADERS = \
	unac.h

unac.c: $(UNICODEDATA) builder.in
	$(PERL) builder -source -database=$(srcdir)/$(UNICODEDATA)

#
# Tests
#
TESTS = t_unac

noinst_PROGRAMS = unactest unactest1

unactest_SOURCES = unactest.c
unactest_LDFLAGS = $(PROFILING)
unactest_LDADD = libunac.la

unactest1_SOURCES = unactest1.c
unactest1_LDFLAGS = $(PROFILING)
unactest1_LDADD = libunac.la

#
# Packaging
#

MAINTAINERCLEANFILES = $(DISTCLEANFILES)

DISTCLEANFILES = config.log config.status

UNICODEDATA = UnicodeData-$(UNICODE_VERSION).txt

EXTRA_DIST = builder.in $(UNICODEDATA) .version t_unac.in \
	unaccent.1 unac.3

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = unac.pc

#
# Build source and binary rpms. As of rpm-3.0, the ~/.rpmmacros
# must contain the following line:
# %_topdir /home/loic/local/rpm
# and that /home/loic/local/rpm contains the directory SOURCES, BUILD etc.
#
VERSION = $(MAJOR_VERSION).$(MINOR_VERSION).$(MICRO_VERSION)

rpm:
	RPM_TOPDIR=`rpm --showrc | $(PERL) -n -e 'print if(s/.*_topdir\s+(.*)/$$1/)'` ; \
	cp $(PACKAGE).spec $$RPM_TOPDIR/SPECS ; \
	cp $(PACKAGE)-$(VERSION).tar.gz $$RPM_TOPDIR/SOURCES ; \
	rpm -ba --clean --rmsource $$RPM_TOPDIR/SPECS/$(PACKAGE).spec ; \
	mv $$RPM_TOPDIR/RPMS/i386/$(PACKAGE)-$(VERSION)-1.i386.rpm . ; \
	mv $$RPM_TOPDIR/SRPMS/$(PACKAGE)-$(VERSION)-1.src.rpm .

#
# Cleanup
#
clean-local:
	rm -f unac.reference

