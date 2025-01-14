Recoll KIO slave
================

Usage
=====

Depending on the protocol name used, the search results will be returned either as HTML pages
(looking quite like a normal Recoll result list), or as directory entries.

The HTML mode only works with Konqueror, not Dolphin and is slightly wobbly and obsolete. The
directory mode is available with both browsers, and also application open dialog (ie Kate).

To try things out, after building and installing, enter "recoll: search terms" in a Dolphin URL
entry. This should show recoll search results, displayed as directory entries.

Use hrecoll: in konqueror to get the HTML result list version.

You need to build/update the index with recollindex, the KIO slave doesn't deal with indexing at
all. 

Building and installing:
=======================

The main Recoll installation shares its prefix with the KIO slave, which needs to use the KDE
one. This means that, if KDE lives in /usr, Recoll must be configured with --prefix=/usr, not
/usr/local. Else you'll have run-time problems, the slave will not be able to find the Recoll
configuration.

By default, Recoll now installs its shared library in the normal $libdir place (it used to use a
private library installed in a subdirectory). This can be overriden by the configuration.

By default the kio build will use the public shared library (and its installed include files).

Recipe:

- Make sure the KF5 (or KF6) core and KIO devel packages and cmake are installed. You probably need
  the kio-devel and extra-cmake-modules packages.

- Extract the Recoll source.

- IF Recoll is not installed yet: configure recoll with --prefix=/usr (or wherever KDE lives), build
  and install Recoll. 

- In the Recoll source, go to kde/kioslave/kio_recoll, then build and install the kio slave:

    mkdir builddir
    cd builddir
    cmake -DCMAKE_INSTALL_PREFIX=/usr ..
    make
    sudo make install
