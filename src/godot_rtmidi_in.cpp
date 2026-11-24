#include "godot_rtmidi_in.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void MidiIn::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_port", "port_number"), &MidiIn::open_port);
	ClassDB::bind_method(D_METHOD("open_virtual_port", "port_name"), &MidiIn::open_virtual_port);
	ClassDB::bind_method(D_METHOD("close_port"), &MidiIn::close_port);
	ClassDB::bind_method(D_METHOD("is_port_open"), &MidiIn::is_port_open);
	ClassDB::bind_static_method("MidiIn", D_METHOD("get_port_count"), &MidiIn::get_port_count);
	ClassDB::bind_static_method("MidiIn", D_METHOD("get_port_name", "port_number"), &MidiIn::get_port_name);
	ClassDB::bind_static_method("MidiIn", D_METHOD("get_port_names"), &MidiIn::get_port_names);
	ClassDB::bind_method(D_METHOD("ignore_types", "midi_sysex", "midi_time", "midi_sense"), &MidiIn::ignore_types);
	ClassDB::bind_method(D_METHOD("get_message"), &MidiIn::get_message);
	ClassDB::bind_method(D_METHOD("set_buffer_size", "buffer_size", "buffer_count"), &MidiIn::set_buffer_size);
	ClassDB::bind_integer_constant("MidiIn", "MidiError", "ERROR_WARNING", static_cast<int>(RtMidiError::WARNING));
	ClassDB::bind_integer_constant("MidiIn", "MidiError", "ERROR_DEBUG_WARNING", static_cast<int>(RtMidiError::DEBUG_WARNING));
	ClassDB::bind_integer_constant("MidiIn", "MidiError", "ERROR_UNSPECIFIED", static_cast<int>(RtMidiError::UNSPECIFIED));
	ClassDB::bind_integer_constant("MidiIn", "MidiError", "ERROR_NO_DEVICES_FOUND", static_cast<int>(RtMidiError::NO_DEVICES_FOUND));
	ClassDB::bind_integer_constant("MidiIn", "MidiError", "ERROR_INVALID_DEVICE", static_cast<int>(RtMidiError::INVALID_DEVICE));
	ClassDB::bind_integer_constant("MidiIn", "MidiError", "ERROR_MEMORY_ERROR", static_cast<int>(RtMidiError::MEMORY_ERROR));
	ClassDB::bind_integer_constant("MidiIn", "MidiError", "ERROR_INVALID_PARAMETER", static_cast<int>(RtMidiError::INVALID_PARAMETER));
	ClassDB::bind_integer_constant("MidiIn", "MidiError", "ERROR_INVALID_USE", static_cast<int>(RtMidiError::INVALID_USE));
	ClassDB::bind_integer_constant("MidiIn", "MidiError", "ERROR_DRIVER_ERROR", static_cast<int>(RtMidiError::DRIVER_ERROR));
	ClassDB::bind_integer_constant("MidiIn", "MidiError", "ERROR_SYSTEM_ERROR", static_cast<int>(RtMidiError::SYSTEM_ERROR));
	ClassDB::bind_integer_constant("MidiIn", "MidiError", "ERROR_THREAD_ERROR", static_cast<int>(RtMidiError::THREAD_ERROR));
	ADD_SIGNAL(MethodInfo("midi_message", PropertyInfo(Variant::FLOAT, "deltatime"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "message")));
	ADD_SIGNAL(MethodInfo("midi_error", PropertyInfo(Variant::INT, "error_type"), PropertyInfo(Variant::STRING, "error_message")));
}

#define MIDI_IN_UNAVAILABLE "MIDI input is not available: RtMidi failed to initialize (see the earlier error)."

// RtMidi throws RtMidiError from its constructor on fatal errors (e.g. the MIDI
// driver can't be opened). Exceptions must never reach Godot, so every
// construction goes through here. Returns nullptr on failure.
static void print_error_callback(RtMidiError::Type type, const std::string &error_text, void *user_data) {
	ERR_PRINT(String("MidiIn: ") + String::utf8(error_text.c_str()));
}

static RtMidiIn *create_rtmidi_in() {
	try {
		RtMidiIn *rtmidi = new RtMidiIn();
		// Once an error callback is set, RtMidi reports errors through it instead of throwing.
		rtmidi->setErrorCallback(print_error_callback, nullptr);
		return rtmidi;
	} catch (const RtMidiError &error) {
		ERR_PRINT(String("MidiIn: failed to initialize RtMidi: ") + String::utf8(error.what()));
	} catch (const std::exception &error) {
		ERR_PRINT(String("MidiIn: failed to initialize RtMidi: ") + String::utf8(error.what()));
	}
	return nullptr;
}

