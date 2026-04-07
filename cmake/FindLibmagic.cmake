#[=======================================================================[.rst:
FindLibmagic
------------

Finds the magic file-identification library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``Libmagic::Libmagic``
  The magic library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Libmagic_FOUND``
  True if the system has the magic library.
``Libmagic_INCLUDE_DIRS``
  Include directories needed to use libmagic.
``Libmagic_LIBRARIES``
  Libraries needed to link to libmagic.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Libmagic_INCLUDE_DIR``
  The directory containing ``magic.h``.
``Libmagic_LIBRARY``
  The path to the magic library.

#]=======================================================================]

find_package(PkgConfig)
pkg_check_modules(PC_libmagic QUIET libmagic)

find_library(Libmagic_LIBRARY
    NAMES magic
    PATHS ${PC_libmagic_LIBRARY_DIRS}
)
find_path(Libmagic_INCLUDE_DIR
    NAMES magic.h
    PATHS ${PC_libmagic_INCLUDE_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libmagic
    FOUND_VAR Libmagic_FOUND
    REQUIRED_VARS Libmagic_LIBRARY Libmagic_INCLUDE_DIR
)
set(Libmagic_LIBRARIES "${Libmagic_LIBRARY}")
set(Libmagic_INCLUDE_DIRS "${Libmagic_INCLUDE_DIR}")

### TODO: This doesn't handle creating proper Windows shared
### library targets (with IMPORTED_IMPLIB and IMPORTED_LOCATION)
### but it probably should
if (Libmagic_FOUND)
    if (NOT TARGET Libmagic::Libmagic)
        add_library(Libmagic::Libmagic UNKNOWN IMPORTED)
    endif()
    set_target_properties(Libmagic::Libmagic PROPERTIES
        IMPORTED_LOCATION "${Libmagic_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Libmagic_INCLUDE_DIR}"
        INTERFACE_COMPILE_DEFINITIONS "ENABLE_LIBMAGIC=1"
    )
endif()

