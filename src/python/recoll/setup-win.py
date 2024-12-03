# Windows-specific version of the recoll Python extension package
from setuptools import setup, Extension
import os
import sys

# For shadow builds: references to the source tree
pytop = "c:/users/bill/documents/recoll/src/python/recoll"
# Windows
recolldeps = "c:/users/bill/documents/recolldeps/msvc"

# Need to set this. Where will the default config be stored. e.g. /usr/share/recoll on linux
# The compiled-in values are not used under Windows, we search from the binary location, which may not always work, in which case, you will need to set RECOLL_DATADIR in the environment.
RECOLL_DATADIR = "c:/install/recoll/Share/"

# For shadow builds: references to the source tree
top = os.path.join(pytop, '..', '..')

VERSION = open(os.path.join(top, "RECOLL-VERSION.txt")).read().strip()

# For shadow builds: reference to the top of the local tree (for finding
# generated .h files, e.g. autoconfig.h)
localtop = os.path.join(os.path.dirname(__file__), '..', '..')

extra_compile_args = ['/std:c++20']
define_macros = [
        ('RECOLL_DATADIR', RECOLL_DATADIR),
        ('__WIN32__', '1')
]

include_dirs = [
        os.path.join(top, 'common'),
        os.path.join(top, 'index'), 
        os.path.join(top, 'internfile'), 
        os.path.join(top, 'query'), 
        os.path.join(top, 'rcldb'), 
        os.path.join(top, 'utils'), 
        os.path.join(top, 'xaposix'), 
]

library_dirs = [
        os.path.join(top, "qmake/build/librecoll/Desktop_Qt_6_7_3_MSVC2019_64bit-Release/release/"),
        os.path.join(recolldeps, "libxml2/libxml2-2.9.4+dfsg1/win32/bin.msvc"),
        os.path.join(recolldeps, "libxslt/libxslt-1.1.29/win32/bin.msvc"),
        os.path.join(top, "qmake/build/libxapian/Desktop_Qt_6_7_3_MSVC2019_64bit-Release/release"),
        os.path.join(recolldeps, "zlib-1.2.11"),
        os.path.join(recolldeps, "libmagic/src/lib"),
        os.path.join(recolldeps, "regex"),
        os.path.join(recolldeps, "wlibiconv",
                     "build/Desktop_Qt_6_7_3_MSVC2019_64bit-Release/release")
        ]

libraries =  ["recoll", "libmagic", "libregex", "libxml2_a", "libxslt_a",
              "libxapian", "iconv", "zlib",
              "rpcrt4", "ws2_32", "shlwapi", "shell32",
              "psapi", "user32", "kernel32"
]

module1 = Extension('_recoll',
                    define_macros = define_macros,
                    include_dirs = include_dirs,
                    extra_compile_args = extra_compile_args,
                    libraries = libraries,
                    library_dirs = library_dirs,
                    sources = [os.path.join(pytop, 'pyrecoll.cpp'),
                               os.path.join(pytop, 'pyresultstore.cpp'),
                               os.path.join(pytop, 'pyrclextract.cpp')
                               ])

setup (name = 'Recoll',
       version = VERSION,
       description = 'Query/Augment a Recoll full text index',
       author = 'J.F. Dockes',
       author_email = 'jfd@recoll.org',
       url = 'https://www.recoll.org',
       license = 'GPL',
#       package_dir = {'' : os.path.join(top, 'python', 'recoll')},
       long_description = '''
''',
       packages = ['recoll'],
       ext_package = 'recoll',
       ext_modules = [module1])
