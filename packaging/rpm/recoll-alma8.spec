# Build for alma linux 8. recoll-chm, kio and krunner disabled because libchm is not available and
# build issue with the kde stuff (the kf5 kio and krunner should be fixable).

%global         gsspver 1.1.3
%global         __cmake_in_source_build 1

Summary:        Desktop full text search tool with Qt GUI
Name:           recoll
Version:        1.41.1
Release:        2%{?dist}
License:        GPLv2+
URL:            https://www.recoll.org
Source0:        https://www.recoll.org/recoll-%{version}.tar.gz
Source1:        https://www.recoll.org/downloads/gssp-recoll-%{gsspver}.tar.gz
Source10:       qmake-qt5.sh
Patch1:         meson-no-iconv-dep.diff
BuildRequires:  aspell-devel
BuildRequires:  bison
# BuildRequires:  chmlib-devel
BuildRequires:  desktop-file-utils
BuildRequires:  extra-cmake-modules
BuildRequires:  file-devel
BuildRequires:  gcc-c++
# kio
#BuildRequires:  kdelibs4-devel
#BuildRequires:  kf5-kio-devel
# krunner
#BuildRequires:  kf5-ki18n-devel
#BuildRequires:  kf5-krunner-devel
#BuildRequires:  kf5-knotifications-devel
#BuildRequires:  kf5-kpackage-devel

BuildRequires:  libxslt-devel
BuildRequires:  make
BuildRequires:  meson
BuildRequires:  python-rpm-macros
BuildRequires:  python3-devel
BuildRequires:  python3-setuptools
BuildRequires:  qt5-linguist
BuildRequires:  qt5-qtbase-devel
BuildRequires:  qt5-qtwebkit-devel
BuildRequires:  xapian-core-devel
BuildRequires:  zlib-devel
Requires:       xdg-utils

%description
Recoll is a personal full text search package for Linux, FreeBSD and
other Unix systems. It is based on a very strong back end (Xapian), for
which it provides an easy to use, feature-rich, easy administration
interface.

%package       libs
Summary:       Libraries for Recoll applications
%description   libs
Shared libraries required to run Recoll applications.

%package       devel
Summary:       Libraries and header files to develop Recoll enabled applications
Requires:      %{name}-libs = %{version}-%{release}
%description   devel
Libraries and header files required to develop Recoll enabled
applications.

%package       kio
Summary:       KIO support for recoll
Requires:      %{name} = %{version}-%{release}

%%description   kio
The recoll KIO slave allows performing a recoll search by entering an
appropriate URL in a KDE open dialog, or with an HTML-based interface
displayed in Konqueror.

%package       krunner
Summary:       KRunner support for recoll
Requires:      %{name} = %{version}-%{release}

%description   krunner
The recoll KRunner plugin adds Recoll search results to KRunner output.

%package       gssp
Summary:       Recoll GNOME Shell search provider
Requires:      %{name} = %{version}-%{release}
Requires:      gnome-shell
Requires:      python3-pydbus
%description   gssp
This package contains the Recoll GNOME Shell search provider

%prep
%setup -q -D -a 1
%patch -P1 -p 1 -F 2
sed -i -e '1{\,^#!/usr/bin/env,d}' python/recoll/recoll/rclconfig.py
ln -s gssp-recoll-%{gsspver} gssp

%build
CFLAGS="%{optflags}"; export CFLAGS
CXXFLAGS="%{optflags}"; export CXXFLAGS
LDFLAGS="%{?__global_ldflags}"; export LDFLAGS

# force use of custom/local qmake, to inject proper build flags (above)
install -m755 -D %{SOURCE10} qmake-qt5.sh
export QMAKE=$(pwd)/qmake-qt5.sh
%meson -Drecollq=true -Dsystemd=true -Dpython-chm=false
%meson_build

# gssp
pushd gssp
%configure
popd

%install
%meson_install

