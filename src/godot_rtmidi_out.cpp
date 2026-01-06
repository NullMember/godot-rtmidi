#include "godot_rtmidi_out.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

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
	midi_out = new RtMidiOut();
	midi_out->setErrorCallback(midi_error_callback, this);
}

MidiOut::~MidiOut() {
	// Add your cleanup here.
	midi_out->closePort();
	delete midi_out;
}

void MidiOut::open_port(int port_number) {
	midi_out->openPort(port_number);
}

void MidiOut::open_virtual_port(String port_name) {
	midi_out->openVirtualPort(port_name.utf8().get_data());
}

void MidiOut::close_port() {
	midi_out->closePort();
}

bool MidiOut::is_port_open() {
	return midi_out->isPortOpen();
}

int MidiOut::get_port_count() {
	RtMidiOut _midi_out;
	return _midi_out.getPortCount();
}

String MidiOut::get_port_name(int port_number) {
	RtMidiOut _midi_out;
	return _midi_out.getPortName(port_number).c_str();
}

PackedStringArray MidiOut::get_port_names() {
	RtMidiOut _midi_out;
	PackedStringArray names;
	for (unsigned int i = 0; i < _midi_out.getPortCount(); i++) {
		names.push_back(_midi_out.getPortName(i).c_str());
	}
	return names;
}

void MidiOut::send_message(PackedByteArray message) {
	if(message.size() == 0) {
		return;
	}
	std::vector<unsigned char> message_vector;
	for (int i = 0; i < message.size(); i++) {
		message_vector.push_back(message[i]);
	}
	midi_out->sendMessage(&message_vector);
}

void MidiOut::midi_error_callback(RtMidiError::Type type, const std::string &error_text, void* user_data) {
	MidiOut* midi_out = (MidiOut*)user_data;
	_err_print_error("MidiOut", "godot_rtmidi_out.cpp", 0, error_text.c_str(), false, false);
	midi_out->call_deferred("emit_signal", "midi_error", static_cast<int>(type), String(error_text.c_str()));
}

void MidiOut::_process(double delta) {
	return;
}