#include "godot_rtmidi_in.h"
#include "rtmidi_common.h"
#include "bluetooth_midi.h"
#ifdef __ANDROID__
#include "android_midi.h"
#endif
#ifdef GODOT_RTMIDI_WMS
#include "wms_virtual_port.h"
#endif

#include <godot_cpp/core/class_db.hpp>

using namespace godot;
using namespace godot_rtmidi;

#define MIDI_IN_UNAVAILABLE "MIDI input is not available: RtMidi failed to initialize (see the earlier error)."

#ifdef GODOT_RTMIDI_WMS
// Same limit as RtMidi's input queue: older messages are kept, newer dropped.
static constexpr size_t WMS_QUEUE_LIMIT = 100;
#endif

void MidiIn::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_port", "port_number"), &MidiIn::open_port);
	ClassDB::bind_method(D_METHOD("open_virtual_port", "port_name"), &MidiIn::open_virtual_port);
	ClassDB::bind_method(D_METHOD("close_port"), &MidiIn::close_port);
	ClassDB::bind_method(D_METHOD("is_port_open"), &MidiIn::is_port_open);
	ClassDB::bind_static_method("MidiIn", D_METHOD("get_port_count"), &MidiIn::get_port_count);
	ClassDB::bind_static_method("MidiIn", D_METHOD("get_port_name", "port_number"), &MidiIn::get_port_name);
	ClassDB::bind_static_method("MidiIn", D_METHOD("get_port_names"), &MidiIn::get_port_names);
	ClassDB::bind_static_method("MidiIn", D_METHOD("show_bluetooth_devices"), &MidiIn::show_bluetooth_devices);
	ClassDB::bind_method(D_METHOD("ignore_types", "midi_sysex", "midi_time", "midi_sense"), &MidiIn::ignore_types, DEFVAL(true), DEFVAL(true), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_message"), &MidiIn::get_message);
	ClassDB::bind_method(D_METHOD("set_buffer_size", "buffer_size", "buffer_count"), &MidiIn::set_buffer_size);
	ClassDB::bind_method(D_METHOD("set_use_signals", "enabled"), &MidiIn::set_use_signals);
	ClassDB::bind_method(D_METHOD("is_using_signals"), &MidiIn::is_using_signals);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_signals"), "set_use_signals", "is_using_signals");
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

MidiIn::MidiIn() {
	midi_in = create_rtmidi<RtMidiIn>("MidiIn");
	if (!midi_in) {
		return;
	}
	midi_in->setErrorCallback(midi_error_callback, this);
	if (use_signals) {
		midi_in->setCallback(midi_message_callback, this);
	}
}

MidiIn::~MidiIn() {
#ifdef GODOT_RTMIDI_WMS
	close_wms_port(); // no callback runs after this
#endif
	if (!midi_in) {
		return;
	}
	// Stop delivering messages before tearing down. RtMidi's input thread may
	// still be running until closePort() returns.
	if (use_signals) {
		midi_in->cancelCallback();
	}
	midi_in->closePort();
	delete midi_in;
}

void MidiIn::set_use_signals(bool enabled) {
	if (use_signals == enabled) {
		return;
	}
	use_signals = enabled;
	if (!midi_in) {
		return;
	}
	// RtMidi only queues messages for getMessage() while no callback is set.
	if (use_signals) {
		midi_in->setCallback(midi_message_callback, this);
	} else {
		midi_in->cancelCallback();
	}
}

bool MidiIn::is_using_signals() const {
	return use_signals;
}

Error MidiIn::open_port(int port_number) {
	ERR_FAIL_NULL_V_MSG(midi_in, ERR_UNAVAILABLE, MIDI_IN_UNAVAILABLE);
	ERR_FAIL_COND_V_MSG(is_port_open(), ERR_ALREADY_IN_USE, "A MIDI input port is already open. Call close_port() first.");
	int port_count = midi_in->getPortCount();
	ERR_FAIL_COND_V_MSG(port_count == 0, ERR_CANT_OPEN, "No MIDI input ports are available.");
	ERR_FAIL_INDEX_V_MSG(port_number, port_count, ERR_INVALID_PARAMETER, vformat("MIDI input port %d does not exist (%d ports available).", port_number, port_count));
#ifdef __ANDROID__
	Error android_err = godot_rtmidi::prepare_android_open_port("MidiIn");
	if (android_err != OK) {
		return android_err;
	}
#endif
	midi_in->openPort(port_number);
	// RtMidi reports failures through the error callback; check the result.
	return midi_in->isPortOpen() ? OK : ERR_CANT_OPEN;
}

Error MidiIn::open_virtual_port(String port_name) {
	ERR_FAIL_NULL_V_MSG(midi_in, ERR_UNAVAILABLE, MIDI_IN_UNAVAILABLE);
	ERR_FAIL_COND_V_MSG(is_port_open(), ERR_ALREADY_IN_USE, "A MIDI input port is already open. Call close_port() first.");
	RtMidi::Api api = midi_in->getCurrentApi();
#ifdef GODOT_RTMIDI_WMS
	if (!api_supports_virtual_ports(api)) {
		// RtMidi can't create virtual ports on Windows; Windows MIDI Services can.
		ERR_FAIL_COND_V_MSG(port_name.is_empty(), ERR_INVALID_PARAMETER, "The virtual MIDI port needs a name.");
		Error err = OK;
		String message;
		wms_port = wms::open_virtual_port(wms::Direction::INPUT, port_name, wms_message_callback, this, err, message);
		if (!wms_port) {
			if (err == ERR_UNAVAILABLE) {
				ERR_PRINT("MidiIn: " + message);
			} else {
				report_rtmidi_error(this, "MidiIn", RtMidiError::DRIVER_ERROR, message.utf8().get_data());
			}
			return err;
		}
		virtual_port_open = true;
		return OK;
	}
#endif
	ERR_FAIL_COND_V_MSG(!api_supports_virtual_ports(api), ERR_UNAVAILABLE, virtual_ports_unsupported_message(api));
	// RtMidi's isPortOpen() doesn't cover virtual ports, and failures are only
	// reported through the error callback.
	error_raised = false;
	midi_in->openVirtualPort(port_name.utf8().get_data());
	if (error_raised) {
		return ERR_CANT_OPEN;
	}
	virtual_port_open = true;
	return OK;
}

Error MidiIn::close_port() {
	ERR_FAIL_NULL_V_MSG(midi_in, ERR_UNAVAILABLE, MIDI_IN_UNAVAILABLE);
	// Closing when nothing is open is harmless, so don't print an error.
	if (!is_port_open()) {
		return ERR_DOES_NOT_EXIST;
	}
#ifdef GODOT_RTMIDI_WMS
	if (wms_port) {
		close_wms_port();
		virtual_port_open = false;
		return OK;
	}
#endif
	// Closes both a regular and a virtual port.
	midi_in->closePort();
	virtual_port_open = false;
	return OK;
}

#ifdef GODOT_RTMIDI_WMS
void MidiIn::close_wms_port() {
	if (!wms_port) {
		return;
	}
	wms::close_virtual_port(wms_port);
	wms_port = nullptr;
	std::lock_guard<std::mutex> lock(wms_queue_mutex);
	wms_queue.clear();
}

void MidiIn::wms_message_callback(double deltatime, const std::vector<unsigned char> &message, void *user_data) {
	MidiIn *midi_in = static_cast<MidiIn *>(user_data);
	// Same filtering as RtMidi's ignoreTypes().
	const unsigned char status = message[0];
	if ((status == 0xF0 && midi_in->ignore_sysex) ||
			((status == 0xF1 || status == 0xF8) && midi_in->ignore_time) ||
			(status == 0xFE && midi_in->ignore_sense)) {
		return;
	}
	if (midi_in->use_signals) {
		emit_midi_message(midi_in, deltatime, message);
		return;
	}
	std::lock_guard<std::mutex> lock(midi_in->wms_queue_mutex);
	if (midi_in->wms_queue.size() < WMS_QUEUE_LIMIT) {
		midi_in->wms_queue.push_back(message);
	}
}
#endif

bool MidiIn::is_port_open() {
	if (!midi_in) {
		return false;
	}
	return virtual_port_open || midi_in->isPortOpen();
}

int MidiIn::get_port_count() {
	RtMidiIn *_midi_in = create_rtmidi<RtMidiIn>("MidiIn");
	ERR_FAIL_NULL_V_MSG(_midi_in, 0, MIDI_IN_UNAVAILABLE);
	int count = _midi_in->getPortCount();
	delete _midi_in;
	return count;
}

String MidiIn::get_port_name(int port_number) {
	ERR_FAIL_COND_V_MSG(port_number < 0, String(), vformat("Invalid MIDI input port number %d.", port_number));
	RtMidiIn *_midi_in = create_rtmidi<RtMidiIn>("MidiIn");
	ERR_FAIL_NULL_V_MSG(_midi_in, String(), MIDI_IN_UNAVAILABLE);
	// Port names come from the OS as UTF-8.
	String name = String::utf8(_midi_in->getPortName(port_number).c_str());
	delete _midi_in;
	return name;
}

PackedStringArray MidiIn::get_port_names() {
	RtMidiIn *_midi_in = create_rtmidi<RtMidiIn>("MidiIn");
	ERR_FAIL_NULL_V_MSG(_midi_in, PackedStringArray(), MIDI_IN_UNAVAILABLE);
	PackedStringArray names;
	for (unsigned int i = 0; i < _midi_in->getPortCount(); i++) {
		names.push_back(String::utf8(_midi_in->getPortName(i).c_str()));
	}
	delete _midi_in;
	return names;
}

Error MidiIn::show_bluetooth_devices() {
	return godot_rtmidi::show_bluetooth_midi_devices();
}

void MidiIn::ignore_types(bool midi_sysex, bool midi_time, bool midi_sense) {
	ERR_FAIL_NULL_MSG(midi_in, MIDI_IN_UNAVAILABLE);
	midi_in->ignoreTypes(midi_sysex, midi_time, midi_sense);
#ifdef GODOT_RTMIDI_WMS
	ignore_sysex = midi_sysex;
	ignore_time = midi_time;
	ignore_sense = midi_sense;
#endif
}

PackedByteArray MidiIn::get_message() {
	ERR_FAIL_NULL_V_MSG(midi_in, PackedByteArray(), MIDI_IN_UNAVAILABLE);
	ERR_FAIL_COND_V_MSG(use_signals, PackedByteArray(), "get_message() only works when use_signals is false. While signals are used, messages arrive through the midi_message signal.");
	std::vector<unsigned char> message;
#ifdef GODOT_RTMIDI_WMS
	if (wms_port) {
		std::lock_guard<std::mutex> lock(wms_queue_mutex);
		if (!wms_queue.empty()) {
			message.swap(wms_queue.front());
			wms_queue.pop_front();
		}
	} else
#endif
	{
		midi_in->getMessage(&message);
	}
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

void MidiIn::midi_message_callback(double deltatime, std::vector<unsigned char> *message, void *user_data) {
	emit_midi_message(static_cast<MidiIn *>(user_data), deltatime, *message);
}

void MidiIn::emit_midi_message(MidiIn *midi_in, double deltatime, const std::vector<unsigned char> &message) {
	PackedByteArray packed_message;
	packed_message.resize(message.size());
	for (unsigned int i = 0; i < message.size(); i++) {
		packed_message.set(i, message[i]);
	}
	midi_in->call_deferred("emit_signal", "midi_message", deltatime, packed_message);
}

void MidiIn::midi_error_callback(RtMidiError::Type type, const std::string &error_text, void *user_data) {
	MidiIn *midi_in = static_cast<MidiIn *>(user_data);
	midi_in->error_raised = true;
	report_rtmidi_error(midi_in, "MidiIn", type, error_text);
}
