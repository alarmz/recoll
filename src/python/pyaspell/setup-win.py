from setuptools import setup, Extension

# For shadow builds: references to the source tree
import os
root = "c:/users/bill/documents/"
top = os.path.join(root, "recoll", "src")
pytop = os.path.join(top, "python", "pyaspell")
recolldeps = os.path.join(root, "recolldeps")

setup (name = 'aspell-python-py3',
    version = '1.15',
    ext_modules = [Extension('recollaspell',
                             [os.path.join(pytop, "aspell.c")],
                             include_dirs = [
                                 os.path.join(recolldeps,
                                              "msvc", "aspell-0.60.7", "interfaces", "cc"), 
                             ],
                             libraries = ["aspell"],
                             library_dirs = [
                                 os.path.join(
                                     recolldeps, "msvc", "aspell-0.60.7", "build",
                                     "Desktop_Qt_6_7_3_MSVC2019_64bit-Release", "release"
                                 ),]
                             ),
                   ],
    description      = "Wrapper around GNU Aspell for Python 3",
    author           = "Wojciech Muła",
    author_email     = "wojciech_mula@poczta.onet.pl",
    maintainer       = "Wojciech Muła",
    maintainer_email = "wojciech_mula@poczta.onet.pl",
    url              = "http://github.com/WojciechMula/aspell-python",
    platforms        = ["Linux", "Windows"],
    license          = "BSD (3 clauses)",
    long_description = "aspell-python - C extension for Python 3",
    classifiers      = [
        "Development Status :: 5 - Production/Stable",
        "License :: OSI Approved :: BSD License",
        "Programming Language :: C",
        "Topic :: Software Development :: Libraries",
        "Topic :: Text Editors :: Text Processing",
    ],
)
