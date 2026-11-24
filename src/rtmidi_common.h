#ifndef GODOT_RTMIDI_COMMON_H
#define GODOT_RTMIDI_COMMON_H

// Helpers shared by MidiIn and MidiOut.

#include <RtMidi.h>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot_rtmidi {

inline bool is_warning(RtMidiError::Type type) {
	return type == RtMidiError::WARNING || type == RtMidiError::DEBUG_WARNING;
}

// Prints an RtMidi error to the Godot console as a warning or an error.
inline void print_rtmidi_error(const char *class_name, RtMidiError::Type type, const std::string &error_text) {
	godot::String message = godot::String(class_name) + ": " + godot::String::utf8(error_text.c_str());
	if (is_warning(type)) {
		WARN_PRINT(message);
	} else {
		ERR_PRINT(message);
	}
}

// Reports an RtMidi error from an instance: emits the midi_error signal and
// prints it only if nothing is connected to that signal. Called synchronously
// for most errors, but may run on RtMidi's thread; the signal is deferred.
inline void report_rtmidi_error(godot::Object *object, const char *class_name, RtMidiError::Type type, const std::string &error_text) {
	if (object->get_signal_connection_list("midi_error").is_empty()) {
		print_rtmidi_error(class_name, type, error_text);
	}
	object->call_deferred("emit_signal", "midi_error", static_cast<int>(type), godot::String::utf8(error_text.c_str()));
}

// Error callback for RtMidi objects that have no Godot instance to signal on.
// user_data is the class name used as message prefix.
inline void print_error_callback(RtMidiError::Type type, const std::string &error_text, void *user_data) {
	print_rtmidi_error(static_cast<const char *>(user_data), type, error_text);
}

// Creates an RtMidiIn or RtMidiOut. RtMidi throws RtMidiError from its
// constructor on fatal errors (e.g. the MIDI driver can't be opened).
// Exceptions must never reach Godot, so every construction goes through here.
// Once an error callback is set, RtMidi reports errors through it instead of
// throwing. Returns nullptr on failure.
template <typename T>
T *create_rtmidi(const char *class_name) {
	try {
		T *rtmidi = new T();
		rtmidi->setErrorCallback(print_error_callback, const_cast<char *>(class_name));
		return rtmidi;
	} catch (const RtMidiError &error) {
		ERR_PRINT(godot::String(class_name) + ": failed to initialize RtMidi: " + godot::String::utf8(error.what()));
	} catch (const std::exception &error) {
		ERR_PRINT(godot::String(class_name) + ": failed to initialize RtMidi: " + godot::String::utf8(error.what()));
	}
	return nullptr;
}

// Only CoreMIDI (macOS, iOS), ALSA and JACK can create virtual ports.
inline bool api_supports_virtual_ports(RtMidi::Api api) {
	return api == RtMidi::MACOSX_CORE || api == RtMidi::LINUX_ALSA || api == RtMidi::UNIX_JACK;
}

inline godot::String virtual_ports_unsupported_message(RtMidi::Api api) {
	godot::String message = godot::String("Virtual MIDI ports are not supported by the ") + RtMidi::getApiDisplayName(api).c_str() + " MIDI API.";
	if (api == RtMidi::WINDOWS_MM || api == RtMidi::WINDOWS_UWP || api == RtMidi::WINDOWS_MIDI_SERVICES) {
		message += " On Windows, create a virtual port with a loopback driver such as loopMIDI and open it with open_port() instead.";
	}
	return message;
}

} // namespace godot_rtmidi

#endif // GODOT_RTMIDI_COMMON_H
