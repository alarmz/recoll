# Turn off the brp-python-bytecompile script
%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')

Summary:        Desktop full text search tool with Qt GUI
Name:           recoll
Version:        1.35.0
Release:        2%{?dist}
Group:          Applications/Databases
License:        GPLv2+
URL:            http://www.lesbonscomptes.com/recoll/
Source0:        http://www.lesbonscomptes.com/recoll/recoll-%{version}.tar.gz
Source10:       qmake-qt6.sh
BuildRequires:  aspell-devel
BuildRequires:  bison
BuildRequires:  desktop-file-utils

BuildRequires:  qt6-gui-devel
BuildRequires:  qt6-tools
BuildRequires:  qt6-webenginewidgets-devel
BuildRequires:  python3-devel
BuildRequires:  libxapian-devel

BuildRequires:  extra-cmake-modules
BuildRequires:  zlib-devel
BuildRequires:  chmlib-devel
BuildRequires:  libxslt-devel
Requires:       xdg-utils
Requires:       aspell

%description
Recoll is a personal full text search package for Linux, FreeBSD and
other Unix systems. It is based on the powerful Xapian backend, for
which it provides an easy to use, feature-rich, easy administration
interface.


%prep
%setup -q -n %{name}-%{version}

%build
CFLAGS="%{optflags}"; export CFLAGS
CXXFLAGS="%{optflags}"; export CXXFLAGS
LDFLAGS="%{?__global_ldflags}"; export LDFLAGS

# force use of custom/local qmake, to inject proper build flags (above)
install -m755 -D %{SOURCE10} qmake-qt6.sh
export QMAKE=qmake6

%configure --enable-webengine
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot} STRIP=/bin/true INSTALL='install -p'

desktop-file-install --delete-original \
  --dir=%{buildroot}/%{_datadir}/applications \
  %{buildroot}/%{_datadir}/applications/%{name}-searchgui.desktop

# use /usr/bin/xdg-open
rm -f %{buildroot}/usr/share/recoll/filters/xdg-open
rm -f %{buildroot}/usr/lib/systemd/system/recollindex@.service
rm -f %{buildroot}/usr/lib/systemd/user/recollindex.service
rm -f %{buildroot}/usr/share/man/man1/rclgrep.1
rm -f %{buildroot}/usr/share/man/man1/rclgrep.1.gz
rm -f %{buildroot}/usr/*/librecoll.la

# Mix of Python 2 and 3, needs special care

py2_byte_compile () {
    bytecode_compilation_path="$1"
    find $bytecode_compilation_path -type f -a -name "*.py" -print0 | xargs -0 %{__python2} -O -c 'import py_compile, sys; [ py_compile.compile(f, dfile=f.partition("%{buildroot}")[2]) for f in sys.argv[1:] ]' || :
    find $bytecode_compilation_path -type f -a -name "*.py" -print0 | xargs -0 %{__python2} -c 'import py_compile, sys; [ py_compile.compile(f, dfile=f.partition("%{buildroot}")[2]) for f in sys.argv[1:] ]' || :
}

py3_byte_compile () {
    bytecode_compilation_path="$1"
    find $bytecode_compilation_path -type f -a -name "*.py" -print0 | xargs -0 %{__python3} -O -c 'import py_compile, sys; [py_compile.compile(f, dfile=f.partition("%{buildroot}")[2], optimize=opt) for opt in range(2) for f in sys.argv[1:] ]' || :
}

for py in %{buildroot}%{_datadir}/%{name}/filters/*.py; do
	py3_byte_compile $py
done

%post
touch --no-create %{_datadir}/icons/hicolor
if [ -x %{_bindir}/gtk-update-icon-cache ] ; then
  %{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/hicolor
fi
if [ -x %{_bindir}/update-desktop-database ] ; then
  %{_bindir}/update-desktop-database &> /dev/null
fi
/sbin/ldconfig
exit 0

%postun
touch --no-create %{_datadir}/icons/hicolor 
if [ -x %{_bindir}/gtk-update-icon-cache ] ; then
  %{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/hicolor
fi
if [ -x %{_bindir}/update-desktop-database ] ; then
  %{_bindir}/update-desktop-database &> /dev/null
fi
/sbin/ldconfig
exit 0

%files
%license COPYING
%doc README
%{_bindir}/%{name}
%{_bindir}/%{name}index
%{_datadir}/%{name}
%{_datadir}/metainfo/org.recoll.recoll.appdata.xml
%{_datadir}/applications/%{name}-searchgui.desktop
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%{_datadir}/pixmaps/%{name}.png
%{_includedir}/recoll
%{_libdir}/librecoll*.so
%{python2_sitearch}/recoll
%{python2_sitearch}/Recoll*.egg-info
%{python3_sitearch}/recoll
%{python3_sitearch}/Recoll*.egg-info
%{python3_sitearch}/recollaspell.*
%{python3_sitearch}/recoll_aspell*.egg-info
%{python3_sitearch}/recollchm
%{python3_sitearch}/recollchm*.egg-info
%{_mandir}/man1/%{name}.1*
%{_mandir}/man1/%{name}q.1*
%{_mandir}/man1/%{name}index.1*
%{_mandir}/man5/%{name}.conf.5*


%changelog
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
