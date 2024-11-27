
QT       -= core gui

TARGET = recollq
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app

DEFINES += BUILDING_RECOLL

SOURCES += \
../query/recollqmain.cpp

INCLUDEPATH += ../common ../index ../internfile ../query ../unac ../utils ../aspell \
    ../rcldb ../qtgui ../xaposix ../confgui ../bincimapmime 

windows {
  DEFINES += UNICODE
  DEFINES += PSAPI_VERSION=1
  DEFINES += __WIN32__

  contains(QMAKE_CC, cl){
    # Visual Studio
    QCBUILDLOC = Desktop_Qt_6_7_3_MSVC2019_64bit
    RECOLLDEPS = $$PWD/../../../recolldeps/msvc
    SOURCES += ../windows/getopt.cc
    PRE_TARGETDEPS = \
      $$PWD/../qmake/build/librecoll/$$QCBUILDLOC-Release/release/recoll.lib
    LIBS += \
      $$PWD/../qmake/build/librecoll/$$QCBUILDLOC-Release/release/recoll.lib \
      $$RECOLLDEPS/libxml2/libxml2-2.9.4+dfsg1/win32/bin.msvc/libxml2.lib \
      $$RECOLLDEPS/libxslt/libxslt-1.1.29/win32/bin.msvc/libxslt.lib \
      $$PWD/build/libxapian/$$QCBUILDLOC-Release/release/libxapian.lib \
      $$RECOLLDEPS/wlibiconv/build/$$QCBUILDLOC-Release/release/iconv.lib \
      $$RECOLLDEPS/libmagic/src/lib/libmagic.lib \
      $$RECOLLDEPS/regex/libregex.lib \
      $$RECOLLDEPS/zlib-1.2.11/zdll.lib \
      -lrpcrt4 -lws2_32 -luser32 -lshell32 -lshlwapi -lpsapi -lkernel32
  }

  INCLUDEPATH += ../windows
}

unix:!mac {
    QCBUILDLOC=Desktop
    SOURCES += \
      ../utils/execmd.cpp \
      ../utils/netcon.cpp

    PRE_TARGETDEPS = \
      ../build-librecoll-$$QCBUILDLOC-Release/librecoll.so
    LIBS += \
      -L../build-librecoll-$$QCBUILDLOC-Release/ -lrecoll \
      -lxapian -lxslt -lxml2 -lmagic -lz -lX11
}

mac {
  QCBUILDLOC=Qt_6_7_3_for_macOS
  RECOLLDEPS = $$PWD/../../..
  QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64
  QMAKE_CXXFLAGS += -std=c++11 -pthread -Wno-unused-parameter
  DEFINES += RECOLL_AS_MAC_BUNDLE
  SOURCES += \
    ../utils/execmd.cpp \
    ../utils/netcon.cpp \
    ../utils/rclionice.cpp
  PRE_TARGETDEPS = $$PWD/build/librecoll/$$QCBUILDLOC-Release/librecoll.a
  LIBS += \
     $$PWD/build/librecoll/$$QCBUILDLOC-Release/librecoll.a \
     $$PWD/build/libxapian/$$QCBUILDLOC-Release/liblibxapian.a \
     $$RECOLLDEPS/libmagic/src/.libs/libmagic.a \
     -lxslt -lxml2 -liconv -lz
}
