#
# spec file for package recollrunner
#
# Copyright (c) 2013 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An &quot;Open Source License&quot; is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

%define rname recoll

Name:           recollrunner
Version:        1.43.0
Release:        0
Summary:        KRunner for the Recoll full text search tool
License:        GPL-2.0+
Group:          Productivity/Text/Utilities
Url:            http://www.lesbonscomptes.com/recoll/
Source:         http://www.lesbonscomptes.com/recoll/%{rname}-%{version}.tar.gz
BuildRequires:  ki18n-devel
BuildRequires:  plasma-framework-devel
BuildRequires:  kconfig-devel
BuildRequires:  kpackage-devel
BuildRequires:  krunner-devel
BuildRequires:  knotifications-devel
BuildRequires:  libxapian-devel
Requires:       recoll = %{version}
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
Recoll is a personal full text search tool for Unix/Linux.

This package provides the krunner


%prep
%setup -q -n %{rname}-%{version}

%build
pushd kde/krunner
mv -f CMakeLists-KF6.txt CMakeLists.txt
%cmake_kf6 build
%ninja_build -C build
popd

%install
pushd kde/krunner
%ninja_install -C build
popd

%files
%defattr(-,root,root)
%{_libdir}/qt6/plugins/kf6/krunner/krunner_recoll.so

%changelog
