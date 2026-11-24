# RtMidi requires CMake 3.24
cmake_minimum_required(VERSION 3.24)

# Silence unused variable warning when specified from toolchain
if(CMAKE_C_COMPILER)
endif()

set(LIBNAME "godot_rtmidi" CACHE STRING "The name of the library")
set(GODOT_PROJECT_DIR "demo" CACHE STRING "The directory of a Godot project folder")

# Oldest Godot version we support. Must match `compatibility_minimum` in
# demo/addons/godot_rtmidi/godot_rtmidi.gdextension. An extension built against
# a newer API will not load in older Godot versions.
if(NOT GODOTCPP_API_VERSION)
    set(GODOTCPP_API_VERSION "4.3" CACHE STRING "The Godot API version to target" FORCE)
endif()

# Build universal macOS binaries by default. This must be set before project().
# APPLE reflects the host at this point, and a toolchain file only sets
# CMAKE_SYSTEM_NAME inside project(), so skip this when cross-compiling (iOS, Android).
if(CMAKE_HOST_APPLE AND NOT CMAKE_TOOLCHAIN_FILE AND (NOT CMAKE_SYSTEM_NAME OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
    # Without this the library requires the build machine's macOS version.
    # 10.13 is Godot 4.3's minimum (arm64 slices are raised to 11.0 automatically).
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

project(godot-rtmidi
    VERSION 0.0.4
    DESCRIPTION "Realtime MIDI IO extension for Godot, built on RtMidi."
    HOMEPAGE_URL "https://github.com/NullMember/godot-rtmidi"
    LANGUAGES C CXX
)

# Make sure all the dependencies are satisfied
find_package(Python3 3.4 REQUIRED)
find_program(GIT git REQUIRED)

# Ensure the submodules have been checked out
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/extern/godot-cpp/src" OR NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/extern/rtmidi/RtMidi.cpp")
    message(NOTICE "Submodules not found, initializing/updating them...")
    execute_process(
        COMMAND ${GIT} submodule update --init --recursive extern/godot-cpp extern/rtmidi
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()

### RtMidi configuration
set(RTMIDI_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(RTMIDI_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "" FORCE)

# JACK support is opt-in, Linux only. Linking JACK makes libjack a hard runtime
# dependency: the extension fails to load on systems without it. Official
# builds are ALSA only; users who want JACK build with -DGODOT_RTMIDI_JACK=ON.
# RtMidi enables JACK by itself whenever it finds libjack, so always set it explicitly.
option(GODOT_RTMIDI_JACK "Linux only: add the JACK backend (libjack becomes required at runtime)" OFF)
if(GODOT_RTMIDI_JACK AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(RTMIDI_API_JACK ON CACHE BOOL "" FORCE)
else()
    set(RTMIDI_API_JACK OFF CACHE BOOL "" FORCE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # RtMidi only enables ALSA when it happens to find the development files.
    # Require it, so a build machine without libasound2-dev fails instead of
    # silently producing a binary with no usable MIDI backend.
    set(RTMIDI_API_ALSA ON CACHE BOOL "" FORCE)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# RtMidi reports fatal errors during construction (e.g. the MIDI driver can't
# be opened) by throwing RtMidiError. godot-cpp disables exceptions by default,
# which would make those uncatchable and terminate Godot.
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)

# Only generate the godot-cpp bindings we use (~85 files instead of ~1900).
# If the code starts using a new Godot class, add it to build_profile.json.
if(NOT GODOTCPP_BUILD_PROFILE)
    set(GODOTCPP_BUILD_PROFILE "${CMAKE_CURRENT_LIST_DIR}/build_profile.json" CACHE PATH "Path to a file containing a feature build profile" FORCE)
endif()

add_subdirectory(extern/godot-cpp SYSTEM)
add_subdirectory(extern/rtmidi)

# Add godot-cpp's module path and include the exported functions.
# This is made available for documentation generation
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${godot-cpp_SOURCE_DIR}/cmake")
include(GodotCPPModule)

# The godot-cpp target has some of useful properties attached that can be retrieved like so.
get_target_property(GODOTCPP_SUFFIX godot::cpp GODOTCPP_SUFFIX)
get_target_property(GODOTCPP_PLATFORM godot::cpp GODOTCPP_PLATFORM)

add_library(${LIBNAME} SHARED)

target_sources(${LIBNAME}
    PRIVATE
    src/register_types.cpp
    src/register_types.h
    src/godot_rtmidi_in.cpp
    src/godot_rtmidi_in.h
    src/godot_rtmidi_out.cpp
    src/godot_rtmidi_out.h
)

# Fetch a list of the xml files to use for documentation and add to our target
file(GLOB_RECURSE DOC_XML LIST_DIRECTORIES NO CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/doc_classes/*.xml")

# conditionally add doc data to compile output
if(DOC_XML)
    if(GODOTCPP_TARGET MATCHES "editor|template_debug")
        target_doc_sources(${LIBNAME} ${DOC_XML})
    endif()
endif()

target_link_libraries(${LIBNAME} PRIVATE godot-cpp rtmidi)

# Require at least C++17 for this target
set_property(TARGET ${LIBNAME} PROPERTY CXX_STANDARD 17)

set_target_properties(${LIBNAME}
    PROPERTIES
    # The generator expression here prevents msvc from adding a Debug or Release subdir.
    RUNTIME_OUTPUT_DIRECTORY "$<1:${PROJECT_SOURCE_DIR}/bin/${GODOTCPP_PLATFORM}>"
    LIBRARY_OUTPUT_DIRECTORY "$<1:${PROJECT_SOURCE_DIR}/bin/${GODOTCPP_PLATFORM}>"

    PREFIX ""
    OUTPUT_NAME "${LIBNAME}${GODOTCPP_SUFFIX}"
)

set(GODOT_PROJECT_BINARY_DIR "${PROJECT_SOURCE_DIR}/${GODOT_PROJECT_DIR}/addons/${LIBNAME}/${GODOTCPP_PLATFORM}")

message(STATUS "Godot API version: ${GODOTCPP_API_VERSION}")
message(STATUS "Copying built library to Godot project directory: ${GODOT_PROJECT_BINARY_DIR}")

add_custom_command(TARGET ${LIBNAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${LIBNAME}>" "${GODOT_PROJECT_BINARY_DIR}/$<TARGET_FILE_NAME:${LIBNAME}>"
)