desktop-file-install --delete-original \
  --dir=%{buildroot}/%{_datadir}/applications \
  %{buildroot}/%{_datadir}/applications/%{name}-searchgui.desktop

# use /usr/bin/xdg-open
rm -f %{buildroot}/usr/share/recoll/filters/xdg-open
rm -f %{buildroot}%{_libdir}/recoll/librecoll.la

export RECOLL_LIB_DIR=%{_builddir}/%{name}-%{version}/redhat-linux-build/

# kio_recoll -kde5
#pushd kde/kioslave/kio_recoll
#%%cmake -DRECOLL_PUBLIC_LIB=1
#%%cmake_build
#%%cmake_install
#popd

# kio_recoll -kde4
#export QMAKE=qmake-qt4
#pushd kde/kioslave/kio_recoll-kde4
#%%cmake_kde4 -DRECOLL_PUBLIC_LIB=1
#%%cmake_build
#%%cmake_install
#popd

# krunner_recoll
#pushd kde/krunner
#%%cmake -DRECOLL_PUBLIC_LIB=1
#%%cmake_build
#%%cmake_install
#popd

# gssp
pushd gssp
make install DESTDIR=%{buildroot}
popd

mkdir -p %{buildroot}%{_sysconfdir}/ld.so.conf.d
echo "%{_libdir}/recoll" > %{buildroot}%{_sysconfdir}/ld.so.conf.d/recoll-%{_arch}.conf

%py_byte_compile %{__python3} %{buildroot}%{_datadir}/%{name}/filters/

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%license COPYING
%doc README
%{_sysconfdir}/ld.so.conf.d/recoll-%{_arch}.conf
%{_bindir}/recoll
%{_bindir}/recollindex
%{_bindir}/recollq
%{_datadir}/recoll
%{_datadir}/metainfo/org.recoll.recoll.appdata.xml
%{_datadir}/applications/recoll-searchgui.desktop
%{_datadir}/icons/hicolor/48x48/apps/recoll.png
%{_datadir}/icons/hicolor/scalable/apps/recoll.svg
%{_datadir}/pixmaps/recoll.png
%{python3_sitearch}/recoll
#%%{python3_sitearch}/recollchm
%{python3_sitearch}/recollaspell.cpython-*-linux-gnu*.so
%{_mandir}/man1/recoll.1*
%{_mandir}/man1/recollq.1*
%{_mandir}/man1/recollindex.1*
%{_mandir}/man1/xadump.1*
%{_mandir}/man5/recoll.conf.5*
%{_unitdir}/recollindex@.service
%{_userunitdir}/recollindex.service

%files libs
%{_libdir}/librecoll.so.*

%files devel
%{_libdir}/librecoll.so
%{_includedir}/recoll

#%%files kio
#%%license COPYING
#%%{_libdir}/kde4/kio_recoll.so
#%%{_libdir}/qt5/plugins/kf5/kio/kio_recoll.so
#%%{_datadir}/kde4/apps/kio_recoll/
#%%{_datadir}/kde4/services/recoll.protocol
#%%{_datadir}/kde4/services/recollf.protocol
#%%{_datadir}/kio_recoll/help.html
#%%{_datadir}/kio_recoll/welcome.html

#%%files krunner
#%%{_libdir}/qt5/plugins/kf5/krunner/krunner_recoll.so

%files gssp
%license COPYING
%{_bindir}/gssp-recoll.py
%{_datadir}/dbus-1/services/org.recoll.Recoll.SearchProvider.service
%{_datadir}/gnome-shell/search-providers/org.recoll.Recoll.search-provider.ini
%{_datadir}/applications/org.recoll.Recoll.SearchProvider.desktop

%changelog
* Sun Jun 30 2024 Terje Rosten <terje.rosten@ntnu.no> - 1.39.1-2
- Add patch from upstream to fix parallel build

* Thu Jun 27 2024 Terje Rosten <terje.rosten@ntnu.no> - 1.39.1-1
- 1.39.1

