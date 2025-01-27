# Semi-generic version of setup.py. This is not used anymore on ux systems (which use meson instead)
# Use as a base if you need another kind of build. Supposes that librecoll and the include files are
# available somewhere
# After customizing (esp: the absolute path to the source tree), this should be usable with
# "python3 -m build" or such.

from setuptools import setup, Extension
import os

# For shadow builds: references to the source tree
# 
# This happens with e.g. "python3 -m build". The setup.py file is copied to a temporary directory
# and there seems to be no way at all to refer the original source tree.
#
# I could find no way to use ../../RECOLL-VERSION.txt
# Adding it as an include to MANIFEST.in does not seem to do anything (it appears to only work for
# files inside the python package source tree). So hard-code the source path:
recoll = "/home/dockes/projets/fulltext/recoll/src/"

VERSION = open(os.path.join(recoll, "RECOLL-VERSION.txt")).read().strip()

extra_compile_args = ['-std=c++17']

# Need to set this. Where will the default config be stored. e.g. /usr/share/recoll on linux. The
# Windows code finds it relative to the exec.
RECOLL_DATADIR = '/usr/share/recoll'
define_macros = [('RECOLL_DATADIR', '"' + RECOLL_DATADIR + '"'),]

# Set to 1 depending on what is set in the main build (this is a meson option set to off by default)
DEF_EXT4_BIRTH_TIME = 0
if DEF_EXT4_BIRTH_TIME == 1:
   define_macros.append(('EXT4_BIRTH_TIME', 1))

# include directories for building without a -dev package installed
include_dirs = [
   os.path.join(recoll, 'common'),
   os.path.join(recoll, 'index'),
   os.path.join(recoll, 'internfile'),
   os.path.join(recoll, 'query'),
   os.path.join(recoll, 'rcldb'),
   os.path.join(recoll, 'utils'),
   os.path.join(recoll, 'python', 'recoll'),
]

# Use the standard location for installed includes (Linux, mostly).
include_dirs.append('/usr/include/recoll')

# This used to add .libs to allow building with an uninstalled lib in .libs. May need to tell where
# librecoll is if not in a standard place (as it would be on Linux)
library_dirs = []

# May need to use instead a variation of the following depending on your linker:
# libraries =  ['recoll', 'xapian', 'iconv', 'z']
libraries = ['recoll']

runtime_library_dirs = list()
    
module1 = Extension('_recoll',
                    define_macros = define_macros,
                    include_dirs = include_dirs,
                    extra_compile_args = extra_compile_args,
                    libraries = libraries,
                    library_dirs = library_dirs,
                    runtime_library_dirs = runtime_library_dirs,
                    sources = ['pyrecoll.cpp',
                               'pyresultstore.cpp',
                               'pyrclextract.cpp'
                               ])

setup (name = 'Recoll',
       version = VERSION,
       packages = ['recoll'],
       ext_package = 'recoll',
       ext_modules = [module1]
       )
