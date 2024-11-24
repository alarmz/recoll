# Meson file to build the QT Recoll GUI
# We are using a custom target to run qmake and make as I don't want to have to duplicate the .pro
# into a meson build as it is used on Windows and MacOS, with special constraints.
# Meson is used to build the .pro from a .pro.in as per recoll configuration, then the custom target
# is defined. Makes use of a small script located in the qtgui directory.

qtgui_conf = configuration_data()
qtgui_conf.set('srcdir', meson.current_source_dir())
qtgui_conf.set('QMAKE_ENABLE_WEBKIT', '#')
qtgui_conf.set('QMAKE_ENABLE_WEBENGINE', '#')
if get_option('webengine')
    qtgui_conf.set('QMAKE_ENABLE_WEBENGINE', '')
else
    if get_option('webkit')
        qtgui_conf.set('QMAKE_ENABLE_WEBKIT', '')
    endif
endif
if get_option('qtzeitgeist')
    qtgui_conf.set('QMAKE_ENABLE_ZEITGEIST', '')
else
    qtgui_conf.set('QMAKE_ENABLE_ZEITGEIST', '#')
endif
if not get_option('guidebug')
    qtgui_conf.set('QMAKE_ENABLE_GUIDEBUG', '#')
else
    qtgui_conf.set('QMAKE_ENABLE_GUIDEBUG', '')
endif
qtgui_conf.set('LIBRECOLL_FOR_QMAKE', librecoll.full_path())

# Tell qmake that we are running an unix-type build, not a macx qcreator one
qtgui_conf.set('MESONRAN', 'MESONRAN')

# Tried xapian.get_pkgconfig_variable('libs')/'--libs'/'Libs' with no success
pkgc = find_program('pkg-config')
libstr = run_command(pkgc, ['--libs', 'xapian-core'], check: true).stdout().strip()
qtgui_conf.set('XAPIAN_LIBS', libstr)
qtgui_conf.set('LIBICONV', '')
libstr = run_command(pkgc, ['--libs', 'libxslt'], check: true).stdout().strip()
qtgui_conf.set('XSLT_LIBS', libstr)
qtgui_conf.set('LIBQZEITGEIST', '')
pro_file = configure_file(
    output: 'recoll.pro',
    input: 'recoll.pro.in',
    configuration: qtgui_conf,
)

qmkmk = find_program(meson.current_source_dir() / 'qmkmk.sh')

infile = pro_file
outfile = 'recoll'
prefixoption = 'PREFIX=' + get_option('prefix')
recoll = custom_target(
    'recoll',
    output: outfile,
    input: infile,
    command: [qmkmk, '@INPUT@', prefixoption],
    depends: librecoll,
    install: true,
    install_dir: 'bin',
)

install_subdir(
    'mtpics',
    install_dir: get_option('datadir') / 'recoll' / 'images',
    strip_directory: true,
    exclude_files: ['License_sidux.txt', 'README', 'aptosid-manual-copyright.txt',],
)
install_subdir(
    meson.current_build_dir() / '.qm',
    install_dir: get_option('datadir') / 'recoll' / 'translations',
    strip_directory: true,
)
install_data(
    '../desktop/recoll-searchgui.desktop',
    install_dir: get_option('datadir') / 'applications',
)
install_data(
    '../desktop/recoll.png',
    install_dir: get_option('datadir') / 'icons/hicolor/48x48/apps',
)
install_data(
    '../desktop/recoll.png',
    install_dir: get_option('datadir') / 'pixmaps',
)
install_data(
    '../desktop/recoll.svg',
    install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps',
)
install_data(
    '../desktop/org.recoll.recoll.appdata.xml',
    install_dir: get_option('datadir') / 'metainfo',
)
