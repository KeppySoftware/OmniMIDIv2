configure_file(version.h.in version.h @ONLY)

qt_standard_project_setup()

qt_add_executable(OmniMIDIConfigurator
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    bassmidisettings.h bassmidisettings.cpp bassmidisettings.ui
    utils.h utils.cpp
    omconfig.h omconfig.cpp
    bassconfig.h bassconfig.cpp
    synthsettings.h
    xsynthsettings.h xsynthsettings.cpp xsynthsettings.ui
    xsynthconfig.h xsynthconfig.cpp
    fluidconfig.h fluidconfig.cpp
    fluidsettings.h fluidsettings.cpp fluidsettings.ui
    soundfontlist.h soundfontlist.cpp
    sflistconfig.h sflistconfig.cpp
    sfsettings.h sfsettings.cpp sfsettings.ui
    aboutwindow.h aboutwindow.cpp aboutwindow.ui
    kdmapi.h
    icons.qrc
)

target_link_libraries(OmniMIDIConfigurator
    PRIVATE
        Qt::Core
        Qt::Widgets
)
target_link_libraries(OmniMIDIConfigurator PRIVATE nlohmann_json::nlohmann_json)

include(GNUInstallDirs)
install(TARGETS OmniMIDIConfigurator
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_finalize_executable(OmniMIDIConfigurator)
