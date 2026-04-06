# Building the Recoll Python extension.

pyrecoll_incdir = include_directories(
    '../../common',
    '../../index',
    '../../internfile',
    '../../query',
    '../../rcldb',
    '../../utils',
)

py3versions_cmd = find_program('py3versions',
    native: true,
    required: false,
)
if py3versions_cmd.found()
    py3vs = run_command(py3versions_cmd, ['-iv',],
        check: true,
        capture: true,
        )
    py3versions = py3vs.stdout().split()
else
    py3versions = ['3',]
endif

py_mod = import('python')
extensions = []
foreach v : py3versions
    py3_name = 'python' + v
    python_installation = py_mod.find_installation(py3_name)
    extensions += python_installation.extension_module(
        '_recoll',
        [
            'pyrclextract.cpp',
            'pyrecoll.cpp',
            'pyresultstore.cpp',
        ],
        include_directories: pyrecoll_incdir,
        dependencies: python_installation.dependency(),
        link_with: librecoll,
        install: true,
        subdir: 'recoll',
    )
endforeach


python.install_sources(
    [
        'recoll/__init__.py',
        'recoll/conftree.py',
        'recoll/fsudi.py',
        'recoll/qresultstore.py',
        'recoll/rclconfig.py',
        'recoll/rclextract.py',
        'recoll/recoll.py',
    ],
    pure: false,
    subdir: 'recoll',
)

