# Error if building out of a build directory
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if (EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with "
            "CMakeLists.txt file). Please make a build subdirectory. Feel free to "
            "remove CMakeCache.txt and CMakeFiles.")
endif ()

if (WIN32)
    set(PATH_DELIMITER ";")
else ()
    set(PATH_DELIMITER ":")
endif ()

# Prepare Qt search paths. Prefer an installed Qt before falling back to a local source build.
set(QT_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/qt-build")
set(QT_INCLUDE_DIR "${QT_BUILD_DIR}/include")
set(QT_BIN_DIR "${QT_BUILD_DIR}/bin")
set(QT_CMAKE_DIR "${QT_BUILD_DIR}/lib/cmake/Qt6")

if (EXISTS "${QT_CMAKE_DIR}/Qt6Config.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${QT_BUILD_DIR}")
    set(ENV{QTDIR} "${QT_BUILD_DIR}")
    set(ENV{PATH} "${QT_BIN_DIR}${PATH_DELIMITER}$ENV{PATH}")
endif ()

if (APPLE)
    set(QT_ADDITIONAL_PACKAGES_PREFIX_PATH "")
    foreach (qt_prefix
            "/opt/homebrew/opt/qtbase"
            "/opt/homebrew/opt/qtwebsockets"
            "/opt/homebrew/opt/qtdeclarative"
            "/opt/homebrew/opt/qtsvg"
            "/usr/local/opt/qtbase"
            "/usr/local/opt/qtwebsockets"
            "/usr/local/opt/qtdeclarative"
            "/usr/local/opt/qtsvg")
        if (EXISTS "${qt_prefix}")
            list(PREPEND CMAKE_PREFIX_PATH "${qt_prefix}")
            list(APPEND QT_ADDITIONAL_PACKAGES_PREFIX_PATH "${qt_prefix}")
        endif ()
    endforeach ()
endif ()

# Fetch the package
find_package(Qt6 COMPONENTS Core Gui Widgets Network WebSockets Test QUIET)

if (NOT Qt6_FOUND)
    # Set Ninja for compilation
    set(CMAKE_GENERATOR "Ninja" CACHE STRING "CMake generator" FORCE)
    # Initial message
    message(STATUS "Qt 6 not found. Downloading and configuring Qt...")

    # Download and extract archive of Qt 6.8.2
    set(QT_VERSION "6.8.2")
    set(QT_ARCHIVE_URL "https://github.com/qt/qtbase/archive/refs/tags/v${QT_VERSION}.tar.gz")
    set(QT_ARCHIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/qtbase-${QT_VERSION}.tar.gz")
    set(QT_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/qtbase-${QT_VERSION}")
    message(STATUS "Downloading the package from the github mirror...")
    file(DOWNLOAD ${QT_ARCHIVE_URL} ${QT_ARCHIVE_FILE}
            SHOW_PROGRESS)
    message(STATUS "Unpacking the release...")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xvf ${QT_ARCHIVE_FILE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            OUTPUT_QUIET
    )

    # Configure Qt (skip building of useless modules)
    set(QT_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/qt-build")
    file(MAKE_DIRECTORY ${QT_BUILD_DIR})
    set(Qt6_DIR "${QT_CMAKE_DIR}")
    set(ENV{QTDIR} "${QT_BUILD_DIR}")
    set(ENV{PATH} "${QT_BIN_DIR}${PATH_DELIMITER}$ENV{PATH}")
    message(STATUS "Configuring...")
    set(QT_CONFIGURE_OPTIONS -static -c++std c++20 -static-runtime -release -nomake examples -nomake tests -prefix ${QT_BUILD_DIR})

    if (WIN32)
        execute_process(COMMAND "${QT_SOURCE_DIR}/configure.bat" ${QT_CONFIGURE_OPTIONS}
                WORKING_DIRECTORY ${QT_BUILD_DIR})
    else ()
        execute_process(COMMAND "${QT_SOURCE_DIR}/configure" ${QT_CONFIGURE_OPTIONS}
                WORKING_DIRECTORY ${QT_BUILD_DIR})
    endif ()

    # Build Qt
    execute_process(COMMAND ${CMAKE_COMMAND} --build . --parallel
            WORKING_DIRECTORY ${QT_BUILD_DIR})

    find_package(Qt6 COMPONENTS Core Gui Widgets Network WebSockets Test REQUIRED)
else ()
    message(STATUS "Qt 6 found in the system.")
endif ()

qt_standard_project_setup()

add_library(qt INTERFACE)

target_include_directories(qt INTERFACE "${QtCMake_SOURCE_DIR}/lib/qt")
target_include_directories(qt INTERFACE "${qt_cmake_SOURCE_DIR}/lib/qt")
target_include_directories(qt INTERFACE "${qtcmake_SOURCE_DIR}/lib/qt")
if (EXISTS "${QT_INCLUDE_DIR}")
    target_include_directories(qt INTERFACE "${QT_INCLUDE_DIR}")
endif ()

target_link_libraries(qt INTERFACE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::WebSockets Qt6::Test)
