#ifndef GODOT_RTMIDI_ANDROID_MIDI_H
#define GODOT_RTMIDI_ANDROID_MIDI_H

#include <godot_cpp/classes/global_constants.hpp>

namespace godot_rtmidi {

// Android only: prepares RtMidi's Java listener class
// (com.yellowlab.rtmidi.MidiDeviceOpenedListener, shipped in the addon's .aar)
// so that ports can be opened. Call before RtMidi's openPort().
//
// Godot loads this library with dlopen() instead of System.loadLibrary(), so
// the Java runtime can't find the class's native method by name; it is
// registered explicitly with RegisterNatives() instead. Runs once.
//
// Returns OK, or ERR_UNCONFIGURED (with an error printed) when the class isn't
// in the APK, i.e. the export doesn't use the godot_rtmidi editor plugin with a
// Gradle build.
godot::Error prepare_android_open_port(const char *class_name);

} // namespace godot_rtmidi

#endif // GODOT_RTMIDI_ANDROID_MIDI_H
