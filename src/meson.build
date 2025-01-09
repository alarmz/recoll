# Note: the project version comes from RECOLL-VERSION.txt and is set by setversion.sh when we
# bump it. We would like to simply set it into a variable at the top to facilitate the sed job, but
# meson wants the first statement to be "project". Also there are several "  version: " in the file,
# hence the comment hack below.
#
# 
# - BSD build:
#   * Use "meson setup -Db_lundef=false" else will get undefined environ during the
#     librecoll build.
#   * recent FreeBSD with qt6:
#       export QMAKE=qmake6
#       export MAKE="gmake"
#       meson setup -Db_lundef=false -Dwebengine=true -Dchm_include_dir=/usr/local/include -Daspell_include_dir=/usr/local/include build
#     gmake is needed by the qmake-generated Makefile apparently
#
#    iconv: on FreeBSD, the default method
#    improperly uses /usr/local/iconv/iconv.h but not /usr/local/lib/libiconv, which results in an
#    undefined libiconv_open (defined by the local .h but does not exist in /usr libc).
#    We use "libiconv = cc.find_library('libiconv', required:true,dirs:'/usr/local/lib')"
#    instead, which works and results in actually using libiconv instead of the libc bundled iconv

project(
    'recoll',
    ['cpp', 'c'],
    license: 'GPL 2+',
    # We'd like to use version: files('RECOLL-VERSION.txt') but this is a 0.57 feature. 
    # Ubuntu focal has 0.53 and is supported until 2025-04.
    version: '1.42.0', # VERSIONCOMMENT keep this here, used by setversion.sh
    # Note: follow_symlinks will be needed in the future but not supported by all the versions we
    # need at the moment
    default_options: ['cpp_std=c++17', 'buildtype=debugoptimized',
                       #'follow_symlinks=true'
                     ],
    meson_version: '>=0.53',
)

# Shared lib versions.
# 1.38.0 had 1.38.0 for soversion, which was a mistake, because the SONAME was xxx.1 For now
# this is not very important because we don't really package the lib users (python, kde stuff)
# independently, but the future rule should be to only change the soversion if the ABI changed.
#
# Linked binaries only depend on soversion, e.g. ldd recollindex: librecoll.so.39 which is a
# link to, e.g., .so.<version>
recoll_soversion = '39' # SONAMECOMMENT keep this here, used by setversion.sh

cpp = meson.get_compiler('cpp')
cc = meson.get_compiler('c')

libxml = dependency('libxml-2.0')
libxslt =  dependency('libxslt')
libz = dependency('zlib')
if get_option('libmagic')
    libmagic = dependency('libmagic', required:false)
    # Libmagic has no pkgconfig data on older debian systems
    if not libmagic.found()
        libmagic = cc.find_library('libmagic', required:true)
    endif
endif

# Check if anything needs Xapian. We also need to build the shared lib if this is the case.
# The only case where it is not needed is for building rclgrep alone.
if get_option('testmains') or get_option('python-module') or get_option('indexer') or get_option('xadump') or get_option('qtgui') or get_option('recollq')
    librecoll_needed = true
    librecolldeps = []
    xapian = dependency('xapian-core')
    librecolldeps += xapian
    if get_option('idxthreads')
        librecolldeps += dependency('threads')
    endif
    librecolldeps += libxml
    librecolldeps += libxslt
    librecolldeps += libz
    if get_option('libmagic')
        librecolldeps += libmagic
    endif
    # Note: we would like to remove the x11 dep from the lib to the indexer, but this is not
    # possible without separating the indexing code out of rcldb (it calls idxstatus->x11mon).
    if get_option('x11mon') and get_option('indexer')
        librecolldeps += dependency('x11')
    endif

    # Searching for iconv is a meson 0.60 feature. Hopefully, for now all the systems where the
    # meson build is used have iconv in libc. Otherwise look at glib search for iconv for how to do
    # it.
    if (build_machine.system() == 'freebsd') or (build_machine.system() == 'openbsd')
        libiconv = cc.find_library('libiconv', required:true, dirs: '/usr/local/lib')
        librecolldeps += libiconv
    else 
        librecolldeps += dependency('iconv', method : 'auto')
    endif
