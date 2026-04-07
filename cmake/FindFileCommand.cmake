#[=======================================================================[.rst:
FindFileCommand
---------------

Finds the ``file`` command on the local system

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``FileCommand::FILE_PROG``
  An interface target containing a symbol definition for FILE_PROG,
  set to the path of the ``file`` command which should be used.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``FileCommand_FOUND``
  True if the system has a ``file`` command.
``FileCommand_DEFINITIONS``
  A compile definition for ``FILE_PROG=${FileCommand_EXECUTABLE}``

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``FileCommand_EXECUTABLE``
  The path to the ``file`` command.

#]=======================================================================]

find_program(FileCommand_EXECUTABLE
    NAMES file
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FileCommand
    FOUND_VAR FileCommand_FOUND
    REQUIRED_VARS FileCommand_EXECUTABLE
)
set(FileCommand_DEFINITIONS "FILE_PROG=\"${FileCommand_EXECUTABLE}\"")

if (FileCommand_FOUND)
    if (NOT TARGET FileCommand::FILE_PROG)
        add_library(FileCommand::FILE_PROG IMPORTED INTERFACE)
    endif()
    set_target_properties(FileCommand::FILE_PROG PROPERTIES
	EXECUTABLE_LOCATION "${FileCommand_EXECUTABLE}"
	INTERFACE_COMPILE_DEFINITIONS "${FileCommand_DEFINITIONS}"
    )
endif()

