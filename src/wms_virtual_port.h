#ifndef GODOT_RTMIDI_WMS_VIRTUAL_PORT_H
#define GODOT_RTMIDI_WMS_VIRTUAL_PORT_H

// Virtual MIDI ports on Windows through Windows MIDI Services (Windows 11 25H2
// and later). RtMidi 6.1.0's Windows MIDI Services backend doesn't implement
// virtual ports yet, so MidiIn/MidiOut use this for open_virtual_port() only;
// everything else stays on RtMidi.
//
// A port is an app-to-app virtual device whose single function block other
// apps see as an ordinary MIDI 1.0 port (also in WinMM). Our side of it is a
// Windows MIDI Services connection, so messages are converted between MIDI 1.0
// bytes and UMP here.
//
// This header doesn't include C++/WinRT, so it can be used from C++17 code.

#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/string.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace godot_rtmidi::wms {

enum class Direction {
	INPUT, // other apps send to the port, we receive (MidiIn)
	OUTPUT, // we send, other apps receive (MidiOut)
};

// Called on a Windows MIDI Services thread with one complete MIDI 1.0 message.
// deltatime is in seconds since the previous message (0 for the first one).
using MessageCallback = void (*)(double deltatime, const std::vector<unsigned char> &message, void *user_data);

class VirtualPort;

// Creates a virtual port. For INPUT, callback receives incoming messages.
// Returns nullptr on failure, with the reason in error and error_message:
// ERR_UNAVAILABLE when Windows MIDI Services or its API can't be used on this
// PC, ERR_CANT_OPEN when creating the port failed, ERR_TIMEOUT when the MIDI
// service didn't answer. Blocks until the port exists (the first use can take
// several seconds while Windows starts the MIDI service).
VirtualPort *open_virtual_port(Direction direction, const godot::String &name, MessageCallback callback, void *user_data, godot::Error &error, godot::String &error_message);

// Removes the port. No callback runs after this returns. Never blocks longer
// than a few seconds, even if the MIDI service hangs.
void close_virtual_port(VirtualPort *port);

// Sends one complete MIDI 1.0 message (OUTPUT ports).
godot::Error send_message(VirtualPort *port, const unsigned char *data, size_t size, godot::String &error_message);

// MIDI 1.0 <-> UMP conversion, exposed for tests. Group 0 only.
// Appends the UMP words for one complete MIDI 1.0 message. Returns false if
// the message isn't valid MIDI 1.0.
bool midi1_to_ump(const unsigned char *data, size_t size, std::vector<uint32_t> &words);

// Reassembles MIDI 1.0 messages from incoming UMP packets.
class UmpToMidi1 {
public:
	// Feeds one UMP packet. Returns true when message holds a complete MIDI
	// 1.0 message. Packets that have no MIDI 1.0 equivalent (UMP stream and
	// utility messages, MIDI 2.0 channel voice) are ignored, as are groups
	// other than 0.
	bool feed(const uint32_t *words, size_t word_count, std::vector<unsigned char> &message);

private:
	std::vector<unsigned char> sysex;
	bool in_sysex = false;
};

} // namespace godot_rtmidi::wms

#endif // GODOT_RTMIDI_WMS_VIRTUAL_PORT_H