else
    librecoll_needed = false
endif

recoll_datadir = get_option('prefix') / get_option('datadir') / 'recoll'

defineslist =     [
        '-DBUILDING_RECOLL',
        '-DREADFILE_ENABLE_ZLIB',
        '-DREADFILE_ENABLE_MINIZ',
        '-DREADFILE_ENABLE_MD5',
        '-DRECOLL_DATADIR="' + recoll_datadir + '"',
    ]
if get_option('appimage')
    defineslist += '-DRECOLL_APPIMAGE'
endif

add_project_arguments(
    defineslist,
    language: 'cpp',
)


recoll_conf = configuration_data()
recoll_conf.set_quoted('PACKAGE_VERSION', meson.project_version())
recoll_conf.set_quoted('FILE_PROG', get_option('file-command'))
recoll_conf.set('IDX_THREADS', get_option('idxthreads'))
recoll_conf.set('ENABLE_LIBMAGIC', get_option('libmagic'))
recoll_conf.set('DISABLE_X11MON', not get_option('x11mon'))
recoll_conf.set('RCL_USE_ASPELL', get_option('aspell'))
recoll_conf.set('PREVIEW_FORCETEXTBROWSER', not get_option('webpreview'))
# TBD: DEF_EXT4_BIRTH_TIME in python recoll setup.py
# For qtgui this was done by running qmake with DEFINE_BTIME=1 on the command line but the affected
# files actually include autoconfig.h so we're ok
recoll_conf.set('EXT4_BIRTH_TIME', get_option('ext4-birthtime'))
if cpp.compiles(
    '''
        #include <stdlib.h>
        #include <iconv.h>
        void tryit()
        {
            iconv_t ic;
            size_t isiz, osiz;
            const char *icp;
            char *ocp;
            ic = iconv_open("utf-8", "utf-8");
            iconv(ic, &icp, &isiz, &ocp, &osiz);
        }''',
    args: ['-Werror', '-O'], # -O needed, else warnings with _FORTIFY_SOURCE
    name: 'ICONV CONST',
)
    recoll_conf.set('ICONV_CONST', 'const')
elif cpp.compiles(
    '''
        #include <stdlib.h>
        #include <iconv.h>
        void tryit()
        {
            iconv_t ic;
            size_t isiz, osiz;
            char *icp;
            char *ocp;
            ic = iconv_open("utf-8", "utf-8");
            iconv(ic, &icp, &isiz, &ocp, &osiz);
        }''',
    args: ['-Werror', '-O'], # -O needed, else warnings with _FORTIFY_SOURCE
    name: 'ICONV NOT CONST',
)
    recoll_conf.set('ICONV_CONST', '')
endif

if cpp.compiles(
    '''#include<stdlib.h>
    char* func() {char t[10];return mkdtemp(t);}
    ''',
    name: 'mkdtemp',
    args: ['-Werror', '-O'], # -O needed, else warnings with _FORTIFY_SOURCE
)
    recoll_conf.set('HAVE_MKDTEMP', true)
endif
if cpp.has_header('features.h')
    recoll_conf.set('HAVE_FEATURES_H', true)
endif
if cpp.has_header('spawn.h')
    recoll_conf.set('HAVE_SPAWN_H', true)
    if cpp.has_function('posix_spawn', prefix: '#include <spawn.h>')
        recoll_conf.set10('HAVE_POSIX_SPAWN', true)
        recoll_conf.set('USE_POSIX_SPAWN', get_option('spawn'))
    endif
endif
malloc_include = ''
if cpp.has_header('malloc.h')
    malloc_include = '<malloc.h>'
    recoll_conf.set('HAVE_MALLOC_H', true)
endif
if cpp.has_header('malloc/malloc.h')
    malloc_include = '<malloc/malloc.h>'
    recoll_conf.set('HAVE_MALLOC_MALLOC_H', true)
