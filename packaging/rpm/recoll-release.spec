%define version_val %{?rpm_version}%{!?rpm_version:1.43.13}

Name:           recoll
Version:        %{version_val}
Release:        1%{?dist}
Summary:        Desktop full text search tool
License:        GPL-2.0-or-later
URL:            https://www.recoll.org

AutoReqProv:    no
Requires:       xapian-core-libs libxml2 libxslt zlib

%description
Recoll is a personal full text search package for Linux and other Unix
systems. It is based on the Xapian search engine library, providing
powerful full-text search with a rich feature set.

%install
rm -rf %{buildroot}

# Binaries
install -d %{buildroot}%{_bindir}
install -m 755 %{_sourcedir}/recollindex %{buildroot}%{_bindir}/
install -m 755 %{_sourcedir}/recollq %{buildroot}%{_bindir}/

# Shared library
install -d %{buildroot}%{_libdir}
if ls %{_sourcedir}/librecoll.so* 1>/dev/null 2>&1; then
    cp -a %{_sourcedir}/librecoll.so* %{buildroot}%{_libdir}/
fi

# Configuration examples
install -d %{buildroot}%{_datadir}/%{name}/examples
cp -a %{_sourcedir}/sampleconf/* %{buildroot}%{_datadir}/%{name}/examples/

# Filters
install -d %{buildroot}%{_datadir}/%{name}/filters
cp -a %{_sourcedir}/filters/* %{buildroot}%{_datadir}/%{name}/filters/

%files
%{_bindir}/recollindex
%{_bindir}/recollq
%{_libdir}/librecoll.so*
%{_datadir}/%{name}/

%changelog
