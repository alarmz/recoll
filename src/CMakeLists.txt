#[===========================================================================[.rst:

Recoll CMake buildsystem
------------------------

Experimental CMake build.

Since this wasn't planned to be used immediately, the version requirements are very aggressive:

Debian 12+ (Bookworm) (or equivalents)
Qt 5.15+ or Qt 6.4+
CMake 3.25+
C++11 (Qt5) or C++17 (Qt6) compiler

Configuration
^^^^^^^^^^^^^

The build can be configured, either on the command line or in the cmake-gui,
by defining the following options. (See GUI or code for defaults.)

* ``RECOLL_ENABLE_X11MON`` (default: ``OFF``)
* ``RECOLL_ENABLE_WEBENGINE`` (default: ``ON``)
* ``RECOLL_ENABLE_LIBMAGIC`` (default: ``OFF``)
* ``RECOLL_ENABLE_SYSTEMD`` (default: ``ON``, linux-only)
* ``RECOLL_QT6_BUILD`` (default: ``OFF``)

If ``RECOLL_ENABLE_SYSTEMD`` is true, the install paths can be overridden
by setting these cache variables. (Values should be paths relative to the
install prefix, e.g. ``lib/systemd/system``.)

* ``SYSTEMD_SYSTEM_UNIT_DIR``
* ``SYSTEMD_USER_UNIT_DIR``

By default, pkg-config will be used to query the paths from ``systemd.pc``.

If ``RECOLL_ENABLE_LIBMAGIC`` is false, the path to a suitable ``find``
utility must be compiled into librecoll. By default it will be discovered
in the system PATH, to override its location set ``FindProgram_EXECUTABLE``
to a suitable path.

Usage
^^^^^

cd .../recoll/src

cmake -S . -B build_dir \
      --install-prefix=/usr/local/ \
      -G Ninja \                    # or -G "Unix Makefiles"
      -DCMAKE_BUILD_TYPE=Release \
      -DRECOLL_QT6_BUILD=1 \        # for Qt6
      -DRECOLL_ENABLE_WEBENGINE=1 \ # use QtWebEngine instead of QtWebKit
      -DRECOLL_ENABLE_X11MON=1 \    # turn on X11 session monitoring
      -DFindProgram_EXECUTABLE=/opt/bin/find

# omit `-G Ninja` to use classical unix makefile as generator

cmake --build build_dir --parallel

cmake --install ./build_dir/

Toggling Options

cmake -Dsome_option=ON .....

macOS
^^^^^

To assemble a portable .app bundle, you must use the Qt from official installer.
The Homebrew Qt doesn't work, this is a known bug and unlikely to be fixed in short time.

To specify qt path, specify `CMAKE_PREFIX_PATH` when generating the build_dir

cmake -DCMAKE_PREFIX_PATH="~/Qt/6.4.2/macos" ....

 ~/Qt/6.4.2/macos/bin/macdeployqt recoll.app

To completely avoid codesign problems, just force one #TODO: use personal codesign

codesign --force --deep -s - recoll.app

#]===========================================================================]

cmake_minimum_required(VERSION 3.25)

# Load versions from text files
file(READ "RECOLL-VERSION.txt" VERSION_FILE_VERSION)
string(STRIP "${VERSION_FILE_VERSION}" VERSION_FILE_VERSION)
# Strip trailing version strings like "X.Y.ZpreN" that CMake doesn't support
string(REGEX REPLACE
    "^([0-9]+\\.[0-9]+\\.[0-9]+).*$" "\\1"
    RECOLL_VERSION "${VERSION_FILE_VERSION}"
)
file(READ "RECOLL-SOVERSION.txt" RECOLL_SOVERSION)
string(STRIP "${RECOLL_SOVERSION}" RECOLL_SOVERSION)

set(_parse_state
    "VERSION_FILE_VERSION=${VERSION_FILE_VERSION}, \
    RECOLL_VERSION=${RECOLL_VERSION}, \
    RECOLL_SOVERSION=${RECOLL_SOVERSION}")
string(LENGTH "${RECOLL_VERSION}" _rcl_ver_len)
string(LENGTH "${RECOLL_SOVERSION}" _rcl_sover_len)
if (_rcl_ver_len LESS_EQUAL 0 OR _rcl_sover_len LESS_EQUAL 0)
    message(FATAL_ERROR
        "Failed to parse version files!\nGot: ${_parse_state}")
endif()
message(DEBUG "Got: ${_parse_state}")

project(recoll
    VERSION ${RECOLL_VERSION}
    LANGUAGES CXX C)

