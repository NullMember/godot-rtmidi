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
    ClassDB::bind_method( D_METHOD( "_emit_midi_message", "deltatime", "message" ),
                          &MidiIn::_emit_midi_message );
	ADD_SIGNAL(MethodInfo("midi_message", PropertyInfo(Variant::FLOAT, "deltatime"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "message")));
}

MidiIn::MidiIn() {
	midi_in = new RtMidiIn();
	message_callback = midi_message_callback;
	error_callback = midi_error_callback;
	midi_in->setCallback(message_callback, this);
	midi_in->setErrorCallback(error_callback, this);
}

MidiIn::~MidiIn() {
	midi_in->closePort();
	delete midi_in;
}

void MidiIn::open_port(int port_number) {
	midi_in->openPort(port_number);
}

void MidiIn::open_virtual_port(String port_name) {
	midi_in->openVirtualPort(port_name.utf8().get_data());
}

void MidiIn::close_port() {
	midi_in->closePort();
}

bool MidiIn::is_port_open() {
	return midi_in->isPortOpen();
}

int MidiIn::get_port_count() {
	RtMidiIn _midi_in;
	return _midi_in.getPortCount();
}

String MidiIn::get_port_name(int port_number) {
	RtMidiIn _midi_in;
	return _midi_in.getPortName(port_number).c_str();
}

PackedStringArray MidiIn::get_port_names() {
	RtMidiIn _midi_in;
	PackedStringArray names;
	for (unsigned int i = 0; i < _midi_in.getPortCount(); i++) {
		names.push_back(_midi_in.getPortName(i).c_str());
	}
	return names;
}

void MidiIn::ignore_types(bool midi_sysex = true, bool midi_time = true, bool midi_sense = true) {
	midi_in->ignoreTypes(midi_sysex, midi_time, midi_sense);
}

PackedByteArray MidiIn::get_message() {
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
	midi_in->setBufferSize(buffer_size, buffer_count);
}

void MidiIn::midi_message_callback(double deltatime, std::vector<unsigned char>* message, void* user_data) {
	MidiIn* midi_in = (MidiIn*)user_data;
	PackedByteArray packed_message;
	packed_message.resize(message->size());
	for (unsigned int i = 0; i < message->size(); i++) {
		packed_message.set(i, (*message)[i]);
	}
	// midi_in->emit_signal("midi_message", deltatime, packed_message);
    midi_in->call_deferred( "_emit_midi_message", deltatime, packed_message);
}

void MidiIn::_emit_midi_message( double deltatime, PackedByteArray message )
{
    emit_signal( "midi_message", deltatime, message );
}

void MidiIn::midi_error_callback(RtMidiError::Type type, const std::string &error_text, void* user_data) {
	_err_print_error("MidiIn", "godot_rtmidi_in.cpp", 0, error_text.c_str(), false, false);
}

void MidiIn::_process(double delta) {
	return;
}