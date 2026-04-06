depth=../../..
include $(depth)/mk/sysconf

# Need to set this by hand until we decide how to autoconf this without
# bringing 2mbytes of kde config files.
# May also need to adjust stuff such as the --rpath's below
#
KDE_INCLUDES=/usr/include/kde
QT_INCLUDES=/usr/include/qt3


all: kio_recoll.so 

DEPS_CXXFLAGS =  -MT pop3.lo -MD -MP -MF .deps/pop3.Tpo

INC_CXXFLAGS = -I. \
	-I$(KDE_INCLUDES) -I$(QT_INCLUDES) \
	     -I$(depth)/common -I$(depth)/query -I$(depth)/utils \
	     -I$(depth)/qtgui -I$(depth)/rcldb

PIC_CXXFLAGS = -fPIC -DPIC
DEBUG_CXXFLAGS = -DNDEBUG -DNO_DEBUG -O2 -O
LANG_CXXFLAGS = -fno-exceptions -fno-check-new -fno-common
QT_CXXFLAGS = -DQT_CLEAN_NAMESPACE -DQT_NO_ASCII_CAST -DQT_NO_STL \
	    -DQT_NO_COMPAT -DQT_NO_TRANSLATION -DQT_THREAD_SUPPORT
SYS_CXXFLAGS = -D_GNU_SOURCE 
THREAD_CXXFLAGS = -D_THREAD_SAFE -pthread -D_THREAD_SAFE -pthread


# -rpath=/usr/lib:/usr/local/lib \

LDFLAGS = \
	 -Wl,--rpath -Wl,/usr/local/lib \
	-Wl,--rpath -Wl,/usr/X11R6/lib \
	-Wl,-export-dynamic -Wl,-soname -Wl,kio_recoll.so
THREAD_LDFLAGS = -pthread

kio_recoll.so : kio_recoll.o libpic
	c++ -shared $(LDFLAGS) $(THREAD_LDFLAGS) \
	    -Wl,--no-undefined \
	    kio_recoll.o piclib/librcl.a \
	    $(LIBXAPIAN) $(LIBICONV) \
	    -L/opt/kde3/lib -L/usr/local/lib -L/usr/X11R6/lib -lkio -lkdecore \
	    -L/usr/lib/qt3/lib -lqt-mt \
	    -L/usr/lib -lstdc++ -lm -lc \
	    -o kio_recoll.so 

kio_recoll.o : kio_recoll.cpp kio_recoll.h
	$(CXX) -c -pipe kio_recoll.cpp  $(INC_CXXFLAGS) $(PIC_CXXFLAGS) \
	$(DEBUG_CXXFLAGS) $(LANG_CXXFLAGS) $(QT_CXXFLAGS) $(SYS_CXXFLAGS) \
	$(THREAD_CXXFLAGS) \
	-o kio_recoll.o

libpic:
	cd piclib;test -f Makefile || depth=$(depth)/.. sh $(depth)/../lib/mkMake
	cd piclib;$(MAKE) CXXFLAGS="$(CXXFLAGS) $(PIC_CXXFLAGS)" \
	   CFLAGS="$(CFLAGS) $(PIC_CXXFLAGS)"

.PHONY: all libpic