# For our custom Find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(GNUInstallDirs)
include(FeatureSummary)

### Configure Options

option(RECOLL_ENABLE_X11MON
    "recollindex support for X11 session monitoring (Linux only)" OFF)
option(RECOLL_ENABLE_WEBENGINE
    "Build with QtWebEngine (instead of QtWebKit)" ON)
option(RECOLL_ENABLE_LIBMAGIC
    "Enable libmagic (avoids 'file' command requirement)" OFF)
option(RECOLL_QT6_BUILD "Build using Qt6" ON)
option(RECOLL_ENABLE_SYSTEMD "Install systemd unit files (linux only)" ON)
option(RECOLL_QTGUI "Build GUI" ON)

if (RECOLL_QT6_BUILD)
    set(QT_MAJOR_VERSION 6)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
else()
    set(QT_MAJOR_VERSION 5)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()


# simulate autoheader
configure_file(common/autoconfig.h.cmake.in autoconfig.h @ONLY)


### Source files
set(librecoll_SOURCES
    aspell/rclaspell.cpp
    aspell/rclaspell.h
    bincimapmime/convert.cc
    bincimapmime/convert.h
    bincimapmime/mime-inputsource.h
    bincimapmime/mime-parsefull.cc
    bincimapmime/mime-parseonlyheader.cc
    bincimapmime/mime-printbody.cc
    bincimapmime/mime.cc
    bincimapmime/mime.h
    common/webstore.cpp
    common/webstore.h
    common/cjksplitter.cpp
    common/cjksplitter.h
    common/cnsplitter.cpp
    common/cnsplitter.h
    common/cstr.cpp
    common/cstr.h
    common/kosplitter.cpp
    common/rclconfig.cpp
    common/rclconfig.h
    common/rclinit.cpp
    common/rclinit.h
    common/rclutil.cpp
    common/rclutil.h
    common/syngroups.cpp
    common/syngroups.h
    common/textsplit.cpp
    common/textsplit.h
    common/unacpp.cpp
    common/unacpp.h
    common/uproplist.h
    index/webqueuefetcher.cpp
    index/webqueuefetcher.h
    index/checkretryfailed.cpp
    index/checkretryfailed.h
    index/exefetcher.cpp
    index/exefetcher.h
    index/fetcher.cpp
    index/fetcher.h
    index/fsfetcher.cpp
    index/fsfetcher.h
    index/idxdiags.h
    index/idxdiags.cpp
    index/idxstatus.h
    index/idxstatus.cpp
    index/mimetype.cpp
    index/mimetype.h
    index/rclmon.h
    index/recollindex.h
    index/subtreelist.cpp
    index/subtreelist.h
    internfile/Filter.h
    internfile/extrameta.cpp
    internfile/extrameta.h
    internfile/htmlparse.cpp
    internfile/htmlparse.h
    internfile/indextext.h
    internfile/internfile.cpp
    internfile/internfile.h
    internfile/mh_exec.cpp
    internfile/mh_exec.h
    internfile/mh_execm.cpp
    internfile/mh_execm.h
    internfile/mh_html.cpp
    internfile/mh_html.h
    internfile/mh_mail.cpp
    internfile/mh_mail.h
    internfile/mh_mbox.cpp
    internfile/mh_mbox.h
    internfile/mh_null.h
    internfile/mh_symlink.h
    internfile/mh_text.cpp
    internfile/mh_text.h
    internfile/mh_unknown.h
    internfile/mh_xslt.cpp
    internfile/mh_xslt.h
    internfile/mimehandler.cpp
    internfile/mimehandler.h
    internfile/myhtmlparse.cpp
    internfile/myhtmlparse.h
    internfile/txtdcode.cpp
    internfile/uncomp.cpp
    internfile/uncomp.h
    query/docseq.cpp
    query/docseq.h
    query/docseqdb.cpp
    query/docseqdb.h
    query/docseqdocs.h
    query/docseqhist.cpp
    query/docseqhist.h
    query/dynconf.cpp
    query/dynconf.h
    query/filtseq.cpp
    query/filtseq.h
    common/plaintorich.cpp
    common/plaintorich.h
    query/qresultstore.cpp
    query/qresultstore.h
    query/recollq.cpp
    query/recollq.h
    query/reslistpager.cpp
    query/reslistpager.h
    query/sortseq.cpp
    query/sortseq.h
    query/wasaparse.cpp # generated via wasaparse.ypp
    query/wasaparseaux.cpp
    query/wasaparserdriver.h
    query/wasatorcl.h
    rcldb/daterange.cpp
    rcldb/daterange.h
    rcldb/expansiondbs.cpp
    rcldb/expansiondbs.h
    rcldb/rclabstract.cpp
    rcldb/rclabsfromtext.cpp
    rcldb/rcldb.cpp
    rcldb/rclndb.cpp
    rcldb/rcldb.h
    rcldb/rcldb_p.h
    rcldb/rcldoc.cpp
    rcldb/rcldoc.h
    rcldb/rcldups.cpp
    rcldb/rclquery.cpp
    rcldb/rclquery.h
    rcldb/rclquery_p.h
    rcldb/rclterms.cpp
    rcldb/rclvalues.cpp
    rcldb/rclvalues.h
    rcldb/searchdata.cpp
    rcldb/searchdata.h
    rcldb/searchdatatox.cpp
    rcldb/searchdataxml.cpp
    rcldb/stemdb.cpp
    rcldb/stemdb.h
    rcldb/stoplist.cpp
    rcldb/stoplist.h
    rcldb/synfamily.cpp
    rcldb/synfamily.h
    rcldb/termproc.h
    rcldb/xmacros.h
    unac/unac.cpp
    unac/unac.h
    unac/unac_version.h
    utils/appformime.cpp
    utils/appformime.h
    utils/base64.cpp
    utils/base64.h
    utils/cancelcheck.cpp
    utils/cancelcheck.h
    utils/chrono.h
    utils/chrono.cpp
    utils/circache.cpp
    utils/circache.h
    utils/closefrom.cpp
    utils/closefrom.h
    utils/cmdtalk.cpp
    utils/cmdtalk.h
    utils/conftree.cpp
    utils/conftree.h
    utils/copyfile.cpp
    utils/copyfile.h
    utils/cpuconf.cpp
    utils/cpuconf.h
    utils/damlev.h
    utils/ecrontab.cpp
    utils/ecrontab.h
    utils/execmd.cpp
    utils/execmd.h
    utils/fileudi.cpp
    utils/fileudi.h
    utils/fstreewalk.cpp
    utils/fstreewalk.h
    utils/hldata.h
    utils/hldata.cpp
    utils/idfile.cpp
    utils/idfile.h
    utils/listmem.cpp
    utils/listmem.h
    utils/log.cpp
    utils/log.h
    utils/md5.cpp
    utils/md5.h
    utils/md5ut.cpp
    utils/md5ut.h
    utils/mimeparse.cpp
    utils/mimeparse.h
    utils/miniz.cpp
    utils/miniz.h
    utils/netcon.cpp
    utils/netcon.h
    utils/pathut.cpp
    utils/pathut.h
    utils/picoxml.h
    utils/pxattr.cpp
    utils/pxattr.h
    utils/rclionice.cpp
    utils/rclionice.h
    utils/readfile.cpp
    utils/readfile.h
    utils/smallut.cpp
    utils/smallut.h
    utils/strmatcher.cpp
    utils/strmatcher.h
    utils/transcode.cpp
    utils/transcode.h
    utils/utf8iter.cpp
    utils/utf8iter.h
    utils/wipedir.cpp
    utils/wipedir.h
    utils/workqueue.h
    utils/x11mon.cpp
    utils/x11mon.h
    utils/zlibut.cpp
    utils/zlibut.h
    xaposix/safeunistd.h
    )

