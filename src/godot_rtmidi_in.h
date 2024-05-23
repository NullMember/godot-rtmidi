#ifndef GODOT_RTMIDI_H
#define GODOT_RTMIDI_H

#include <RtMidi.h>
#include <godot_cpp/classes/node.hpp>

namespace godot {

class MidiIn : public Node {
	GDCLASS(MidiIn, Node)

private:
	RtMidiIn* midi_in;
    void (*message_callback)(double deltatime, std::vector<unsigned char>* message, void* user_data);
    void (*error_callback)(RtMidiError::Type type, const std::string &error_text, void* user_data);
    static void midi_message_callback(double deltatime, std::vector<unsigned char>* message, void* user_data);
    static void midi_error_callback(RtMidiError::Type type, const std::string &error_text, void* user_data);
    void MidiIn::_emit_midi_message( double deltatime, PackedByteArray message );

protected:
	static void _bind_methods();

public:
	MidiIn();
	~MidiIn();

    void open_port(int port_number);
    void open_virtual_port(String port_name);
    void close_port();
    bool is_port_open();
    static int get_port_count();
    static String get_port_name(int port_number);
    static PackedStringArray get_port_names();
    void ignore_types(bool midi_sysex, bool midi_time, bool midi_sense);
    PackedByteArray get_message();
    void set_buffer_size(unsigned int buffer_size, unsigned int buffer_count);

	void _process(double delta) override;
};

}

#endif