# Building the recollaspell Python extension. Same as pyaspell, but this is not packaged.

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
aspell_include_dir = get_option('aspell_include_dir')

aspellexts = []
foreach v : py3versions
    py3_name = 'python' + v
    python_installation = py_mod.find_installation(py3_name)
    py3_dep = python_installation.dependency()
    
    aspellexts += python_installation.extension_module(
        'recollaspell',
        [
           'aspell.c',
        ],
        dependencies: [
            py3_dep, 
            # We do not need this line below if the python module did is job.
            # When running meson, specify the python option, in particular
            # use `-Dpython.install_env=auto` if you are using a virtual enviroment or conda 
            # We can however leave the line below as it was already here. However,
            # we need to specify that required is false, since it otherwise fails
            # on systems where aspell is provided in a virtual environment, and aspell
            # was correctly detected in py3_dep by the python module. 
            cc.find_library('aspell',required: false)
        ],
        include_directories: include_directories(aspell_include_dir),
        install: true,
        subdir: '.',
    )
endforeach