* Fri Jun 07 2024 Python Maint <python-maint@redhat.com> - 1.37.5-2
- Rebuilt for Python 3.13

* Sun May 05 2024 Terje Rosten <terje.rosten@ntnu.no> - 1.37.5-1
- 1.37.5

* Sun Feb 04 2024 Terje Rosten <terje.rosten@ntnu.no> - 1.37.4-1
- 1.37.4

* Sun Jan 28 2024 Terje Rosten <terje.rosten@ntnu.no> - 1.37.0-1
- 1.37.0

* Fri Jan 26 2024 Fedora Release Engineering <releng@fedoraproject.org> - 1.36.1-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_40_Mass_Rebuild

* Mon Jan 22 2024 Fedora Release Engineering <releng@fedoraproject.org> - 1.36.1-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_40_Mass_Rebuild

* Sat Nov 04 2023 Terje Rosten <terje.rosten@ntnu.no> - 1.36.1-1
- 1.36.1

* Mon Oct 30 2023 Terje Rosten <terje.rosten@ntnu.no> - 1.36.0-1
- 1.36.0
- Ship public lib in -libs package
- Ship headers in -devel package

* Sun Oct 15 2023 Terje Rosten <terje.rosten@ntnu.no> - 1.35.0-2
- Add explicit buildreq on qt5-linguist
- Use kde4 macros for kde4 parts

* Thu Aug 17 2023 Terje Rosten <terje.rosten@ntnu.no> - 1.35.0-1
- 1.35.0

* Fri Jul 21 2023 Fedora Release Engineering <releng@fedoraproject.org> - 1.34.7-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_39_Mass_Rebuild

* Mon Jun 26 2023 Python Maint <python-maint@redhat.com> - 1.34.7-2
- Rebuilt for Python 3.12

* Sun Jun 25 2023 Terje Rosten <terje.rosten@ntnu.no> - 1.34.7-1
- 1.34.7
- Add patch from upstream to fix bz#2213017 and bz#2203626

* Tue Jun 13 2023 Python Maint <python-maint@redhat.com> - 1.34.6-2
- Rebuilt for Python 3.12

* Sun Mar 26 2023 Terje Rosten <terje.rosten@ntnu.no> - 1.34.6-1
- 1.34.6

* Sun Mar 05 2023 Terje Rosten <terje.rosten@ntnu.no> - 1.34.5-1
- 1.34.5

* Sun Feb 19 2023 Terje Rosten <terje.rosten@ntnu.no> - 1.34.3-1
- 1.34.3

