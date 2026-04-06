from setuptools import setup, Extension
import os
import sys

# Precooked setup.py for macos and macports, supposing that librecoll.dylib exists in
# /opt/local/lib

## CHANGE THIS
pytop = "/Users/dockes/projets/fulltext/recoll/src/python/recoll"

# The compiled-in values are not used under MacOS
RECOLL_DATADIR = "/opt/local/share/recoll"

# For shadow builds: references to the source tree
top = os.path.join(pytop, '..', '..')

VERSION = open(os.path.join(top, "RECOLL-VERSION.txt")).read().strip()

extra_compile_args = ['-std=c++17']
define_macros = [('RECOLL_DATADIR', RECOLL_DATADIR),]

include_dirs = [
        os.path.join(top, 'common'),
        os.path.join(top, 'index'), 
        os.path.join(top, 'internfile'), 
        os.path.join(top, 'query'), 
        os.path.join(top, 'rcldb'), 
        os.path.join(top, 'utils'), 
        os.path.join(top, 'xaposix'), 
]

# This supposes that you have copied librecoll to /opt/local/lib. Else, adjust.
library_dirs = ["/opt/local/lib/", ]
libraries =  ["recoll", "xml2", "xslt", "xapian", "iconv", "z", ]

module1 = Extension('_recoll',
                    define_macros = define_macros,
                    include_dirs = include_dirs,
                    extra_compile_args = extra_compile_args,
                    libraries = libraries,
                    library_dirs = library_dirs,
                    sources = [os.path.join(pytop, 'pyrecoll.cpp'),
                               os.path.join(pytop, 'pyresultstore.cpp'),
                               os.path.join(pytop, 'pyrclextract.cpp')])

setup (name = 'Recoll',
       version = VERSION,
       description = 'Query/Augment a Recoll full text index',
       author = 'J.F. Dockes',
       author_email = 'jfd@recoll.org',
       url = 'https://www.recoll.org',
       license = 'GPL',
       long_description = '''
''',
    packages = ['recoll'],
    ext_package = 'recoll',
       ext_modules = [module1])
