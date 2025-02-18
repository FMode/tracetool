SET(GUI_SOURCES
  main.cpp
  settings.cpp
  mainwindow.cpp
  fixedheaderview.cpp
  entryfilter.cpp
  columnsinfo.cpp
  filterform.cpp
  settingsform.cpp
  storageview.cpp
  configuration.cpp
  configeditor.cpp
  entryitemmodel.cpp
  watchtree.cpp
  applicationtable.cpp
  searchwidget.cpp
  ../server/database.cpp)

SET(GUI_TS
    ${CMAKE_CURRENT_BINARY_DIR}/gui.ts)

IF (WIN32)
    LIST(APPEND GUI_SOURCES jobobject.cpp)
ENDIF()

IF (FALSE)
    LIST(APPEND GUI_SOURCES modeltest.cpp)
    ADD_DEFINITIONS(-DHAVE_MODELTEST)
ENDIF()

# Make sure QtGui is available by finding Qt again
FIND_PACKAGE(Qt6 COMPONENTS Core Network Xml Sql Gui Widgets Core5Compat REQUIRED)

QT6_CREATE_TRANSLATION( GUI_QM ${GUI_SOURCES} ${GUI_TS} )

# Locate generated ui_*.f files in build directory

ADD_EXECUTABLE(tracegui WIN32 MACOSX_BUNDLE ${GUI_SOURCES} ${GUI_QM})

SET_TARGET_PROPERTIES(tracegui PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
)

TARGET_LINK_LIBRARIES(tracegui Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Network Qt6::Core5Compat)

# Installation
INSTALL(TARGETS tracegui RUNTIME DESTINATION bin COMPONENT applications
                         LIBRARY DESTINATION lib COMPONENT applications
                         BUNDLE  DESTINATION bin COMPONENT applications
                         ARCHIVE DESTINATION lib COMPONENT applications)
# Make tracegui (and other executables in same directory) self-contained wrt. Qt
IF(WIN32)
    GET_TARGET_PROPERTY(_qmake_path Qt6::qmake IMPORTED_LOCATION)
    GET_FILENAME_COMPONENT(_qt_bindir ${_qmake_path} DIRECTORY)
    INSTALL(
        CODE "EXECUTE_PROCESS(COMMAND \"${_qt_bindir}/windeployqt.exe\" \"\${CMAKE_INSTALL_PREFIX}/bin/tracegui.exe\")"
        COMPONENT applications)
ELSEIF(APPLE AND BUNDLE_QT)
    GET_TARGET_PROPERTY(_qmake_path Qt5::qmake IMPORTED_LOCATION)
    GET_FILENAME_COMPONENT(_qt_bindir ${_qmake_path} DIRECTORY)
    INSTALL(
        CODE "EXECUTE_PROCESS(COMMAND \"${_qt_bindir}/macdeployqt\" \"\${CMAKE_INSTALL_PREFIX}/bin/tracegui.app\")"
        COMPONENT applications)
ELSEIF(NOT WIN32 AND NOT APPLE AND BUNDLE_QT)
    FUNCTION(install_qt6_plugin pluginTargetName)
        STRING(TOUPPER ${CMAKE_BUILD_TYPE} _buildType_UC)
        GET_TARGET_PROPERTY(_qtplugin_path ${pluginTargetName} IMPORTED_LOCATION_${_buildType_UC})
        GET_FILENAME_COMPONENT(_qtplugin_name "${_qtplugin_path}" NAME)
        GET_FILENAME_COMPONENT(_qtplugin_dir "${_qtplugin_path}" DIRECTORY)
        GET_FILENAME_COMPONENT(_qtplugin_type "${_qtplugin_dir}" NAME)
        SET(${installPathVar} "\${CMAKE_INSTALL_PREFIX}/plugins/${_qtplugin_type}/${_qtplugin_name}")
        INSTALL(FILES "${_qtplugin_path}" DESTINATION "plugins/${_qtplugin_type}" COMPONENT applications)
    ENDFUNCTION()
    FUNCTION(install_library_from_qt_libdir libname)
        STRING(TOUPPER ${CMAKE_BUILD_TYPE} _buildType_UC)
        GET_TARGET_PROPERTY(_qtlib_dir Qt6::Core IMPORTED_LOCATION_${_buildType_UC})
        GET_FILENAME_COMPONENT(_qtlib_dir ${_qtlib_dir} PATH)
        FILE(GLOB _libfiles "${_qtlib_dir}/${libname}*")
        message(STATUS "${_libfiles}")
        INSTALL(FILES ${_libfiles} DESTINATION lib COMPONENT applications)
    ENDFUNCTION()
    INSTALL_LIBRARY_FROM_QT_LIBDIR("libQt6Core.so.5")
    INSTALL_LIBRARY_FROM_QT_LIBDIR("libQt6DBus.so.5")
    INSTALL_LIBRARY_FROM_QT_LIBDIR("libQt6Gui.so.5")
    INSTALL_LIBRARY_FROM_QT_LIBDIR("libQt6Network.so.5")
    INSTALL_LIBRARY_FROM_QT_LIBDIR("libQt6Sql.so.5")
    INSTALL_LIBRARY_FROM_QT_LIBDIR("libQt6Widgets.so.5")
    INSTALL_LIBRARY_FROM_QT_LIBDIR("libQt6XcbQpa.so.5")
    INSTALL_LIBRARY_FROM_QT_LIBDIR("libicudata.so.56")
    INSTALL_LIBRARY_FROM_QT_LIBDIR("libicuuc.so.56")
    INSTALL_LIBRARY_FROM_QT_LIBDIR("libicui18n.so.56")
    INSTALL_QT6_PLUGIN(Qt6::QXcbIntegrationPlugin)
    INSTALL_QT6_PLUGIN(Qt6::QSQLiteDriverPlugin)
    INSTALL(
        CODE "FILE(WRITE \"\${CMAKE_INSTALL_PREFIX}/bin/qt.conf\" \"[Paths]\nPrefix=..\")"
        COMPONENT applications )
ENDIF()
