TARGET = xapian-check
QT       -= core gui
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app

DEFINES += HAVE_CONFIG_H

RECOLLDEPS = $$PWD/../../../recolldeps/msvc
XAPIANDIR = $$RECOLLDEPS/xapian-core/

SOURCES += \
        $$XAPIANDIR/bin/xapian-check.cc

INCLUDEPATH += $$XAPIANDIR \
            $$XAPIANDIR/include \
            $$XAPIANDIR/common

windows {
  DEFINES += __WIN32__
  DEFINES += UNICODE
  contains(QMAKE_CC, cl){
    # msvc
    QCBUILDLOC = Desktop_Qt_6_7_3_MSVC2019_64bit
    DEFINES += USING_STATIC_LIBICONV
    INCLUDEPATH += \
      ../../../recolldeps/msvc/zlib-1.2.11/ \
      ../../../recolldeps/msvc/wlibiconv/include
    LIBS += \
      $$PWD/build/libxapian/$$QCBUILDLOC-Release/release/libxapian.lib \
      $$RECOLLDEPS/wlibiconv/build/$$QCBUILDLOC-Release/release/iconv.lib \
      $$RECOLLDEPS/zlib-1.2.11/zdll.lib \
      -lrpcrt4 -lws2_32 -luser32 -lshell32 -lshlwapi -lpsapi -lkernel32
   }
}
