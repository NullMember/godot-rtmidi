#ifndef GODOT_RTMIDI_OUT_H
#define GODOT_RTMIDI_OUT_H

#include <RtMidi.h>
#include <godot_cpp/classes/node.hpp>

#include <atomic>

namespace godot {

class MidiOut : public Node {
    GDCLASS(MidiOut, Node)

private:
    RtMidiOut* midi_out = nullptr; // nullptr if RtMidi failed to initialize
    bool virtual_port_open = false; // RtMidi's isPortOpen() ignores virtual ports
    std::atomic<bool> error_raised{false}; // set by the error callback, to detect failed RtMidi calls
    static void midi_error_callback(RtMidiError::Type type, const std::string &error_text, void* user_data);

protected:
    static void _bind_methods();

public:
    MidiOut();
    ~MidiOut();

    Error open_port(int port_number);
    Error open_virtual_port(String port_name);
    Error close_port();
    bool is_port_open();
    static int get_port_count();
    static String get_port_name(int port_number);
    static PackedStringArray get_port_names();
    Error send_message(PackedByteArray message);
};

}

#endif
