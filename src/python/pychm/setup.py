from setuptools import setup, Extension

# This file is not used on Linux (the module is built with Meson). It is only used on Windows, where
# we bundle the libchm c files to simplify things

long_description = '''
Version of the chm package modified to support Python 3 and bundled with Recoll.
The chm package provides two modules, chm, and chmlib, which provide
access to the API implemented by the C library chmlib and some additional
classes and functions. They are used to access MS-ITSS encoded files -
Compressed Html Help files (.chm).
'''

setup(name="recollchm",
      version="0.8.4.1+git",
      description="Python package to handle CHM files",
      author="Rubens Ramos",
      author_email="rubensr@users.sourceforge.net",
      maintainer="Jean-Francois Dockes",
      maintainer_email="jfd@recoll.org",
      url="https://github.com/dottedmag/pychm",
      license="GPL",
      long_description=long_description,
      py_modules=["recollchm.chm", "recollchm.chmlib"],
      ext_modules=[Extension("recollchm._chmlib",
                             ["recollchm/swig_chm.c", "recollchm/chm_lib.c", "recollchm/lzx.c"],
                             extra_compile_args=["-DSWIG_COBJECT_TYPES"]),]
      )
