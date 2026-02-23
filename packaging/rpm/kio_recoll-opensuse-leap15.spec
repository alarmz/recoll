#
# spec file for package kio_recoll
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

Name:           kio_recoll
Version:        1.43.13
Release:        0
Summary:        Extended Search
License:        GPL-2.0+
Summary:        KIO slave for the Recoll full text search tool
Group:          Productivity/Text/Utilities
Url:            http://www.lesbonscomptes.com/recoll/
Source:         http://www.lesbonscomptes.com/recoll/%{rname}-%{version}.tar.gz
BuildRequires:  kio-devel
BuildRequires:  ki18n-devel
BuildRequires:  knotifications-devel
Requires:       recoll = %{version}
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
Recoll is a personal full text search tool for Unix/Linux.

This package provides the kio-slave


%prep
%setup -q -n %{rname}-%{version}

%build
pushd kde/kioslave/kio_recoll
%cmake_kf5 -d build
%make_jobs
popd

%install
pushd kde/kioslave/kio_recoll
%kf5_makeinstall -C build
popd


%files
%defattr(-,root,root)
%{_libdir}/qt5/plugins/kf5/kio/kio_recoll.so
%{_datadir}/kio_recoll/

%changelog
* Tue Apr 05 2016 Jean-Francois Dockes <jfd@recoll.org> 1.21.6-0
- Also build kde5 versions: works with Dolphin. Keep kde4 version for
  Konqueror
* Sun Mar 18 2012 Jean-Francois Dockes <jfd@recoll.org> 1.17.0-0
- 1.17.0
* Mon May 02 2011 Jean-Francois Dockes <jfd@recoll.org> 1.16.2-0
- 1.16.2
* Mon May 02 2011 Jean-Francois Dockes <jfd@recoll.org> 1.15.8-0
- 1.15.8
* Sun Mar 06 2011 Jean-Francois Dockes <jfd@recoll.org> 1.15.5-0
- Initial spec file for kio

