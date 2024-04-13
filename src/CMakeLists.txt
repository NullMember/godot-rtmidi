# SPDX-License-Identifier: Unlicense

target_sources( ${PROJECT_NAME}
    PRIVATE
        godot_rtmidi_in.h
        godot_rtmidi_in.cpp
        godot_rtmidi_out.h
        godot_rtmidi_out.cpp
        GDExtensionTemplate.h
        GDExtensionTemplate.cpp
        RegisterExtension.cpp
)

target_include_directories( ${PROJECT_NAME}
    PRIVATE
        "src"
)