endif
if malloc_include != ''
    if cpp.has_function('malloc_trim', prefix: '#include ' + malloc_include)
        recoll_conf.set('HAVE_MALLOC_TRIM', true)
    endif
endif
if cpp.has_function(
    'setrlimit',
    prefix: '''#include <sys/time.h>
       #include <sys/resource.h>''',
)
    recoll_conf.set('HAVE_SETRLIMIT', true)
endif
if cpp.has_function(
    'kqueue',
    prefix: '''#include <sys/event.h>
       #include <sys/time.h>''',
)
    recoll_conf.set('HAVE_KQUEUE', true)
endif

# Check for macOS (darwin)
if host_machine.system() == 'darwin' and get_option('fsevents')
    # Declare the Core Services framework dependency
    core_services = dependency('CoreServices', required: true)
    apple_core_services_found = true

    librecolldeps += core_services  # Add the CoreServices framework as a dependency
    
    recoll_conf.set('RCL_USE_FSEVENTS', true)
    recoll_conf.set('RCL_MONITOR', true)
else 
    apple_core_services_found = false
endif

recoll_conf.set('HAVE_VSNPRINTF', cpp.has_function('vsnprintf'))
recoll_conf.set(
    'HAVE_POSIX_FADVISE',
    cpp.has_function('posix_fadvise', prefix: '#include <fcntl.h>'),
)

if (
    get_option('inotify')
    and cpp.has_function(
        'inotify_add_watch',
        prefix: '#include <sys/inotify.h>',
    )
)
    recoll_conf.set('RCL_USE_INOTIFY', true)
    recoll_conf.set('RCL_MONITOR', true)
elif get_option('fam')
    libfam = dependency('libgamin', required:false)
    if libfam.found()
        recoll_conf.set('RCL_USE_FAM', true)
        recoll_conf.set('RCL_MONITOR', true)
        librecolldeps += libfam
    endif
endif
recoll_conf.set('RCL_SPLIT_CAMELCASE', get_option('camelcase'))
recoll_conf.set('USE_SIMDUTF', get_option('simdutf'))

cfile = configure_file(output: 'autoconfig.h', configuration: recoll_conf)

# Could not find a way to make this work for now.
# In case wasaparse.cpp/h need to be rebuilt just do it with:
#   ylwrap query/wasaparse.ypp
#bison = find_program('bison')
#wasaparse_cpp = custom_target('wasaparse.cpp',
#  output: ['wasaparse.cpp', 'wasaparse.h'],
#  input: 'query/wasaparse.ypp',
#  command: ['./ylwrap', '@INPUT@'],
#)

