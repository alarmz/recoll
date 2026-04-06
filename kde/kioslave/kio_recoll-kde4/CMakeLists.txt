cmake_minimum_required(VERSION 2.6)

project(kio_recoll)

find_package(KDE4 REQUIRED)
set(CMAKE_SKIP_RPATH TRUE)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
add_definitions(-DKDE_DEFAULT_DEBUG_AREA=7130
    -DRECOLL_DATADIR="${CMAKE_INSTALL_PREFIX}/share/recoll"
    -DLIBDIR="${CMAKE_INSTALL_PREFIX}/lib"
)
set(CMAKE_CXX_FLAGS "-fPIC ${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}") 

set(rcltop ${CMAKE_CURRENT_SOURCE_DIR}/../../../)

include_directories (${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES}
  ${rcltop}/common 
  ${rcltop}/internfile 
  ${rcltop}/query 
  ${rcltop}/rcldb 
  ${rcltop}/utils 
  ${rcltop}/redhat-linux-build
)

include_directories (
    ${CMAKE_INSTALL_PREFIX}/include/recoll
)

# Check if running inside a build tree, then use the not yet installed lib. 
foreach(libdir IN ITEMS  ${CMAKE_BINARY_DIR}/../../.libs
                         ${CMAKE_BINARY_DIR}/../../../.libs
                         ${CMAKE_BINARY_DIR}/../../../../.libs
                         ${CMAKE_BINARY_DIR}/../../../../redhat-linux-build
                         $ENV{RECOLL_LIB_DIR}/
                         )
    message(STATUS "TESTING link directory ${libdir}")
    if (EXISTS ${libdir}/librecoll.so)
        message(STATUS "USING link directory ${libdir}")
        link_directories(${CMAKE_INSTALL_PREFIX}/lib ${libdir})
    endif()
endforeach()

set(kio_recoll_SRCS  kio_recoll.cpp htmlif.cpp dirif.cpp)

CHECK_LIBRARY_EXISTS(dl dlopen "" DLOPEN_IN_LIBDL)
IF(DLOPEN_IN_LIBDL)
	LIST(APPEND EXTRA_LIBS dl)
ENDIF(DLOPEN_IN_LIBDL)
CHECK_LIBRARY_EXISTS(pthread pthread_sigmask "" PTHREAD_IN_LIBPTHREAD)
IF(PTHREAD_IN_LIBPTHREAD)
	LIST(APPEND EXTRA_LIBS pthread)
ENDIF(PTHREAD_IN_LIBPTHREAD)

kde4_add_plugin(kio_recoll ${kio_recoll_SRCS})

target_link_libraries(kio_recoll recoll xapian xslt xml2 z ${EXTRA_LIBS} ${KDE4_KIO_LIBS} X11)

install(TARGETS kio_recoll DESTINATION ${PLUGIN_INSTALL_DIR})

IF ("${KDE_VERSION_MAJOR}.${KDE_VERSION_MINOR}" GREATER 4.0)
     install(FILES recoll.protocol recollf.protocol DESTINATION ${SERVICES_INSTALL_DIR})
ELSE ("${KDE_VERSION_MAJOR}.${KDE_VERSION_MINOR}" GREATER 4.0)
   install(FILES recollnolist.protocol DESTINATION ${SERVICES_INSTALL_DIR}
   		 RENAME recoll.protocol)
ENDIF ("${KDE_VERSION_MAJOR}.${KDE_VERSION_MINOR}" GREATER 4.0)

install(FILES data/welcome.html	data/help.html
	      DESTINATION  ${DATA_INSTALL_DIR}/kio_recoll)
