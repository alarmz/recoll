# Turn off the brp-python-bytecompile script
%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')

Summary:        Desktop full text search tool with Qt GUI
Name:           recoll
Version:        1.43.0
Release:        2%{?dist}
Group:          Applications/Databases
License:        GPLv2+
URL:            http://www.lesbonscomptes.com/recoll/
Source0:        http://www.lesbonscomptes.com/recoll/recoll-%{version}.tar.gz
Source10:       qmake-qt5.sh

BuildRequires:  aspell-devel
BuildRequires:  chmlib-devel
BuildRequires:  bison
BuildRequires:  chmlib-devel
BuildRequires:  desktop-file-utils
BuildRequires:  extra-cmake-modules
BuildRequires:  file-devel
# For leap 15: the default gcc-7 won't work because it does not support named initializer for
# structs in the Python extension. (supported as of gcc8 I think, but 13 is available).
BuildRequires:  gcc13-c++
BuildRequires:  libQt5Gui-devel
BuildRequires:  libQt5Xml-devel
BuildRequires:  libqt5-qtwebengine-devel
BuildRequires:  libqt5-linguist
BuildRequires:  libxapian-devel
BuildRequires:  libxslt-devel
BuildRequires:  meson
BuildRequires:  python3-devel
BuildRequires:  zlib-devel

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
# For leap 15: See above
export CC=gcc-13
export CXX=g++-13
CFLAGS="%{optflags}"; export CFLAGS
CXXFLAGS="%{optflags}"; export CXXFLAGS
LDFLAGS="%{?__global_ldflags}"; export LDFLAGS

# force use of custom/local qmake, to inject proper build flags (above)
install -m755 -D %{SOURCE10} qmake-qt5.sh
export QMAKE=$(pwd)/qmake-qt5.sh
%meson -Dwebengine=true -Drecollq=true
%meson_build


%install
%meson_install

desktop-file-install --delete-original \
  --dir=%{buildroot}/%{_datadir}/applications \
  %{buildroot}/%{_datadir}/applications/%{name}-searchgui.desktop

rm -f %{buildroot}/usr/share/man/man1/xadump.1
rm -f %{buildroot}/usr/share/man/man1/xadump.1.gz
rm -f %{buildroot}/usr/lib/systemd/system/recollindex@.service
rm -f %{buildroot}/usr/lib/systemd/user/recollindex.service


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
%{_bindir}/%{name}q
%{_bindir}/%{name}index
%{_datadir}/%{name}
%{_datadir}/metainfo/org.recoll.recoll.appdata.xml
%{_datadir}/applications/%{name}-searchgui.desktop
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%{_datadir}/icons/hicolor/scalable/apps/%{name}.svg
%{_datadir}/pixmaps/%{name}.png
%{_includedir}/recoll
%{_libdir}/librecoll*.so
%{_libdir}/librecoll.so.*
%{python3_sitearch}/recoll
%{python3_sitearch}/recollaspell.*
%{python3_sitearch}/recollchm
%{_mandir}/man1/%{name}.1*
%{_mandir}/man1/%{name}q.1*
%{_mandir}/man1/%{name}index.1*
%{_mandir}/man5/%{name}.conf.5*


%changelog
* Wed Apr 16 2025 <jfd@recoll.org> - 1.43.0
- Opensuse Leap build

