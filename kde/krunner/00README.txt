Recoll Plasma KRunner module
====================

The Recoll Plasma desktop KRunner module (recollrunner) provides results from an existing recoll
index to KRunner.


Building and installing:
=======================

These instructions are for building the KRunner module on a system where a main Recoll package is
available. See the Recoll manual for a full Recoll build from source.

By default, Recoll now installs its shared library in the normal $libdir place (it used to use a
private library installed in a subdirectory). This makes it much easier to build modules using the
library. 

By default the krunner build will use the public shared library (and its installed include
files). Depending on your distribution, the development files come with a separate xxx-dev or
xxx-devel package or with the main Recoll package.

Recipe:

- Make sure the KF5 (or KF6) core and KRunner devel packages and cmake are installed. You probably
  need the kio-devel and extra-cmake-modules packages.

- Extract the Recoll source, use the same main version than your binary packages.
  Source tar files: https://www.recoll.org/pages/filelists.html

- IF Recoll (esp. the library and include files) is not installed from a package: configure recoll
  with --prefix=/usr (or wherever KDE lives), build and install Recoll. See the main manual.

- In the Recoll source, go to kde/krunner, then build and install the krunner module:

    mkdir builddir
    cd builddir
    cmake -DCMAKE_INSTALL_PREFIX=/usr ..
    make
    sudo make install