librecoll_sources = [
    'aspell/rclaspell.cpp',
    'bincimapmime/convert.cc',
    'bincimapmime/mime-parsefull.cc',
    'bincimapmime/mime-parseonlyheader.cc',
    'bincimapmime/mime-printbody.cc',
    'bincimapmime/mime.cc',
    'common/cjksplitter.cpp',
    'common/cnsplitter.cpp',
    'common/cstr.cpp',
    'common/kosplitter.cpp',
    'common/rclconfig.cpp',
    'common/rclinit.cpp',
    'common/rclutil.cpp',
    'common/syngroups.cpp',
    'common/textsplit.cpp',
    'common/unacpp.cpp',
    'common/webstore.cpp',
    'index/webqueuefetcher.cpp',
    'index/checkretryfailed.cpp',
    'index/exefetcher.cpp',
    'index/fetcher.cpp',
    'index/fsfetcher.cpp',
    'index/idxdiags.cpp',
    'index/idxstatus.cpp',
    'index/mimetype.cpp',
    'index/subtreelist.cpp',
    'internfile/extrameta.cpp',
    'internfile/htmlparse.cpp',
    'internfile/internfile.cpp',
    'internfile/mh_exec.cpp',
    'internfile/mh_execm.cpp',
    'internfile/mh_html.cpp',
    'internfile/mh_mail.cpp',
    'internfile/mh_mbox.cpp',
    'internfile/mh_text.cpp',
    'internfile/mh_xslt.cpp',
    'internfile/mimehandler.cpp',
    'internfile/myhtmlparse.cpp',
    'internfile/txtdcode.cpp',
    'internfile/uncomp.cpp',
    'query/docseq.cpp',
    'query/docseqdb.cpp',
    'query/docseqhist.cpp',
    'query/dynconf.cpp',
    'query/filtseq.cpp',
    'common/plaintorich.cpp',
    'query/qresultstore.cpp',
    'query/recollq.cpp',
    'query/reslistpager.cpp',
    'query/sortseq.cpp',
    'query/wasaparse.cpp',
    'query/wasaparseaux.cpp',
    'rcldb/daterange.cpp',
    'rcldb/expansiondbs.cpp',
    'rcldb/rclabstract.cpp',
    'rcldb/rclabsfromtext.cpp',
    'rcldb/rcldb.cpp',
    'rcldb/rclndb.cpp',
    'rcldb/rcldoc.cpp',
    'rcldb/rcldups.cpp',
    'rcldb/rclndb.cpp',
    'rcldb/rclquery.cpp',
    'rcldb/rclterms.cpp',
    'rcldb/rclvalues.cpp',
    'rcldb/searchdata.cpp',
    'rcldb/searchdatatox.cpp',
    'rcldb/searchdataxml.cpp',
    'rcldb/stemdb.cpp',
    'rcldb/stoplist.cpp',
    'rcldb/synfamily.cpp',
    'unac/unac.cpp',
    'utils/appformime.cpp',
    'utils/base64.cpp',
    'utils/cancelcheck.cpp',
    'utils/chrono.cpp',
    'utils/circache.cpp',
    'utils/closefrom.cpp',
    'utils/cmdtalk.cpp',
    'utils/conftree.cpp',
    'utils/copyfile.cpp',
    'utils/cpuconf.cpp',
    'utils/ecrontab.cpp',
    'utils/execmd.cpp',
    'utils/fileudi.cpp',
    'utils/fstreewalk.cpp',
    'utils/hldata.cpp',
    'utils/idfile.cpp',
    'utils/listmem.cpp',
    'utils/log.cpp',
    'utils/md5.cpp',
    'utils/md5ut.cpp',
    'utils/mimeparse.cpp',
    'utils/miniz.cpp',
    'utils/netcon.cpp',
    'utils/pathut.cpp',
    'utils/pxattr.cpp',
    'utils/rclionice.cpp',
    'utils/readfile.cpp',
    'utils/smallut.cpp',
    'utils/strmatcher.cpp',
    'utils/transcode.cpp',
    'utils/utf8iter.cpp',
    'utils/wipedir.cpp',
    'utils/x11mon.cpp',
    'utils/zlibut.cpp',
]

if host_machine.system() == 'darwin'
    librecoll_sources += 'internfile/finderxattr.cpp'
endif
librecoll_incdir = include_directories(
    'aspell',
    'bincimapmime',
    'common',
    'index',
    'internfile',
    'rcldb',
    'unac',
    'utils',
    'xaposix',
)

if librecoll_needed
    librecoll = library(
        'librecoll',
        librecoll_sources,
        name_prefix: '',
        soversion: recoll_soversion,
        version: meson.project_version(),
        include_directories: librecoll_incdir,
        dependencies: librecolldeps,
        install: true,
    )
    librecoll_headers = files(
    'common/plaintorich.h',
    'common/rclconfig.h',
    'common/rclinit.h',
    'common/rclutil.h',
    'index/idxstatus.h',
    'internfile/Filter.h',
    'internfile/internfile.h',
    'internfile/mimehandler.h',
    'query/docseq.h',
    'query/docseqdb.h',
    'query/qresultstore.h',
    'query/reslistpager.h',
    'query/wasatorcl.h',
    'rcldb/rcldb.h',
    'rcldb/rcldoc.h',
    'rcldb/rclquery.h',
    'rcldb/searchdata.h',
    'utils/hldata.h',
    'utils/log.h',
    'utils/pathut.h',
    'utils/readfile.h',
    'utils/smallut.h',
    )
    install_headers(librecoll_headers, subdir: 'recoll')
