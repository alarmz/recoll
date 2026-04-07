#[=======================================================================[.rst:
FindSystemdPaths
----------------

Looks up various systemd install paths using PkgConfig

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``SystemdPaths_FOUND``
  True if pkg-config was able to locate the systemd configuration.
``SystemdPaths_SYSTEM_UNIT_DIR``
    Where to install system unit files (relative to install prefix).
``SystemdPaths_USER_UNIT_DIR``
    Where to install user unit files (relative to install prefix).

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables will be defined, and may be overridden
by the user during configuration:

``SYSTEMD_SYSTEM_UNIT_DIR``
  Where to install system unit files (relative to install prefix)
``SYSTEMD_USER_UNIT_DIR``
  Where to install user unit files (relative to install prefix)

#]=======================================================================]
cmake_policy(PUSH)
cmake_minimum_required(VERSION 3.22)

find_package(PkgConfig)
pkg_search_module(PC_systemd systemd)

if (PC_systemd_FOUND)
    if(CMAKE_VERSION VERSION_LESS "3.28")
        # Append arguments to allow prefix substitution
        set(PKG_CONFIG_ARGN_old "$CACHE{PKG_CONFIG_ARGN}")
        set(PKG_CONFIG_ARGN "${PKG_CONFIG_ARGN_old}")
        list(APPEND PKG_CONFIG_ARGN "--define-variable=prefix='%%PREFIX%%'")
        list(APPEND PKG_CONFIG_ARGN "--define-variable=root_prefix='%%PREFIX%%'")
        set(PKG_CONFIG_ARGN "${PKG_CONFIG_ARGN}"
            CACHE STRING "Arguments to supply to pkg-config" FORCE)
        set(_define_args)
    else()
        # Just supply prefix definition directly
        set(_define_args DEFINE_VARIABLES "prefix=%%PREFIX%%")
    endif()

    pkg_get_variable(SystemdPaths_SYSTEM_UNIT_DIR
        ${PC_systemd_MODULE_NAME} "systemd_system_unit_dir"
        ${_define_args})
    pkg_get_variable(SystemdPaths_USER_UNIT_DIR
            ${PC_systemd_MODULE_NAME} "systemd_user_unit_dir"
        ${_define_args})

    message(DEBUG "pkg-config variables retrieved:")
    message(DEBUG "  systemd_system_unit_dir=${SystemdPaths_SYSTEM_UNIT_DIR}")
    message(DEBUG "  systemd_user_unit_dir=${SystemdPaths_USER_UNIT_DIR}")
    # Make relative to CMAKE_INSTALL_PREFIX
    string(REPLACE "%%PREFIX%%/" ""
        SystemdPaths_SYSTEM_UNIT_DIR "${SystemdPaths_SYSTEM_UNIT_DIR}")
    string(REPLACE "%%PREFIX%%/" ""
        SystemdPaths_USER_UNIT_DIR "${SystemdPaths_USER_UNIT_DIR}")

    set(SYSTEMD_SYSTEM_UNIT_DIR "${SystemdPaths_SYSTEM_UNIT_DIR}" CACHE STRING
        "Where to install system unit files (relative to install prefix)")
    set(SYSTEMD_USER_UNIT_DIR "${SystemdPaths_USER_UNIT_DIR}" CACHE STRING
        "Where to install user unit files (relative to install prefix)")

    # Ensure that cache overrides take precedence
    set(SystemdPaths_SYSTEM_UNIT_DIR "$CACHE{SYSTEMD_SYSTEM_UNIT_DIR}")
    set(SystemdPaths_USER_UNIT_DIR "$CACHE{SYSTEMD_USER_UNIT_DIR}")

    if(CMAKE_VERSION VERSION_LESS "3.28")
        # Restore previous PKG_CONFIG_ARGN
        set(PKG_CONFIG_ARGN "${PKG_CONFIG_ARGN_old}"
            CACHE STRING "Arguments to supply to pkg-config" FORCE)
        unset(PKG_CONFIG_ARGN_old)
        unset(PKG_CONFIG_ARGN)
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SystemdPaths
    FOUND_VAR SystemdPaths_FOUND
    REQUIRED_VARS
        SystemdPaths_SYSTEM_UNIT_DIR
        SystemdPaths_USER_UNIT_DIR
)

cmake_policy(POP)
