#ifndef GODOT_RTMIDI_BLUETOOTH_MIDI_H
#define GODOT_RTMIDI_BLUETOOTH_MIDI_H

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/core/error_macros.hpp>

namespace godot_rtmidi {

// Shows the system's Bluetooth MIDI device list so the user can connect a
// device. Connected devices then show up as regular input and output ports.
// iOS (bluetooth_midi_ios.mm) and Android (android_midi.cpp); returns
// ERR_UNAVAILABLE elsewhere (bluetooth_midi.cpp).
godot::Error show_bluetooth_midi_devices();

} // namespace godot_rtmidi

#endif // GODOT_RTMIDI_BLUETOOTH_MIDI_H