set(recollindex_SOURCES
    index/checkindexed.cpp
    index/checkindexed.h
    index/fsindexer.cpp
    index/fsindexer.h
    index/indexer.cpp
    index/indexer.h
    index/rclmonprc.cpp
    index/rclmonrcv.cpp
    index/recollindex.cpp
    index/webqueue.cpp
    index/webqueue.h
    )

set(recollq_SOURCE
    query/recollqmain.cpp
    query/recollq.cpp
    query/recollq.h)


set(example_config_files
    sampleconf/fields
    sampleconf/fragment-buttons.xml
    sampleconf/mimeconf
    sampleconf/mimemap
    sampleconf/mimeview
    sampleconf/recoll-common.css
    sampleconf/recoll-common.qss
    sampleconf/recoll-dark.css
    sampleconf/recoll-dark.qss
    sampleconf/recoll.conf
    sampleconf/recoll.qss
    )

set(filter_files
    python/recoll/recoll/rclconfig.py
    python/recoll/recoll/conftree.py
    filters/abiword.xsl
    filters/cmdtalk.py
    filters/fb2.xsl
    filters/gnumeric.xsl
    filters/kosplitter.py
    filters/msodump.zip
    filters/okular-note.xsl
    filters/opendoc-body.xsl
    filters/opendoc-flat.xsl
    filters/opendoc-meta.xsl
    filters/openxml-meta.xsl
    filters/openxml-word-body.xsl
    filters/openxml-xls-body.xsl
    filters/ppt-dump.py
    filters/rcl7z.py
    filters/rclaptosidman
    filters/rclaspell-sugg.py
    filters/rclaudio.py
    filters/rclbasehandler.py
    filters/rclbibtex.sh
    filters/rclcheckneedretry.sh
    filters/rclchm.py
    filters/rcldia.py
    filters/rcldjvu.py
    filters/rcldoc.py
    filters/rcldvi
    filters/rclepub.py
    filters/rclepub1.py
    filters/rclexec1.py
    filters/rclexecm.py
    filters/rclfb2.py
    filters/rclgaim
    filters/rclgenxslt.py
    filters/rclhwp.py
    filters/rclics.py
    filters/rclimg
    filters/rclimg.py
    filters/rclinfo.py
    filters/rclipynb.py
    filters/rclkar.py
    filters/rclkwd
    filters/rcllatinclass.py
    filters/rcllatinstops.zip
    filters/rcllyx
    filters/rclman
    filters/rclmidi.py
    filters/rclocr.py
    filters/rclocrabbyy.py
    filters/rclocrcache.py
    filters/rclocrtesseract.py
    filters/rclopxml.py
    filters/rclorgmode.py
    filters/rclpages.py
    filters/rclpdf.py
    filters/rclppt.py
    filters/rclps
    filters/rclpst.py
    filters/rclpurple
    filters/rclpython.py
    filters/rclrar.py
    filters/rclrtf.py
    filters/rclscribus
    filters/rclshowinfo
    filters/rcltar.py
    filters/rcltex
    filters/rcltext.py
    filters/rcluncomp
    filters/rcluncomp.py
    filters/rclwar.py
    filters/rclxls.py
    filters/rclxml.py
    filters/rclxmp.py
    filters/rclxslt.py
    filters/rclzip.py
    filters/recoll-we-move-files.py
    filters/recollepub.zip
    filters/svg.xsl
    filters/thunderbird-open-message.sh
    filters/xls-dump.py
    filters/xlsxmltocsv.py
    filters/xml.xsl
    )

