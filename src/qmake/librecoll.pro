#-------------------------------------------------
#
# Project created by QtCreator 2015-10-03T09:04:49
#
#-------------------------------------------------

QT       -= core gui

TARGET = recoll
TEMPLATE = lib

CONFIG += c++17

DEFINES += BUILDING_RECOLL
DEFINES += READFILE_ENABLE_MINIZ
DEFINES += READFILE_ENABLE_MD5
DEFINES += READFILE_ENABLE_ZLIB

SOURCES += \
../aspell/rclaspell.cpp \
../bincimapmime/convert.cc \
../bincimapmime/mime-parsefull.cc \
../bincimapmime/mime-parseonlyheader.cc \
../bincimapmime/mime-printbody.cc \
../bincimapmime/mime.cc \
../common/cjksplitter.cpp \
../common/cnsplitter.cpp \
../common/kosplitter.cpp \
../common/cstr.cpp \
../common/plaintorich.cpp \
../common/rclconfig.cpp \
../common/rclinit.cpp \
../common/rclutil.cpp \
../common/syngroups.cpp \
../common/textsplit.cpp \
../common/unacpp.cpp \
../common/webstore.cpp \
../index/exefetcher.cpp \
../index/fetcher.cpp \
../index/fsfetcher.cpp \
../index/idxdiags.cpp \
../index/idxstatus.cpp \
../index/mimetype.cpp \
../index/subtreelist.cpp \
../index/webqueuefetcher.cpp \
../internfile/extrameta.cpp \
../internfile/htmlparse.cpp \
../internfile/internfile.cpp \
../internfile/mh_exec.cpp \
../internfile/mh_execm.cpp \
../internfile/mh_html.cpp \
../internfile/mh_mail.cpp \
../internfile/mh_mbox.cpp \
../internfile/mh_text.cpp \
../internfile/mh_xslt.cpp \
../internfile/mimehandler.cpp \
../internfile/myhtmlparse.cpp \
../internfile/txtdcode.cpp \
../internfile/uncomp.cpp \
../query/docseq.cpp \
../query/docseqdb.cpp \
../query/docseqhist.cpp \
../query/dynconf.cpp \
../query/filtseq.cpp \
../query/qresultstore.cpp \
../query/recollq.cpp \
../query/reslistpager.cpp \
../query/sortseq.cpp \
../query/wasaparse.cpp \
../query/wasaparseaux.cpp \
../rcldb/daterange.cpp \
../rcldb/expansiondbs.cpp \
../rcldb/rclabsfromtext.cpp \
../rcldb/rclabstract.cpp \
../rcldb/rcldb.cpp \
../rcldb/rcldoc.cpp \
../rcldb/rcldups.cpp \
../rcldb/rclndb.cpp \
../rcldb/rclquery.cpp \
../rcldb/rclterms.cpp \
../rcldb/rclvalues.cpp \
../rcldb/rclvalues.h \
../rcldb/searchdata.cpp \
../rcldb/searchdatatox.cpp \
../rcldb/searchdataxml.cpp \
../rcldb/stemdb.cpp \
../rcldb/stoplist.cpp \
../rcldb/synfamily.cpp \
../unac/unac.cpp \
../utils/appformime.cpp \
../utils/base64.cpp \
../utils/cancelcheck.cpp \
../utils/chrono.cpp \
../utils/circache.cpp \
../utils/closefrom.cpp \
../utils/cmdtalk.cpp \
../utils/conftree.cpp \
../utils/copyfile.cpp \
../utils/cpuconf.cpp \
../utils/ecrontab.cpp \
../utils/fileudi.cpp \
../utils/fstreewalk.cpp \
../utils/hldata.cpp \
../utils/idfile.cpp \
../utils/listmem.cpp \
../utils/log.cpp \
../utils/md5.cpp \
../utils/md5ut.cpp \
../utils/mimeparse.cpp \
../utils/miniz.cpp \
../utils/pathut.cpp \
../utils/pxattr.cpp \
../utils/readfile.cpp \
../utils/smallut.cpp \
../utils/strmatcher.cpp \
../utils/transcode.cpp \
../utils/utf8iter.cpp \
../utils/zlibut.cpp \
../utils/zlibut.h \
../utils/wipedir.cpp

INCLUDEPATH += ../common ../index ../internfile ../query ../unac ../utils ../aspell \
  ../rcldb ../qtgui ../xaposix ../confgui ../bincimapmime 

windows {
  # VC only defines _WIN32, not __WIN32__ . For some reason xapian uses __WIN32__ which it actually
  # defines in conf_post.h if _WIN32 is set. Reason: mystery.
  DEFINES += __WIN32__
  DEFINES += PSAPI_VERSION=1
  defines += UNICODE

  SOURCES += \
    ../windows/execmd_w.cpp \
    ../windows/fnmatch.c \
    ../windows/wincodepages.cpp \
    ../windows/strptime.cpp

  # Look at pre-2004-04 versions for a gcc/mingw section. Was removed because not used any more
  
  contains(QMAKE_CC, cl){
    # MSVC
    RECOLLDEPS = ../../../recolldeps
    CONFIG += staticlib
    DEFINES += USING_STATIC_LIBICONV

    INCLUDEPATH += ../windows \
      $$RECOLLDEPS/msvc/xapian-core/include \
      $$RECOLLDEPS/msvc/zlib-1.2.11/ \
      $$RECOLLDEPS/msvc/libxslt/libxslt-1.1.29/ \
      $$RECOLLDEPS/msvc/libxml2/libxml2-2.9.4+dfsg1/include \
      $$RECOLLDEPS/msvc/wlibiconv/include \
      $$RECOLLDEPS/msvc/libmagic/src
  }

}

unix:!mac {
    target.path = /usr/lib
    INSTALLS += target
    INCLUDEPATH += \
        /usr/include/libxml2
    SOURCES += \
       ../utils/execmd.cpp \
       ../utils/x11mon.cpp
}

mac {
    CONFIG += staticlib
    DEFINES += RECOLL_AS_MAC_BUNDLE
    QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64
    # This is necessary to avoid an undefined impl__xmlFree.
    # See comment in libxml/xmlexports.h
    DEFINES += LIBXML_STATIC
    RECOLLDEPS = ../../../
    QMAKE_CXXFLAGS += -pthread -Wno-unused-parameter
    INCLUDEPATH += \
      $$RECOLLDEPS/xapian-core-1.4.24/include \
      $$RECOLLDEPS/libmagic/src
    SOURCES += \
      ../internfile/finderxattr.cpp
}
