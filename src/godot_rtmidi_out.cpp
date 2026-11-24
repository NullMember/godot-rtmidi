#include "godot_rtmidi_out.h"
#include "rtmidi_common.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;
using namespace godot_rtmidi;

#define MIDI_OUT_UNAVAILABLE "MIDI output is not available: RtMidi failed to initialize (see the earlier error)."

void MidiOut::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_port", "port_number"), &MidiOut::open_port);
	ClassDB::bind_method(D_METHOD("open_virtual_port", "port_name"), &MidiOut::open_virtual_port);
	ClassDB::bind_method(D_METHOD("close_port"), &MidiOut::close_port);
	ClassDB::bind_method(D_METHOD("is_port_open"), &MidiOut::is_port_open);
	ClassDB::bind_static_method("MidiOut", D_METHOD("get_port_count"), &MidiOut::get_port_count);
	ClassDB::bind_static_method("MidiOut", D_METHOD("get_port_name", "port_number"), &MidiOut::get_port_name);
	ClassDB::bind_static_method("MidiOut", D_METHOD("get_port_names"), &MidiOut::get_port_names);
	ClassDB::bind_method(D_METHOD("send_message", "message"), &MidiOut::send_message);
	ClassDB::bind_integer_constant("MidiOut", "MidiError", "ERROR_WARNING", static_cast<int>(RtMidiError::WARNING));
	ClassDB::bind_integer_constant("MidiOut", "MidiError", "ERROR_DEBUG_WARNING", static_cast<int>(RtMidiError::DEBUG_WARNING));
	ClassDB::bind_integer_constant("MidiOut", "MidiError", "ERROR_UNSPECIFIED", static_cast<int>(RtMidiError::UNSPECIFIED));
	ClassDB::bind_integer_constant("MidiOut", "MidiError", "ERROR_NO_DEVICES_FOUND", static_cast<int>(RtMidiError::NO_DEVICES_FOUND));
	ClassDB::bind_integer_constant("MidiOut", "MidiError", "ERROR_INVALID_DEVICE", static_cast<int>(RtMidiError::INVALID_DEVICE));
	ClassDB::bind_integer_constant("MidiOut", "MidiError", "ERROR_MEMORY_ERROR", static_cast<int>(RtMidiError::MEMORY_ERROR));
	ClassDB::bind_integer_constant("MidiOut", "MidiError", "ERROR_INVALID_PARAMETER", static_cast<int>(RtMidiError::INVALID_PARAMETER));
	ClassDB::bind_integer_constant("MidiOut", "MidiError", "ERROR_INVALID_USE", static_cast<int>(RtMidiError::INVALID_USE));
	ClassDB::bind_integer_constant("MidiOut", "MidiError", "ERROR_DRIVER_ERROR", static_cast<int>(RtMidiError::DRIVER_ERROR));
	ClassDB::bind_integer_constant("MidiOut", "MidiError", "ERROR_SYSTEM_ERROR", static_cast<int>(RtMidiError::SYSTEM_ERROR));
	ClassDB::bind_integer_constant("MidiOut", "MidiError", "ERROR_THREAD_ERROR", static_cast<int>(RtMidiError::THREAD_ERROR));
	ADD_SIGNAL(MethodInfo("midi_error", PropertyInfo(Variant::INT, "error_type"), PropertyInfo(Variant::STRING, "error_message")));
}

MidiOut::MidiOut() {
	midi_out = create_rtmidi<RtMidiOut>("MidiOut");
	if (!midi_out) {
		return;
	}
	midi_out->setErrorCallback(midi_error_callback, this);
}

MidiOut::~MidiOut() {
	if (!midi_out) {
		return;
	}
	midi_out->closePort();
	delete midi_out;
}

Error MidiOut::open_port(int port_number) {
	ERR_FAIL_NULL_V_MSG(midi_out, ERR_UNAVAILABLE, MIDI_OUT_UNAVAILABLE);
	ERR_FAIL_COND_V_MSG(is_port_open(), ERR_ALREADY_IN_USE, "A MIDI output port is already open. Call close_port() first.");
	int port_count = midi_out->getPortCount();
	ERR_FAIL_COND_V_MSG(port_count == 0, ERR_CANT_OPEN, "No MIDI output ports are available.");
	ERR_FAIL_INDEX_V_MSG(port_number, port_count, ERR_INVALID_PARAMETER, vformat("MIDI output port %d does not exist (%d ports available).", port_number, port_count));
	midi_out->openPort(port_number);
	// RtMidi reports failures through the error callback; check the result.
	return midi_out->isPortOpen() ? OK : ERR_CANT_OPEN;
}