set(mtpics_files
    qtgui/mtpics/aptosid-book.png
    qtgui/mtpics/aptosid-manual.png
    qtgui/mtpics/archive.png
    qtgui/mtpics/book.png
    qtgui/mtpics/bookchap.png
    qtgui/mtpics/document.png
    qtgui/mtpics/drawing.png
    qtgui/mtpics/emblem-symbolic-link.png
    qtgui/mtpics/folder.png
    qtgui/mtpics/html.png
    qtgui/mtpics/image.png
    qtgui/mtpics/message.png
    qtgui/mtpics/mozilla_doc.png
    qtgui/mtpics/pdf.png
    qtgui/mtpics/pidgin.png
    qtgui/mtpics/postscript.png
    qtgui/mtpics/presentation.png
    qtgui/mtpics/sidux-book.png
    qtgui/mtpics/soffice.png
    qtgui/mtpics/source.png
    qtgui/mtpics/sownd.png
    qtgui/mtpics/spreadsheet.png
    qtgui/mtpics/text-x-python.png
    qtgui/mtpics/txt.png
    qtgui/mtpics/video.png
    qtgui/mtpics/wordprocessing.png
    )

set(docs_files
    doc/user/usermanual.html
    doc/user/docbook-xsl.css)

set(bundle_icon qtgui/images/recoll.icns)