endif

if get_option('indexer')

    recollindex_sources = [
        'index/checkindexed.cpp',
        'index/fsindexer.cpp',
        'index/indexer.cpp',
        'index/rclmonprc.cpp',
        'index/rclmonrcv.cpp',
        'index/rclmonrcv_fsevents.cpp',
        'index/rclmonrcv_inotify.cpp',
        'index/rclmonrcv_fam.cpp',
        'index/rclmonrcv_win32.cpp',
        'index/recollindex.cpp',
        'index/webqueue.cpp',
    ]

    recollindexdeps = []
    recollindexdeps += dependency('threads')
    if(apple_core_services_found)
        recollindexdeps += core_services
    endif
    recollindex = executable(
        'recollindex',
        recollindex_sources,
        include_directories: librecoll_incdir,
        dependencies: recollindexdeps,
        link_with: librecoll,
        install: true,
    )

endif

############# Recollq
if get_option('recollq')

    recollq_sources = [
        'query/recollqmain.cpp',
    ]

    recollq = executable(
        'recollq',
        recollq_sources,
        include_directories: librecoll_incdir,
        link_with: librecoll,
        install: true,
    )

endif

############# Xadump
if get_option('xadump')

    xadump_sources = [
        'query/xadump.cpp',
    ]

    xadumpdeps = []
    xadumpdeps += declare_dependency(
        link_with: librecoll,
    )
    xadumpdeps += xapian
    xadump = executable(
        'xadump',
        xadump_sources,
        include_directories: librecoll_incdir,
        dependencies: xadumpdeps,
        install: true,
    )

endif

py_mod = import('python')
python = py_mod.find_installation(
    'python3',
)
py3_dep = dependency('python3', required: true)

if get_option('python-module')
    subdir('python/recoll')
endif   
if get_option('python-aspell')
    subdir('python/pyaspell')
endif
if get_option('python-chm')
    subdir('python/pychm')
endif
if get_option('testmains')
    subdir('testmains')
endif
if get_option('rclgrep')
    subdir('rclgrep')
endif
if get_option('qtgui')
    subdir('qtgui')
endif

manfiles = files(
'doc/man/recoll.1',
'doc/man/recoll.conf.5',
'doc/man/recollindex.1',
'doc/man/recollq.1',
'doc/man/xadump.1',
)
install_man(manfiles)

example_files = files(
'desktop/recollindex.desktop',
'index/rclmon.sh',
'index/recollindex.service',
'index/recollindex@.service',
'sampleconf/backends',
'sampleconf/fields',
'sampleconf/fragment-buttons.xml',
'sampleconf/mimeconf',
'sampleconf/mimemap',
'sampleconf/mimeview',
'sampleconf/recoll-common.css',
'sampleconf/recoll-common.qss',
'sampleconf/recoll-dark.css',
'sampleconf/recoll-dark.qss',
'sampleconf/recoll.conf',
'sampleconf/recoll.qss',
)
install_data(example_files, install_dir: get_option('datadir') / 'recoll' / 'examples')

install_subdir(
    'filters',
    install_dir: get_option('datadir') / 'recoll',
    exclude_directories: '__pycache__',
    exclude_files: ['recfiltcommon', 'injectcommon.sh',],
)

doc_files = files(
'doc/user/docbook-xsl.css',
'doc/user/usermanual.html',
)
install_data(doc_files, install_dir: get_option('datadir') / 'recoll' / 'doc')

if get_option('indexer') and get_option('systemd')
   install_data('index/recollindex@.service', install_dir: get_option('systemd-system-unit-dir'))
   install_data('index/recollindex.service', install_dir: get_option('systemd-user-unit-dir'))
endif