* Sun Feb 19 2023 bzs <me@bzs.rocks> - 1.34.2-3
- Add krunner (rhbz#2170512)

* Sun Jan 29 2023 Terje Rosten <terje.rosten@ntnu.no> - 1.34.2-2
- Fix file pattern to cover arm32 platform

* Thu Jan 26 2023 Terje Rosten <terje.rosten@ntnu.no> - 1.34.2-1
- 1.34.2

* Wed Jan 18 2023 Terje Rosten <terje.rosten@ntnu.no> - 1.34.0-1
- 1.34.0

* Mon Nov 28 2022 Terje Rosten <terje.rosten@ntnu.no> - 1.33.3-1
- 1.33.3

* Sat Nov 12 2022 Terje Rosten <terje.rosten@ntnu.no> - 1.33.2-1
- 1.33.2

* Sun Oct 23 2022 Terje Rosten <terje.rosten@ntnu.no> - 1.33.1-1
- 1.33.1

* Sat Aug 27 2022 Terje Rosten <terje.rosten@ntnu.no> - 1.32.7-1
- 1.32.7

* Sat Jul 23 2022 Fedora Release Engineering <releng@fedoraproject.org> - 1.31.6-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_37_Mass_Rebuild

* Mon Jun 13 2022 Python Maint <python-maint@redhat.com> - 1.31.6-3
- Rebuilt for Python 3.11

* Thu Feb 24 2022 Terje Rosten <terje.rosten@ntnu.no> - 1.31.6-2
- Add patch to fix KIO build

* Thu Feb 24 2022 Terje Rosten <terje.rosten@ntnu.no> - 1.31.6-1
- 1.31.6

* Sun Nov 28 2021 Terje Rosten <terje.rosten@ntnu.no> - 1.31.4-1
- 1.31.4

* Thu Sep 23 2021 Terje Rosten <terje.rosten@ntnu.no> - 1.31.0-4
- GSSP 1.1.1

* Fri Jul 23 2021 Fedora Release Engineering <releng@fedoraproject.org> - 1.31.0-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_35_Mass_Rebuild

* Fri Jun 04 2021 Python Maint <python-maint@redhat.com> - 1.31.0-2
- Rebuilt for Python 3.10

* Wed Apr 28 2021 Terje Rosten <terje.rosten@ntnu.no> - 1.31.0-1
- 1.31.0

* Fri Apr 23 2021 Terje Rosten <terje.rosten@ntnu.no> - 1.30.1-1
- 1.30.1

* Sat Apr 03 2021 Terje Rosten <terje.rosten@ntnu.no> - 1.30.0-1
- 1.30.0

* Sun Mar 14 2021 Terje Rosten <terje.rosten@ntnu.no> - 1.29.2-1
- 1.29.2

* Thu Mar 11 2021 Terje Rosten <terje.rosten@ntnu.no> - 1.29.1-1
- 1.29.1

* Sun Feb 07 2021 Terje Rosten <terje.rosten@ntnu.no> - 1.28.6-1
- 1.28.6

* Wed Jan 27 2021 Fedora Release Engineering <releng@fedoraproject.org> - 1.28.5-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_34_Mass_Rebuild

* Wed Jan 20 2021 Terje Rosten <terje.rosten@ntnu.no> - 1.28.5-1
- 1.28.5

* Thu Jan 14 2021 Terje Rosten <terje.rosten@ntnu.no> - 1.28.3-1
- 1.28.3

* Mon Oct 05 2020 Terje Rosten <terje.rosten@ntnu.no> - 1.27.9-1
- 1.27.9

* Wed Jul 29 2020 Fedora Release Engineering <releng@fedoraproject.org> - 1.27.3-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_33_Mass_Rebuild

* Sat Jul 25 2020 Terje Rosten <terje.rosten@ntnu.no> - 1.27.3-2
- Fix cmake macro usage

* Sun Jun 28 2020 Terje Rosten <terje.rosten@ntnu.no> - 1.27.3-1
- 1.27.3

* Sun Jun 07 2020 Terje Rosten <terje.rosten@ntnu.no> - 1.27.2-1
- 1.27.2

* Tue May 26 2020 Miro Hrončok <mhroncok@redhat.com> - 1.27.0-3
- Rebuilt for Python 3.9

* Tue May 12 2020 Terje Rosten <terje.rosten@ntnu.no> - 1.27.0-2
- Add patch to fix python subscriptable issue

* Sat May 09 2020 Terje Rosten <terje.rosten@ntnu.no> - 1.27.0-1
- 1.27.0

* Mon Apr 13 2020 Terje Rosten <terje.rosten@ntnu.no> - 1.26.7-1
- 1.26.7

* Mon Mar 02 2020 Terje Rosten <terje.rosten@ntnu.no> - 1.26.5-1
- 1.26.5

* Thu Jan 30 2020 Fedora Release Engineering <releng@fedoraproject.org> - 1.26.4-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_32_Mass_Rebuild

* Thu Jan 23 2020 Terje Rosten <terje.rosten@ntnu.no> - 1.26.4-1
- 1.26.4

* Mon Dec 02 2019 Terje Rosten <terje.rosten@ntnu.no> - 1.26.3-1
- 1.26.3

* Sat Oct 26 2019 Terje Rosten <terje.rosten@ntnu.no> - 1.26.1-1
- 1.26.1

* Wed Oct 16 2019 Terje Rosten <terje.rosten@ntnu.no> - 1.26.0-1
- 1.26.0

* Wed Sep 04 2019 Terje Rosten <terje.rosten@ntnu.no> - 1.25.2-1
- 1.25.22

* Mon Aug 19 2019 Miro Hrončok <mhroncok@redhat.com> - 1.25.20-3
- Rebuilt for Python 3.8

* Mon Aug 05 2019 Terje Rosten <terje.rosten@ntnu.no> - 1.25.20-2
- Remove all Python 2 deps and provides

* Mon Aug 05 2019 Terje Rosten <terje.rosten@ntnu.no> - 1.25.20-1
- 1.25.20

* Fri Jul 26 2019 Fedora Release Engineering <releng@fedoraproject.org> - 1.25.16-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_31_Mass_Rebuild

* Sat May 25 2019 Terje Rosten <terje.rosten@ntnu.no> - 1.25.16-1
- 1.25.16

* Sun Apr 28 2019 Terje Rosten <terje.rosten@ntnu.no> - 1.25.15-1
- 1.25.15

* Sun Mar 31 2019 Terje Rosten <terje.rosten@ntnu.no> - 1.25.11-1
- 1.25.11

* Thu Mar 14 2019 Terje Rosten <terje.rosten@ntnu.no> - 1.25.5-2
- Add gssp

* Thu Mar 07 2019 Terje Rosten <terje.rosten@ntnu.no> - 1.25.5-1
- 1.25.5

* Sun Feb 24 2019 Terje Rosten <terje.rosten@ntnu.no> - 1.25.4-1
- 1.25.4

* Sat Feb 02 2019 Fedora Release Engineering <releng@fedoraproject.org> - 1.24.3-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_30_Mass_Rebuild

* Mon Dec 03 2018 Terje Rosten <terje.rosten@ntnu.no> - 1.24.3-1
- 1.24.3

* Sat Sep 29 2018 Terje Rosten <terje.rosten@ntnu.no> - 1.24.1-1
- 1.24.1

* Tue Sep 18 2018 Terje Rosten <terje.rosten@ntnu.no> - 1.23.7-8
- Add patch from upstream to fix rhbz#1625313

* Tue Jul 31 2018 Florian Weimer <fweimer@redhat.com> - 1.23.7-7
- Rebuild with fixed binutils

* Mon Jul 30 2018 Terje Rosten <terje.rosten@ntnu.no> - 1.23.7-6
- Add patch from upstream to fix FTBFS

* Sat Jul 14 2018 Fedora Release Engineering <releng@fedoraproject.org> - 1.23.7-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_29_Mass_Rebuild

* Tue Jun 19 2018 Miro Hrončok <mhroncok@redhat.com> - 1.23.7-4
- Rebuilt for Python 3.7

* Sun May 13 2018 Terje Rosten <terje.rosten@ntnu.no> - 1.23.7-3
- Add patch to fix rhbz#1577613

* Fri Feb 09 2018 Fedora Release Engineering <releng@fedoraproject.org> - 1.23.7-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_28_Mass_Rebuild

* Tue Jan 09 2018 Terje Rosten <terje.rosten@ntnu.no> - 1.23.7-1
- 1.23.7

* Sat Dec 09 2017 Terje Rosten <terje.rosten@ntnu.no> - 1.23.6-1
- 1.23.6

* Mon Sep 04 2017 Terje Rosten <terje.rosten@ntnu.no> - 1.23.3-1
- 1.23.3

* Thu Aug 03 2017 Fedora Release Engineering <releng@fedoraproject.org> - 1.23.2-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Binutils_Mass_Rebuild

* Thu Jul 27 2017 Fedora Release Engineering <releng@fedoraproject.org> - 1.23.2-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Mass_Rebuild

* Mon May 15 2017 Terje Rosten <terje.rosten@ntnu.no> - 1.23.2-1
- 1.23.2

* Mon May 15 2017 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.23.1-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_26_27_Mass_Rebuild

* Mon Mar 13 2017 Terje Rosten <terje.rosten@ntnu.no> - 1.23.1-1
- 1.23.1

* Sat Feb 18 2017 Terje Rosten <terje.rosten@ntnu.no> - 1.22.4-1
- 1.22.4

* Sat Feb 11 2017 Fedora Release Engineering <releng@fedoraproject.org> - 1.22.3-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_26_Mass_Rebuild

* Mon Dec 26 2016 Peter Robinson <pbrobinson@fedoraproject.org> 1.22.3-2
- Rebuild (xapian 1.4)

* Sun Sep 18 2016 Terje Rosten <terje.rosten@ntnu.no> - 1.22.3-1
- 1.22.3

* Tue Jul 19 2016 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.21.7-2
- https://fedoraproject.org/wiki/Changes/Automatic_Provides_for_Python_RPM_Packages

* Fri May 13 2016 Terje Rosten <terje.rosten@ntnu.no> - 1.21.7-1
- 1.21.7

* Wed Apr 06 2016 Terje Rosten <terje.rosten@ntnu.no> - 1.21.6-1
- 1.21.6
- Nice fixes from Jean-Francois Dockes (upstream) merged, thanks!

* Sat Feb 06 2016 Terje Rosten <terje.rosten@ntnu.no> - 1.21.5-1
- 1.21.5

* Thu Feb 04 2016 Fedora Release Engineering <releng@fedoraproject.org> - 1.21.4-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_24_Mass_Rebuild

* Mon Jan 18 2016 Terje Rosten <terje.rosten@ntnu.no> - 1.21.4-1
- 1.21.4

* Sat Oct 31 2015 Terje Rosten <terje.rosten@ntnu.no> - 1.21.3-1
- 1.21.3

* Sat Oct 03 2015 Terje Rosten <terje.rosten@ntnu.no> - 1.21.2-1
- 1.21.2

* Thu Jun 18 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.20.6-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Tue May 05 2015 Terje Rosten <terje.rosten@ntnu.no> - 1.20.6-1
- 1.20.6
- Fixes bz#1218161 and bz#1204464

* Sat May 02 2015 Kalev Lember <kalevlember@gmail.com> - 1.20.5-3
- Rebuilt for GCC 5 C++11 ABI change

* Sat Apr 11 2015 Terje Rosten <terje.rosten@ntnu.no> - 1.20.5-2
- Add KIO subpackage

* Tue Apr 07 2015 Terje Rosten <terje.rosten@ntnu.no> - 1.20.5-1
- 1.20.5
- Include kio support (bz#1203257)

* Sat Jan 10 2015 Terje Rosten <terje.rosten@ntnu.no> - 1.20.1-1
- 1.20.1

* Sun Nov 09 2014 Terje Rosten <terje.rosten@ntnu.no> - 1.19.14p2-1
- 1.19.14p2

* Sun Aug 17 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.19.13-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Sun Jun 08 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.19.13-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Tue May 06 2014 Terje Rosten <terje.rosten@ntnu.no> - 1.19.13-1
- 1.19.13

* Mon Jan 20 2014 Terje Rosten <terje.rosten@ntnu.no> - 1.19.11-1
- 1.19.11

* Mon Nov 11 2013 Terje Rosten <terje.rosten@ntnu.no> - 1.19.9-1
- 1.19.9

* Tue Nov 05 2013 Terje Rosten <terje.rosten@ntnu.no> - 1.19.8-1
- 1.19.8

* Sun Aug 04 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.19.4-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Wed Jun 12 2013 Terje Rosten <terje.rosten@ntnu.no> - 1.19.4-2
- Fix filter setup

* Mon Jun 10 2013 Terje Rosten <terje.rosten@ntnu.no> - 1.19.4-1
- 1.19.4

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.18.1-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Mon Nov 19 2012 Terje Rosten <terje.rosten@ntnu.no> - 1.18.1-1
- 1.18.1

* Sat Jul 21 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.17.3-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Wed May 30 2012 Terje Rosten <terje.rosten@ntnu.no> - 1.17.3-1
- 1.17.3

* Sat Mar 31 2012 Terje Rosten <terje.rosten@ntnu.no> - 1.17.1-1
- 1.17.1

* Sun Mar 25 2012 Terje Rosten <terje.rosten@ntnu.no> - 1.17-1
- 1.17

* Tue Feb 28 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.16.2-3
- Rebuilt for c++ ABI breakage

* Wed Feb 15 2012 Terje Rosten <terje.rosten@ntnu.no> - 1.16.2-2
- Add patch to build with gcc 4.7

* Wed Feb 01 2012 Terje Rosten <terje.rosten@ntnu.no> - 1.16.2-1
- 1.16.2

* Sat Jan 14 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.16.1-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Mon Oct 24 2011 Terje Rosten <terje.rosten@ntnu.no> - 1.16.1-2
- Add patch to fix crash (bz #747472)

* Tue Oct 18 2011 Terje Rosten <terje.rosten@ntnu.no> - 1.16.1-1
- 1.16.1

* Tue May 24 2011 Terje Rosten <terje.rosten@ntnu.no> - 1.15.8-2
- add patch from upstream to fix crash.

* Sun May 08 2011 Terje Rosten <terje.rosten@ntnu.no> - 1.15.8-1
- 1.15.8

* Wed Feb 09 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.14.4-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Fri Jan 28 2011 Terje Rosten <terje.rosten@ntnu.no> - 1.14.4-1
- 1.14.4

* Mon Nov 15 2010 Terje Rosten <terje.rosten@ntnu.no> - 1.14.2-2
- Add patch to fix #631704

* Sun Nov  7 2010 Terje Rosten <terje.rosten@ntnu.no> - 1.14.2-1
- 1.14.2

* Thu Jul 15 2010 Terje Rosten <terje.rosten@ntnu.no> - 1.13.04-6
- add patch to build with xapian 1.2 (from J.F. Dockes, thanks)

* Sat Jul 10 2010 Terje Rosten <terje.rosten@ntnu.no> - 1.13.04-5
- use system xdg-open
- trim chagenlog

* Fri Jul  9 2010 Terje Rosten <terje.rosten@ntnu.no> - 1.13.04-4
- fix some review comments

* Mon May 10 2010 Terje Rosten <terje.rosten@ntnu.no> - 1.13.04-3
- use version macro in source url
- don't strip bins, giving non empty debuginfo package
- more explicit file listing
- try to preserve timestamps 
- use proper name for spec file
- fix changelog
- license seems to be GPLv2+
- include some %%doc files
- add buildroot tag
- some source add execute bit set
- explicit enable inotify
- add aspell-devel to bulldreq
- make with _smp_mflags seems to work
- add post scripts
- add patch to build gui with correct flags (ref: 338791)

* Sun May  9 2010  J.F. Dockes <jfd@recoll.org> 1.13.04-2
- bumped the release number to issue new rpms for fc10

* Sun May  9 2010  J.F. Dockes 1.13.04
- updated to recoll release 1.13.04 

* Fri Feb 12 2010 Terry Duell 1.13.02
- updated to release 1.13.02

* Tue Jan 12 2010 Terry Duell  1.13.01-3
- updated to fix Fedora desktop-file-install and install icon

* Sun Jan 10 2010 Jean-Francois Dockes <jfd@recoll.org> 1.13.01-2
- updated for recent fedoras: depend on xapian packages, use qt4

* Thu Jan 07 2010 Jean-Francois Dockes <jfd@recoll.org> 1.13.01-1
- update to release 1.13.01

* Wed Feb  1 2006 Jean-Francois Dockes <jfd@recoll.org> 1.2.0-1
- initial packaging
