cmake_minimum_required(VERSION 3.24)

project(IssueTrace VERSION 1.0.0 LANGUAGES C CXX)

# Placeholder identity for development archives. A stable release must override
# this with an identifier controlled by the publisher.
set(ISSUETRACE_BUNDLE_IDENTIFIER "io.issuetrace.desktop.dev" CACHE STRING
    "Reverse-DNS application identifier used by desktop bundles")

include(GNUInstallDirs)
include(FetchContent)
find_package(Threads REQUIRED)

find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    FetchContent_Declare(sqlite_amalgamation
        URL https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
        URL_HASH SHA3_256=628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(sqlite_amalgamation)
    add_library(issuetrace_sqlite3 STATIC
        "${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c"
    )
    target_include_directories(issuetrace_sqlite3 PUBLIC
        "${sqlite_amalgamation_SOURCE_DIR}"
    )
    target_compile_definitions(issuetrace_sqlite3 PRIVATE SQLITE_ENABLE_FTS5)
    add_library(SQLite3::SQLite3 ALIAS issuetrace_sqlite3)
endif()

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(USE_NO_MD5 ON CACHE BOOL "" FORCE)

if(NOT ISSUETRACE_BUNDLED_ZLIB)
    find_package(ZLIB QUIET)
endif()
if(ISSUETRACE_BUNDLED_ZLIB OR NOT ZLIB_FOUND)
    set(ISSUETRACE_BUNDLED_ZLIB ON CACHE INTERNAL "Build the portable zlib fallback")
    set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG v1.3.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(zlib)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
    set(ZLIB_LIBRARY zlibstatic)
    set(ZLIB_LIBRARIES zlibstatic)
    set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR}")
    set(ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}")
endif()

