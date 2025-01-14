# KF5 version of the recoll krunner module.
# Create from https://develop.kde.org/docs/extend/plasma/krunner/ as example
# Dependancies:
#  ki18n-devel plasma-framework-devel kconfig-devel kpackage-devel
#  krunner-devel knotifications-devel

cmake_minimum_required(VERSION 3.16)
project(recollrunner)

option(RECOLL_PUBLIC_LIB "Does nothing, kept for compat" 1)

set(KF5_MIN_VERSION "5.0")
set(CMAKE_SKIP_RPATH TRUE)

# Include the Extra-CMake-Modules project
find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS I18n Config Runner KIO Notifications Config)

## Recoll stuff
set(rcltop ${CMAKE_CURRENT_SOURCE_DIR}/../../)

add_definitions(
    -DRECOLL_DATADIR="${CMAKE_INSTALL_PREFIX}/share/recoll"
    -DLIBDIR="${CMAKE_INSTALL_PREFIX}/lib"
)


include_directories (
  ${CMAKE_SOURCE_DIR}
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
                         ${CMAKE_BINARY_DIR}/../../../redhat-linux-build
                         $ENV{RECOLL_LIB_DIR}/
                         )
    message(STATUS "TESTING link directory ${libdir}")
    if (EXISTS ${libdir}/librecoll.so)
        message(STATUS "USING link directory ${libdir}")
        link_directories(${CMAKE_INSTALL_PREFIX}/lib ${libdir})
    endif()
endforeach()

kcoreaddons_add_plugin(krunner_recoll SOURCES recollrunner.cpp INSTALL_NAMESPACE "kf5/krunner")

set_property(TARGET krunner_recoll PROPERTY CXX_STANDARD 14)

target_link_libraries(krunner_recoll
    KF5::I18n
    KF5::ConfigCore
    KF5::Runner
    KF5::KIOWidgets
    KF5::Notifications
    recoll
    xapian
    xslt
    xml2
    X11
    dl
    z
    pthread
)

