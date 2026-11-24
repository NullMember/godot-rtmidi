// Fallback for every platform without a Bluetooth MIDI device picker.
// The iOS implementation is in bluetooth_midi_ios.mm.

#include "bluetooth_midi.h"

godot::Error godot_rtmidi::show_bluetooth_midi_devices() {
	ERR_FAIL_V_MSG(godot::ERR_UNAVAILABLE, "The Bluetooth MIDI device list is only available on iOS. On macOS, connect Bluetooth MIDI devices in Audio MIDI Setup.");
}
