# qtgui related

set(qtgui_ui_SOURCES
    actsearch.ui
    advsearch.ui
    configswitch.ui
    crontool.ui
    firstidx.ui
    idxsched.ui
    preview.ui
    ptrans.ui
    rclmain.ui
    restable.ui
    rtitool.ui
    snippets.ui
    specialindex.ui
    spell.ui
    ssearchb.ui
    uiprefs.ui
    viewaction.ui
    webcache.ui
    widgets/editdialog.ui
    widgets/listdialog.ui
    winschedtool.ui
    )

set(qtgui_SOURCES
    actsearch_w.cpp
    actsearch_w.h
    advsearch_w.cpp
    advsearch_w.h
    advshist.cpp
    advshist.h
    confgui/confgui.cpp
    confgui/confgui.h
    confgui/confguiindex.cpp
    confgui/confguiindex.h
    configswitch.cpp
    configswitch.h
    crontool.cpp
    crontool.h
    firstidx.h
    fragbuts.cpp
    fragbuts.h
    guiutils.cpp
    guiutils.h
    idxmodel.cpp
    idxmodel.h
    idxsched.h
    main.cpp
    multisave.cpp
    multisave.h
    preview_load.cpp
    preview_load.h
    preview_plaintorich.cpp
    preview_plaintorich.h
    preview_w.cpp
    preview_w.h
    ptrans_w.cpp
    ptrans_w.h
    rclhelp.cpp
    rclhelp.h
    rclm_idx.cpp
    rclm_menus.cpp
    rclm_preview.cpp
    rclm_saveload.cpp
    rclm_sidefilters.cpp
    rclm_view.cpp
    rclm_wins.cpp
    rclmain_w.cpp
    rclmain_w.h
    rclwebpage.h
    rclzg.cpp
    rclzg.h
    recoll.h
    reslist.cpp
    reslist.h
    respopup.cpp
    respopup.h
    restable.cpp
    restable.h
    rtitool.cpp
    rtitool.h
    scbase.cpp
    scbase.h
    searchclause_w.cpp
    searchclause_w.h
    snippets_w.cpp
    snippets_w.h
    specialindex.h
    spell_w.cpp
    spell_w.h
    ssearch_w.cpp
    ssearch_w.h
    systray.cpp
    systray.h
    uiprefs_w.cpp
    uiprefs_w.h
    viewaction_w.cpp
    viewaction_w.h
    webcache.cpp
    webcache.h
    widgets/editdialog.h
    widgets/listdialog.h
    widgets/qxtconfirmationmessage.cpp
    widgets/qxtconfirmationmessage.h
    widgets/qxtglobal.h
    xmltosd.cpp
    xmltosd.h
    )


#--- recoll ---

find_package(Qt${QT_MAJOR_VERSION} COMPONENTS Core)
if (NOT Qt${QT_MAJOR_VERSION}_FOUND)
     message(FATAL_ERROR "Could not find Qt version ${QT_MAJOR_VERSION}!")
endif()

if (QT_MAJOR_VERSION GREATER 5)
    qt_standard_project_setup()
endif()
set(CMAKE_AUTOMOC TRUE)
set(CMAKE_AUTOUIC TRUE)
set(CMAKE_AUTORCC TRUE)

add_executable(recoll
    ${qtgui_SOURCES}
    ${qtgui_ui_SOURCES}
    recoll.qrc
)

set_target_properties(recoll PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/recoll"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER org.lesbonscomptes.recoll
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING
        "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    MACOSX_BUNDLE_ICON_FILE recoll.icns
)
target_compile_definitions(recoll PRIVATE BUILDING_RECOLLGUI)

set(QtComponents Core Gui Widgets PrintSupport)

if (RECOLL_ENABLE_WEBENGINE)
    list(APPEND QtComponents WebEngineWidgets)
    target_compile_definitions(recoll PUBLIC USING_WEBENGINE)
else()
    list(APPEND QtComponents WebKitWidgets)
    target_compile_definitions(recoll PUBLIC USING_WEBKIT)
endif ()

find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS ${QtComponents})

list(TRANSFORM QtComponents
    PREPEND "Qt${QT_MAJOR_VERSION}::"
    OUTPUT_VARIABLE _qt_targets)
target_link_libraries(recoll PUBLIC ${_qt_targets})
unset(_qt_targets)

target_include_directories(recoll PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    widgets
    ../query
    )

target_link_libraries(recoll PRIVATE librecoll)

if (QT_MAJOR_VERSION GREATER 5)
    qt_finalize_target(recoll)
endif()

### Special handling of translation files

find_package(Qt${QT_MAJOR_VERSION} COMPONENTS LinguistTools)

# include all *.ts files under locale
file(GLOB TRANS_FILES "qtgui/i18n/*.ts")

# Put generated *.qm to output dir's locale
set_source_files_properties(${TRANS_FILES} PROPERTIES
    OUTPUT_LOCATION "${PROJECT_BINARY_DIR}/i18n")

# Do *.ts -> *.qm during the target `recoll` and store result files to ${QM_FILES}
qt_add_translation(QM_FILES ${TRANS_FILES})

target_sources(recoll PRIVATE ${QM_FILES})