Error MidiOut::open_virtual_port(String port_name) {
	ERR_FAIL_NULL_V_MSG(midi_out, ERR_UNAVAILABLE, MIDI_OUT_UNAVAILABLE);
	ERR_FAIL_COND_V_MSG(is_port_open(), ERR_ALREADY_IN_USE, "A MIDI output port is already open. Call close_port() first.");
	RtMidi::Api api = midi_out->getCurrentApi();
	ERR_FAIL_COND_V_MSG(!api_supports_virtual_ports(api), ERR_UNAVAILABLE, virtual_ports_unsupported_message(api));
	// RtMidi's isPortOpen() doesn't cover virtual ports, and failures are only
	// reported through the error callback.
	error_raised = false;
	midi_out->openVirtualPort(port_name.utf8().get_data());
	if (error_raised) {
		return ERR_CANT_OPEN;
	}
	virtual_port_open = true;
	return OK;
}

Error MidiOut::close_port() {
	ERR_FAIL_NULL_V_MSG(midi_out, ERR_UNAVAILABLE, MIDI_OUT_UNAVAILABLE);
	// Closing when nothing is open is harmless, so don't print an error.
	if (!is_port_open()) {
		return ERR_DOES_NOT_EXIST;
	}
	// Closes both a regular and a virtual port.
	midi_out->closePort();
	virtual_port_open = false;
	return OK;
}

bool MidiOut::is_port_open() {
	if (!midi_out) {
		return false;
	}
	return virtual_port_open || midi_out->isPortOpen();
}

int MidiOut::get_port_count() {
	RtMidiOut *_midi_out = create_rtmidi<RtMidiOut>("MidiOut");
	ERR_FAIL_NULL_V_MSG(_midi_out, 0, MIDI_OUT_UNAVAILABLE);
	int count = _midi_out->getPortCount();
	delete _midi_out;
	return count;
}

String MidiOut::get_port_name(int port_number) {
	ERR_FAIL_COND_V_MSG(port_number < 0, String(), vformat("Invalid MIDI output port number %d.", port_number));
	RtMidiOut *_midi_out = create_rtmidi<RtMidiOut>("MidiOut");
	ERR_FAIL_NULL_V_MSG(_midi_out, String(), MIDI_OUT_UNAVAILABLE);
	// Port names come from the OS as UTF-8.
	String name = String::utf8(_midi_out->getPortName(port_number).c_str());
	delete _midi_out;
	return name;
}

PackedStringArray MidiOut::get_port_names() {
	RtMidiOut *_midi_out = create_rtmidi<RtMidiOut>("MidiOut");
	ERR_FAIL_NULL_V_MSG(_midi_out, PackedStringArray(), MIDI_OUT_UNAVAILABLE);
	PackedStringArray names;
	for (unsigned int i = 0; i < _midi_out->getPortCount(); i++) {
		names.push_back(String::utf8(_midi_out->getPortName(i).c_str()));
	}
	delete _midi_out;
	return names;
}

Error MidiOut::send_message(PackedByteArray message) {
	ERR_FAIL_NULL_V_MSG(midi_out, ERR_UNAVAILABLE, MIDI_OUT_UNAVAILABLE);
	ERR_FAIL_COND_V_MSG(message.is_empty(), ERR_INVALID_PARAMETER, "Cannot send an empty MIDI message.");
	ERR_FAIL_COND_V_MSG(!is_port_open(), ERR_UNCONFIGURED, "No MIDI output port is open. Call open_port() or open_virtual_port() first.");

	std::vector<unsigned char> message_vector(message.ptr(), message.ptr() + message.size());
	// RtMidi reports send failures through the error callback, not a return value.
	error_raised = false;
	midi_out->sendMessage(&message_vector);
	return error_raised ? FAILED : OK;
}

void MidiOut::midi_error_callback(RtMidiError::Type type, const std::string &error_text, void *user_data) {
	MidiOut *midi_out = static_cast<MidiOut *>(user_data);
	midi_out->error_raised = true;
	report_rtmidi_error(midi_out, "MidiOut", type, error_text);
}