set_source_files_properties(${bundle_icon} PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
set_source_files_properties(${example_config_files} PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources/examples)
set_source_files_properties(${filter_files} PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources/filters)
set_source_files_properties(${mtpics_files} PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources/images)
set_source_files_properties(${docs_files} PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources/doc)


set(COMMON_INCLUDES_DIRS
    bincimapmime
    common
    index
    internfile
    rcldb
    unac
    utils
    xaposix
)

### Targets

# Libraries shared by all targets. The cmake manual says include_directories() should be avoided, btw.
include_directories(
    ${PROJECT_BINARY_DIR} # for autoconfig.h
    ${COMMON_INCLUDES_DIRS}
)

#--- librecoll

if (LINUX)
    add_library(librecoll SHARED ${librecoll_SOURCES})
else ()
    add_library(librecoll STATIC ${librecoll_SOURCES})
endif ()

set_target_properties(librecoll PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${RECOLL_SOVERSION}
    OUTPUT_NAME recoll
    POSITION_INDEPENDENT_CODE TRUE
)

target_compile_definitions(librecoll PUBLIC
    BUILDING_RECOLL
    READFILE_ENABLE_ZLIB
    READFILE_ENABLE_MINIZ
    READFILE_ENABLE_MD5
    HAVE_DLOPEN
    RECOLL_DATADIR="${CMAKE_INSTALL_PREFIX}/share/recoll"
)


### External Libraries


if (LINUX AND RECOLL_ENABLE_X11MON)
    find_package(X11 REQUIRED) # https://cmake.org/cmake/help/latest/module/FindX11.html
    target_link_libraries(librecoll PUBLIC ${X11MON_LIBRARIES})
    target_include_directories(librecoll PUBLIC ${X11MON_INCLUDE_DIRS})
else()
    target_compile_definitions(librecoll PUBLIC DISABLE_X11MON)
endif()

# To know ${VARS} provided with find_package, check the urls

find_package(LibXml2 REQUIRED) # https://cmake.org/cmake/help/latest/module/FindLibXml2.html
find_package(LibXslt REQUIRED) # https://cmake.org/cmake/help/latest/module/FindLibXslt.html
find_package(Xapian REQUIRED) # https://github.com/xapian/xapian/blob/master/xapian-core/cmake/xapian-config.cmake.in

target_link_libraries(librecoll PUBLIC
    LibXml2::LibXml2
    LibXslt::LibXslt
    ${XAPIAN_LIBRARIES}
)

include_directories(${XAPIAN_INCLUDE_DIR})

if (RECOLL_ENABLE_LIBMAGIC)
    find_package(Libmagic REQUIRED)
    target_link_libraries(librecoll PUBLIC Libmagic::Libmagic)
else()
    find_package(FileCommand REQUIRED)
    target_link_libraries(librecoll PUBLIC FileCommand::FILE_PROG)
endif()

if (APPLE)
    # assuming packages are installed via homebrew.
    include_directories(/usr/local/include /opt/homebrew/include)
    # External dependencies will be copied automatically via macdeployqt
    find_package(ZLIB REQUIRED) # https://cmake.org/cmake/help/latest/module/FindZLIB.html
    find_package(Iconv REQUIRED) # https://cmake.org/cmake/help/latest/module/FindIconv.html
    target_link_libraries(librecoll PUBLIC ZLIB::ZLIB Iconv::Iconv)
endif ()

#--- recollindex ---

add_executable(recollindex ${recollindex_SOURCES})
target_link_libraries(recollindex PRIVATE librecoll)

#--- recollq ---

add_executable(recollq ${recollq_SOURCE})
target_link_libraries(recollq PRIVATE librecoll)

set(RECOLL_EXECUTABLES recollq recollindex)
if(RECOLL_QTGUI)
    add_subdirectory(qtgui)
    set(RECOLL_EXECUTABLES ${RECOLL_EXECUTABLES} recoll)
endif()

### Install targets and other runtime files

if (APPLE)
    target_compile_definitions(recoll PRIVATE RECOLL_AS_MAC_BUNDLE)

    # .app bundle assembling
    target_sources(recoll PRIVATE
        ${example_config_files}
        ${filter_files}
        ${mtpics_files}
        ${bundle_icon}
        ${docs_files}
    )
    set_target_properties(recollindex recollq PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/recoll.app/Contents/MacOS/)
endif()


if (LINUX)
    set_target_properties(${RECOLL_EXECUTABLES} PROPERTIES
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )

    if (RECOLL_ENABLE_SYSTEMD)
        find_package(SystemdPaths)

	install(FILES index/recollindex.service
	    DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR})
	install(FILES index/recollindex@.service
            DESTINATION ${SYSTEMD_USER_UNIT_DIR})
    endif()

    install(TARGETS librecoll ${RECOLL_EXECUTABLES}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(FILES desktop/recoll-searchgui.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES desktop/org.recoll.recoll.appdata.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)

    install(FILES desktop/recoll.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps)
    install(FILES desktop/recoll.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/pixmaps)
    install(FILES desktop/recoll.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

    install(FILES ${docs_files}
       DESTINATION ${CMAKE_INSTALL_DATADIR}/recoll/doc)

    install(FILES
        doc/man/recollindex.1
        doc/man/recollq.1
        doc/man/recoll.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
    install(FILES doc/man/recoll.conf.5
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)

    install(FILES ${example_config_files}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/recoll/examples)
    install(FILES ${filter_files}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/recoll/filters)
    install(FILES ${mtpics_files}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/recoll/images)

    install(FILES ${QM_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/recoll/translations)
endif ()

feature_summary(WHAT ALL DESCRIPTION "Build configuration:")
