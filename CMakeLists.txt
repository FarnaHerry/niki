# CMakeLists.txt — hui: a drag-and-drop HuxerUI designer (GUI + CLI + MCP).
#
# Shape follows `huxerui create app` (the same shape apitab and llm-switch use):
# a project-introspection block the `huxerui` CLI reads in plan-only mode, a
# platform entry under platform/<platform>/main.cpp, recursive src/ discovery,
# and resources/ registered under the app namespace.
#
# The UI layer is plain C++ with the <huxerui/huxerui.h> umbrella header, exactly
# as the CMake path expects. The headless engine (JSON, document model, code
# generation, CLI, MCP) is C++23 modules and is added to the same target as a
# CXX_MODULES file set, the way apitab adds its domain layer.
#
#   cmake -S . -B build -G Ninja
#   cmake --build build
#   ./build/hui                  # GUI
#   ./build/hui selftest         # CLI
cmake_minimum_required(VERSION 3.30)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CxxImportStdGate)

# Prefer the sibling HuxerUI checkout when present; HUXERUI_HOME still wins.
if (NOT DEFINED HUXERUI_HOME
        AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/huxerui/CMakeLists.txt")
    set(HUXERUI_HOME "${CMAKE_CURRENT_SOURCE_DIR}/third_party/huxerui"
            CACHE PATH "HuxerUI source checkout or installed SDK" FORCE)
endif ()

project(hui VERSION 0.1.0 DESCRIPTION "Drag-and-drop HuxerUI designer with CLI and MCP" LANGUAGES NONE)

include("${CMAKE_CURRENT_SOURCE_DIR}/HuxerUIProject.cmake")
if (HUXERUI_PROJECT_PLAN_ONLY)
    return()
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE APP_SOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)
if (NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    if (WIN32)
        list(APPEND APP_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/main.cpp")
    elseif (APPLE AND NOT IOS)
        list(APPEND APP_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/main.cpp")
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND APP_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/main.cpp")
    endif ()
endif ()

# The headless engine's module interfaces. The UI never sees them as files: it
# imports them, and the platform entry imports hui.cli.
set(HUI_MODULE_UNITS
        src/core/json.cppm
        src/core/catalog.cppm
        src/core/doc.cppm
        src/core/docio.cppm
        src/core/validate.cppm
        src/core/codegen.cppm
        src/mcp.cppm
        src/cli.cppm
)

huxerui_add_app(hui
        SOURCES
            ${APP_SOURCE_FILES}
        RESOURCES
            resources
        RESOURCE_NAMESPACE
            app
        ${HUXERUI_APP_RESOURCE_OUTPUT_ARGUMENTS}
        BUNDLE_NAME
            "hui"
        BUNDLE_IDENTIFIER
            "dev.farna.hui"
)

huxerui_configure_project_app(hui)

# src/ is on the include path for the UI headers ("ui/ui.h") and the platform
# entry; hux.cli and the engine modules come in through the file set below.
if (NOT HUXERUI_LIBRARY_GRAPH_ONLY)
    target_include_directories(hui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

    # Engine modules (`import std;` in every one of them).
    target_sources(hui PRIVATE FILE_SET hui_modules TYPE CXX_MODULES
            BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
            FILES ${HUI_MODULE_UNITS})
    set_target_properties(hui PROPERTIES CXX_MODULE_STD ON)
    # Scan every source, not just the module units: platform/linux/main.cpp is an
    # ordinary .cpp that imports hui.cli and needs the module flags too. In the
    # source-checkout configuration that default is not applied for us.
    set_target_properties(hui PROPERTIES CXX_SCAN_FOR_MODULES ON)
    target_compile_features(hui PRIVATE cxx_std_23)

    # MSVC reads a source file in the system codepage unless told otherwise, and
    # the UI draws glyphs outside ASCII (the lightning mark on a bound event, the
    # dirty dot on a tab). windows.h's min/max macros would also break every
    # std::max in a translation unit that reaches it through a framework header.
    if (WIN32)
        target_compile_definitions(hui PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif ()
    if (MSVC)
        target_compile_options(hui PRIVATE /utf-8)
    endif ()
endif ()