FetchContent_Declare(libxlsxwriter
    GIT_REPOSITORY https://github.com/jmcnamara/libxlsxwriter.git
    GIT_TAG v1.2.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(libxlsxwriter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(issuetrace_core
    src/core/summary_renderer.cpp
    src/core/xlsx_exporter.cpp
    src/storage/sqlite/issue_store.cpp
)
target_include_directories(issuetrace_core PUBLIC include)
target_link_libraries(issuetrace_core PUBLIC SQLite3::SQLite3 xlsxwriter)
target_compile_options(issuetrace_core PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)

enable_testing()
function(issuetrace_enable_test_assertions target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-UNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    )
endfunction()
add_executable(issuetrace_core_tests tests/core_tests.cpp)
target_link_libraries(issuetrace_core_tests PRIVATE issuetrace_core)
issuetrace_enable_test_assertions(issuetrace_core_tests)
add_test(NAME issuetrace_core_tests COMMAND issuetrace_core_tests)

add_library(issuetrace_update_package src/updater/update_package.cpp)
target_include_directories(issuetrace_update_package PUBLIC include)
target_compile_options(issuetrace_update_package PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
)
add_executable(IssueTraceUpdater src/updater/main.cpp)
target_link_libraries(IssueTraceUpdater PRIVATE issuetrace_update_package Threads::Threads)
if(WIN32)
    target_link_libraries(IssueTraceUpdater PRIVATE shell32)
endif()
if(MINGW)
    # The helper is copied outside the active portable directory before the
    # application exits, so it must not depend on DLLs that it is about to replace.
    target_link_options(IssueTraceUpdater PRIVATE -static -static-libgcc -static-libstdc++)
elseif(UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(IssueTraceUpdater PRIVATE -static-libgcc -static-libstdc++)
endif()
if(WIN32)
    set(issuetrace_update_platform windows)
elseif(APPLE)
    set(issuetrace_update_platform macos)
else()
    set(issuetrace_update_platform linux)
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" issuetrace_processor)
if(issuetrace_processor MATCHES "^(x86_64|amd64)$")
    set(issuetrace_update_architecture x86_64)
elseif(issuetrace_processor MATCHES "^(arm64|aarch64)$")
    set(issuetrace_update_architecture arm64)
else()
    set(issuetrace_update_architecture "${issuetrace_processor}")
endif()
target_compile_definitions(IssueTraceUpdater PRIVATE
    ISSUETRACE_UPDATE_PLATFORM="${issuetrace_update_platform}"
    ISSUETRACE_UPDATE_ARCHITECTURE="${issuetrace_update_architecture}"
)
add_executable(issuetrace_update_package_tests tests/update_package_tests.cpp)
target_link_libraries(issuetrace_update_package_tests PRIVATE issuetrace_update_package)
issuetrace_enable_test_assertions(issuetrace_update_package_tests)
add_test(NAME issuetrace_update_package_tests COMMAND issuetrace_update_package_tests)
if(UNIX)
    add_test(NAME issuetrace_updater_integration
        COMMAND "${CMAKE_COMMAND}"
            -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/updater-integration
            -DUPDATER=$<TARGET_FILE:IssueTraceUpdater>
            -DMANIFEST_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/packaging/generate-release-manifest.cmake
            -DPLATFORM=${issuetrace_update_platform}
            -DARCHITECTURE=${issuetrace_update_architecture}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/updater_integration.cmake
    )
endif()

option(ISSUETRACE_BUILD_DESKTOP "Build the Qt desktop application" OFF)
if(ISSUETRACE_BUILD_DESKTOP)
    find_package(Qt6 6.11 REQUIRED COMPONENTS
        Core
        Gui
        Qml
        Quick
        QuickControls2
        Widgets
    )
    qt_standard_project_setup()
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)

    qt_add_executable(IssueTrace
        src/app/app_controller.cpp
        src/app/app_controller.hpp
        src/app/form_template.cpp
        src/app/form_template.hpp
        src/app/issue_export_service.cpp
        src/app/issue_export_service.hpp
        src/app/main.cpp
    )
    if(APPLE)
        set(issuetrace_macos_icon
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/IssueTrace.icns")
        set_source_files_properties(${issuetrace_macos_icon} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
        )
        target_sources(IssueTrace PRIVATE ${issuetrace_macos_icon})
    elseif(WIN32)
        set(ISSUETRACE_WINDOWS_ICON
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/IssueTrace.ico")
        configure_file(resources/icons/IssueTrace.rc.in
            "${CMAKE_CURRENT_BINARY_DIR}/IssueTrace.rc" @ONLY)
        target_sources(IssueTrace PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/IssueTrace.rc"
            resources/icons/IssueTrace.ico
        )
    endif()
    target_compile_definitions(IssueTrace PRIVATE
        ISSUETRACE_APP_VERSION="${PROJECT_VERSION}"
    )
    if(MINGW)
        target_sources(IssueTrace PRIVATE src/app/windows_mingw_compat.cpp)
        target_link_libraries(IssueTrace PRIVATE ssp)
    endif()
    set_target_properties(IssueTrace PROPERTIES
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "IssueTrace"
        MACOSX_BUNDLE_ICON_FILE "IssueTrace.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${ISSUETRACE_BUNDLE_IDENTIFIER}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
    qt_add_qml_module(IssueTrace
        URI IssueTrace
        VERSION 1.0
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/IssueTrace"
        QML_FILES
            qml/Main.qml
            qml/QuietButton.qml
            qml/EventMetadataFields.qml
            qml/EventFormFields.qml
            qml/IssueCalendar.qml
            qml/IssueDetailsDialog.qml
            qml/IssueInbox.qml
            qml/IssueEditorList.qml
            qml/TimelineComposer.qml
            qml/TimelineList.qml
    )
    qt_add_resources(IssueTrace issuetrace_resources
        PREFIX "/issuetrace"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}"
        FILES
            resources/icons/issuetrace-256.png
            resources/templates/forms/default-issue.json
            resources/templates/summaries/default-summary.md
    )
    target_link_libraries(IssueTrace PRIVATE
        issuetrace_core
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
    )
    add_executable(issuetrace_form_template_tests
        tests/form_template_tests.cpp
        src/app/form_template.cpp
    )
    target_include_directories(issuetrace_form_template_tests PRIVATE src/app)
    target_compile_definitions(issuetrace_form_template_tests PRIVATE
        ISSUETRACE_DEFAULT_FORM_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/templates/forms/default-issue.json"
    )
    target_link_libraries(issuetrace_form_template_tests PRIVATE Qt6::Core)
    issuetrace_enable_test_assertions(issuetrace_form_template_tests)
    add_test(NAME issuetrace_form_template_tests COMMAND issuetrace_form_template_tests)
    add_executable(issuetrace_issue_export_service_tests
        tests/issue_export_service_tests.cpp
        src/app/form_template.cpp
        src/app/issue_export_service.cpp
    )
    target_include_directories(issuetrace_issue_export_service_tests PRIVATE src/app)
    target_compile_definitions(issuetrace_issue_export_service_tests PRIVATE
        ISSUETRACE_DEFAULT_FORM_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/templates/forms/default-issue.json"
        ISSUETRACE_DEFAULT_SUMMARY_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/templates/summaries/default-summary.md"
    )
    target_link_libraries(issuetrace_issue_export_service_tests PRIVATE
        issuetrace_core
        Qt6::Core
    )
    issuetrace_enable_test_assertions(issuetrace_issue_export_service_tests)
    add_test(NAME issuetrace_issue_export_service_tests
        COMMAND issuetrace_issue_export_service_tests)
    if(NOT CMAKE_CROSSCOMPILING)
        add_test(NAME issuetrace_ui_scroll_smoke
            COMMAND "${CMAKE_COMMAND}" -E env
                QT_QPA_PLATFORM=offscreen
                ISSUETRACE_WORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/ui-scroll-workspace
                $<TARGET_FILE:IssueTrace> --verify-scroll-layout
        )
        set_tests_properties(issuetrace_ui_scroll_smoke PROPERTIES TIMEOUT 15)
        add_test(NAME issuetrace_ui_status_refresh_smoke
            COMMAND "${CMAKE_COMMAND}" -E env
                QT_QPA_PLATFORM=offscreen
                ISSUETRACE_WORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/ui-status-workspace
                $<TARGET_FILE:IssueTrace> --verify-status-refresh
        )
        set_tests_properties(issuetrace_ui_status_refresh_smoke PROPERTIES TIMEOUT 15)
        add_test(NAME issuetrace_custom_reminder_smoke
            COMMAND "${CMAKE_COMMAND}" -E env
                QT_QPA_PLATFORM=offscreen
                ISSUETRACE_WORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/custom-reminder-workspace
                $<TARGET_FILE:IssueTrace> --verify-custom-reminder
        )
        set_tests_properties(issuetrace_custom_reminder_smoke PROPERTIES TIMEOUT 15)
        add_test(NAME issuetrace_description_image_smoke
            COMMAND "${CMAKE_COMMAND}" -E env
                QT_QPA_PLATFORM=offscreen
                ISSUETRACE_WORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/description-image-workspace
                $<TARGET_FILE:IssueTrace> --verify-description-image
        )
        set_tests_properties(issuetrace_description_image_smoke PROPERTIES TIMEOUT 15)
        add_test(NAME issuetrace_tracked_duration_edit_smoke
            COMMAND "${CMAKE_COMMAND}" -E env
                QT_QPA_PLATFORM=offscreen
                ISSUETRACE_WORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/tracked-duration-workspace
                $<TARGET_FILE:IssueTrace> --verify-tracked-duration-edit
        )
        set_tests_properties(issuetrace_tracked_duration_edit_smoke PROPERTIES TIMEOUT 15)
        add_test(NAME issuetrace_explicit_metadata_save_smoke
            COMMAND "${CMAKE_COMMAND}" -E env
                QT_QPA_PLATFORM=offscreen
                ISSUETRACE_WORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/metadata-save-workspace
                $<TARGET_FILE:IssueTrace> --verify-explicit-metadata-save
        )
        set_tests_properties(issuetrace_explicit_metadata_save_smoke PROPERTIES TIMEOUT 15)
        add_test(NAME issuetrace_two_pane_navigation_smoke
            COMMAND "${CMAKE_COMMAND}" -E env
                QT_QPA_PLATFORM=offscreen
                ISSUETRACE_WORKSPACE=${CMAKE_CURRENT_BINARY_DIR}/two-pane-navigation-workspace
                $<TARGET_FILE:IssueTrace> --verify-two-pane-navigation
        )
        set_tests_properties(issuetrace_two_pane_navigation_smoke PROPERTIES TIMEOUT 15)
    endif()
    if(WIN32)
        set(issuetrace_runtime_destination ".")
    else()
        set(issuetrace_runtime_destination "${CMAKE_INSTALL_BINDIR}")
    endif()
    install(TARGETS IssueTrace
        BUNDLE DESTINATION .
        RUNTIME DESTINATION "${issuetrace_runtime_destination}"
    )
    install(TARGETS IssueTraceUpdater
        RUNTIME DESTINATION "${issuetrace_runtime_destination}"
    )
    install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION .)
    # Qt's deployment tool must run natively for the target platform. Native
    # builds use the official deployment API; cross builds are staged by the
    # platform packaging script using PE dependency inspection.
    if(NOT CMAKE_CROSSCOMPILING)
        qt_generate_deploy_qml_app_script(
            TARGET IssueTrace
            OUTPUT_SCRIPT issuetrace_deploy_script
            NO_TRANSLATIONS
            NO_UNSUPPORTED_PLATFORM_ERROR
        )
        install(SCRIPT "${issuetrace_deploy_script}")
    endif()
endif()
