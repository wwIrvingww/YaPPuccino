cmake_minimum_required(VERSION 3.19)
project(YaPPuccinoClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC on)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets WebSockets)

qt_standard_project_setup()

qt_add_executable(YaPPuccinoClient
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    Images.qrc
)

target_link_libraries(YaPPuccinoClient
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::WebSockets
)

include(GNUInstallDirs)

install(TARGETS YaPPuccinoClient
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET YaPPuccinoClient
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