MidiIn::MidiIn() {
	midi_in = create_rtmidi_in();
	if (!midi_in) {
		return;
	}
	midi_in->setErrorCallback(midi_error_callback, this);
	midi_in->setCallback(midi_message_callback, this);
}

MidiIn::~MidiIn() {
	if (!midi_in) {
		return;
	}
	midi_in->closePort();
	delete midi_in;
}

void MidiIn::open_port(int port_number) {
	ERR_FAIL_NULL_MSG(midi_in, MIDI_IN_UNAVAILABLE);
	midi_in->openPort(port_number);
}

void MidiIn::open_virtual_port(String port_name) {
	ERR_FAIL_NULL_MSG(midi_in, MIDI_IN_UNAVAILABLE);
	midi_in->openVirtualPort(port_name.utf8().get_data());
}

void MidiIn::close_port() {
	ERR_FAIL_NULL_MSG(midi_in, MIDI_IN_UNAVAILABLE);
	midi_in->closePort();
}

bool MidiIn::is_port_open() {
	if (!midi_in) {
		return false;
	}
	return midi_in->isPortOpen();
}

int MidiIn::get_port_count() {
	RtMidiIn *_midi_in = create_rtmidi_in();
	ERR_FAIL_NULL_V_MSG(_midi_in, 0, MIDI_IN_UNAVAILABLE);
	int count = _midi_in->getPortCount();
	delete _midi_in;
	return count;
}

String MidiIn::get_port_name(int port_number) {
	RtMidiIn *_midi_in = create_rtmidi_in();
	ERR_FAIL_NULL_V_MSG(_midi_in, String(), MIDI_IN_UNAVAILABLE);
	String name = _midi_in->getPortName(port_number).c_str();
	delete _midi_in;
	return name;
}

PackedStringArray MidiIn::get_port_names() {
	RtMidiIn *_midi_in = create_rtmidi_in();
	ERR_FAIL_NULL_V_MSG(_midi_in, PackedStringArray(), MIDI_IN_UNAVAILABLE);
	PackedStringArray names;
	for (unsigned int i = 0; i < _midi_in->getPortCount(); i++) {
		names.push_back(_midi_in->getPortName(i).c_str());
	}
	delete _midi_in;
	return names;
}

void MidiIn::ignore_types(bool midi_sysex = true, bool midi_time = true, bool midi_sense = true) {
	ERR_FAIL_NULL_MSG(midi_in, MIDI_IN_UNAVAILABLE);
	midi_in->ignoreTypes(midi_sysex, midi_time, midi_sense);
}

PackedByteArray MidiIn::get_message() {
	ERR_FAIL_NULL_V_MSG(midi_in, PackedByteArray(), MIDI_IN_UNAVAILABLE);
	std::vector<unsigned char> message;
	midi_in->getMessage(&message);
	PackedByteArray packed_message;
	packed_message.resize(message.size());
	for (unsigned int i = 0; i < message.size(); i++) {
		packed_message.set(i, message[i]);
	}
	return packed_message;
}

void MidiIn::set_buffer_size(unsigned int buffer_size, unsigned int buffer_count) {
	ERR_FAIL_NULL_MSG(midi_in, MIDI_IN_UNAVAILABLE);
	midi_in->setBufferSize(buffer_size, buffer_count);
}

void MidiIn::midi_message_callback(double deltatime, std::vector<unsigned char>* message, void* user_data) {
	MidiIn* midi_in = (MidiIn*)user_data;
	PackedByteArray packed_message;
	packed_message.resize(message->size());
	for (unsigned int i = 0; i < message->size(); i++) {
		packed_message.set(i, (*message)[i]);
	}
	midi_in->call_deferred("emit_signal", "midi_message", deltatime, packed_message);
}

void MidiIn::midi_error_callback(RtMidiError::Type type, const std::string &error_text, void* user_data) {
	MidiIn* midi_in = (MidiIn*)user_data;
	_err_print_error("MidiIn", "godot_rtmidi_in.cpp", 0, error_text.c_str(), false, false);
	midi_in->call_deferred("emit_signal", "midi_error", static_cast<int>(type), String(error_text.c_str()));
}

void MidiIn::_process(double delta) {
	return;
}