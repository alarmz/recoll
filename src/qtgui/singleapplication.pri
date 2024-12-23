QT += core network
CONFIG += c++11

HEADERS += \
    $$PWD/singleapplication/singleapplication.h \
    $$PWD/singleapplication/singleapplication_p.h
SOURCES += $$PWD/singleapplication/singleapplication.cpp \
    $$PWD/singleapplication/singleapplication_p.cpp

INCLUDEPATH += $$PWD/singleapplication
DEFINES += QAPPLICATION_CLASS=QApplication

win32 {
    msvc:LIBS += Advapi32.lib
    gcc:LIBS += -ladvapi32
}
