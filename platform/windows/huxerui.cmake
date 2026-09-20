# platform/windows/huxerui.cmake — what the Windows build needs from this project.
#
# The project helper adds HUXERUI_WINDOWS_MANIFEST and HUXERUI_WINDOWS_RESOURCE
# to the application target, so this file is where they have to be defined. The
# resource script carries the executable's version block; there is no icon yet,
# and a designer that draws its own chrome does not need one to build.

set(HUXERUI_WINDOWS_MANIFEST "${CMAKE_CURRENT_LIST_DIR}/app.manifest")

# VERSIONINFO wants four comma-separated numbers; project(VERSION) has three.
set(HUI_VERSION_COMMA "${PROJECT_VERSION}")
string(REPLACE "." "," HUI_VERSION_COMMA "${HUI_VERSION_COMMA}")
set(HUI_VERSION_COMMA "${HUI_VERSION_COMMA},0")

set(HUXERUI_WINDOWS_RESOURCE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/huxerui-platform/windows")
file(MAKE_DIRECTORY "${HUXERUI_WINDOWS_RESOURCE_DIRECTORY}")
configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/app.rc.in"
        "${HUXERUI_WINDOWS_RESOURCE_DIRECTORY}/app.rc"
        @ONLY
)
set(HUXERUI_WINDOWS_RESOURCE "${HUXERUI_WINDOWS_RESOURCE_DIRECTORY}/app.rc")
