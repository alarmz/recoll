# Building the recollchm Python extension.

py_mod = import('python')

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

# Get the custom include directories from the command line option
chm_include_dir = get_option('chm_include_dir')

chmexts = []
foreach v : py3versions
    py3_name = 'python' + v
    python_installation = py_mod.find_installation(py3_name)
    py3_dep = python_installation.dependency()

    chmexts += python_installation.extension_module(
        '_chmlib',
        [
            'recollchm/swig_chm.c',
        ],
        c_args: ['-DSWIG_COBJECT_TYPES'],
        dependencies: [
            py3_dep, 
            # We do not need this line below if the python module did is job.
            # When running meson, specify the python option, in particular
            # use `-Dpython.install_env=auto` if you are using a virtual enviroment or conda 
            # We can however leave the line below as it was already here. However,
            # we need to specify that required is false, since it otherwise fails
            # on systems where chm is provided in a virtual environment, and chm
            # was correctly detected in py3_dep by the python module. 
            cc.find_library('chm',required: false)
        ],
        include_directories: include_directories(chm_include_dir),
        install: true,
        subdir: 'recollchm',
    )

endforeach

python.install_sources(
    [
        'recollchm/__init__.py',
        'recollchm/chm.py',
        'recollchm/chmlib.py',
    ],
    subdir: 'recollchm',
    pure: false,
)
