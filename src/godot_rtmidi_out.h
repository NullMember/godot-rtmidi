#ifndef GODOT_RTMIDI_OUT_H
#define GODOT_RTMIDI_OUT_H

#include <RtMidi.h>
#include <godot_cpp/classes/node.hpp>

namespace godot {

class MidiOut : public Node {
    GDCLASS(MidiOut, Node)

private:
    RtMidiOut* midi_out;
    void (*error_callback)(RtMidiError::Type type, const std::string &error_text, void* user_data);
    static void midi_error_callback(RtMidiError::Type type, const std::string &error_text, void* user_data);

protected:
    static void _bind_methods();

public:
    MidiOut();
    ~MidiOut();

    void open_port(int port_number);
    void open_virtual_port(String port_name);
    void close_port();
    bool is_port_open();
    static int get_port_count();
    static String get_port_name(int port_number);
    static PackedStringArray get_port_names();
    void send_message(PackedByteArray message);

    void _process(double delta) override;
};

}

#endif