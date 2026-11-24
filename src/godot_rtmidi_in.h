#ifndef GODOT_RTMIDI_IN_H
#define GODOT_RTMIDI_IN_H

#include <RtMidi.h>
#include <godot_cpp/classes/node.hpp>

#include <atomic>

namespace godot {

class MidiIn : public Node {
	GDCLASS(MidiIn, Node)

private:
	RtMidiIn* midi_in = nullptr; // nullptr if RtMidi failed to initialize
	bool use_signals = true; // false: messages are queued for get_message() instead
	bool virtual_port_open = false; // RtMidi's isPortOpen() ignores virtual ports
	std::atomic<bool> error_raised{false}; // set by the error callback, to detect failed RtMidi calls
    static void midi_message_callback(double deltatime, std::vector<unsigned char>* message, void* user_data);
    static void midi_error_callback(RtMidiError::Type type, const std::string &error_text, void* user_data);

protected:
	static void _bind_methods();

public:
	MidiIn();
	~MidiIn();

    Error open_port(int port_number);
    Error open_virtual_port(String port_name);
    Error close_port();
    bool is_port_open();
    static int get_port_count();
    static String get_port_name(int port_number);
    static PackedStringArray get_port_names();
    static Error show_bluetooth_devices();
    void ignore_types(bool midi_sysex = true, bool midi_time = true, bool midi_sense = true);
    PackedByteArray get_message();
    void set_buffer_size(unsigned int buffer_size, unsigned int buffer_count);
    void set_use_signals(bool enabled);
    bool is_using_signals() const;
};

}

#endif