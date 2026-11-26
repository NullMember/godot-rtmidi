#include "wms_virtual_port.h"

#include <godot_cpp/variant/char_string.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Transports.Virtual.h>

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace godot;

namespace midi2 = winrt::Windows::Devices::Midi2;
namespace midi2enum = winrt::Windows::Devices::Midi2::Enumeration;
namespace midi2virtual = winrt::Windows::Devices::Midi2::Transports::Virtual;

namespace godot_rtmidi::wms {

// ---------------------------------------------------------------------------
// MIDI 1.0 <-> UMP
// ---------------------------------------------------------------------------

// Length of a MIDI 1.0 message by its status byte, 0 if it can't stand alone.
static size_t midi1_message_length(unsigned char status) {
	if (status < 0x80) {
		return 0;
	}
	if (status < 0xF0) {
		unsigned char type = status & 0xF0;
		return (type == 0xC0 || type == 0xD0) ? 2 : 3;
	}
	switch (status) {
		case 0xF1: // MTC quarter frame
		case 0xF3: // song select
			return 2;
		case 0xF2: // song position
			return 3;
		case 0xF6: // tune request
			return 1;
		case 0xF0: // SysEx is handled separately
		case 0xF4: // undefined
		case 0xF5: // undefined
		case 0xF7: // end of SysEx, never alone
			return 0;
		default: // real-time, 0xF8-0xFF
			return 1;
	}
}

bool midi1_to_ump(const unsigned char *data, size_t size, std::vector<uint32_t> &words) {
	if (size == 0) {
		return false;
	}
	const unsigned char status = data[0];
	for (size_t i = 1; i < size; i++) {
		// Only SysEx's closing F7 may be a status byte after the first one.
		if (data[i] >= 0x80 && !(status == 0xF0 && i == size - 1 && data[i] == 0xF7)) {
			return false;
		}
	}

	if (status == 0xF0) {
		if (size < 2 || data[size - 1] != 0xF7) {
			return false;
		}
		// Payload without F0/F7, in SysEx7 packets of up to 6 bytes.
		const unsigned char *payload = data + 1;
		const size_t payload_size = size - 2;
		size_t offset = 0;
		do {
			const size_t count = std::min<size_t>(6, payload_size - offset);
			const bool first = offset == 0;
			const bool last = offset + count == payload_size;
			// 0: complete in one packet, 1: start, 2: continue, 3: end
			const uint32_t packet_status = first ? (last ? 0 : 1) : (last ? 3 : 2);
			unsigned char bytes[6] = {};
			for (size_t i = 0; i < count; i++) {
				bytes[i] = payload[offset + i];
			}
			words.push_back(0x30000000u | (packet_status << 20) | (static_cast<uint32_t>(count) << 16) | (bytes[0] << 8) | bytes[1]);
			words.push_back((static_cast<uint32_t>(bytes[2]) << 24) | (bytes[3] << 16) | (bytes[4] << 8) | bytes[5]);
			offset += count;
		} while (offset < payload_size);
		return true;
	}

	const size_t length = midi1_message_length(status);
	if (length == 0 || size != length) {
		return false;
	}
	const uint32_t d1 = length > 1 ? data[1] : 0;
	const uint32_t d2 = length > 2 ? data[2] : 0;
	// Type 2: MIDI 1.0 channel voice, type 1: system common and real-time.
	const uint32_t type = status < 0xF0 ? 0x20000000u : 0x10000000u;
	words.push_back(type | (static_cast<uint32_t>(status) << 16) | (d1 << 8) | d2);
	return true;
}

bool UmpToMidi1::feed(const uint32_t *words, size_t word_count, std::vector<unsigned char> &message) {
	if (word_count == 0) {
		return false;
	}
	const uint32_t w0 = words[0];
	const uint32_t type = w0 >> 28;
	const uint32_t group = (w0 >> 24) & 0x0F;
	if ((type != 0x1 && type != 0x2 && type != 0x3) || group != 0) {
		return false;
	}

	if (type == 0x3) {
		if (word_count < 2) {
			return false;
		}
		const uint32_t packet_status = (w0 >> 20) & 0x0F;
		const size_t count = std::min<uint32_t>((w0 >> 16) & 0x0F, 6);
		const unsigned char bytes[6] = {
			static_cast<unsigned char>((w0 >> 8) & 0x7F),
			static_cast<unsigned char>(w0 & 0x7F),
			static_cast<unsigned char>((words[1] >> 24) & 0x7F),
			static_cast<unsigned char>((words[1] >> 16) & 0x7F),
			static_cast<unsigned char>((words[1] >> 8) & 0x7F),
			static_cast<unsigned char>(words[1] & 0x7F),
		};
		if (packet_status == 0 || packet_status == 1) { // complete, start
			sysex.assign(1, 0xF0);
			in_sysex = true;
		} else if (!in_sysex) { // continue or end without a start
			return false;
		}
		sysex.insert(sysex.end(), bytes, bytes + count);
		if (packet_status == 0 || packet_status == 3) { // complete, end
			sysex.push_back(0xF7);
			message.swap(sysex);
			sysex.clear();
			in_sysex = false;
			return true;
		}
		return false;
	}

	const unsigned char status = static_cast<unsigned char>((w0 >> 16) & 0xFF);
	if ((type == 0x2) != (status >= 0x80 && status < 0xF0)) {
		return false;
	}
	const size_t length = midi1_message_length(status);
	if (length == 0) {
		return false;
	}
	message.clear();
	message.push_back(status);
	if (length > 1) {
		message.push_back(static_cast<unsigned char>((w0 >> 8) & 0x7F));
	}
	if (length > 2) {
		message.push_back(static_cast<unsigned char>(w0 & 0x7F));
	}
	return true;
}

// ---------------------------------------------------------------------------
// Windows MIDI Services
// ---------------------------------------------------------------------------

namespace {

constexpr auto SERVICE_TIMEOUT = std::chrono::seconds(20); // first use starts the MIDI service (~10 s seen)
constexpr auto CREATE_TIMEOUT = std::chrono::seconds(10);
constexpr auto CLOSE_TIMEOUT = std::chrono::seconds(5);

// The API is supported from Windows 11 25H2. 24H2 has an older Windows MIDI
// Services that never gets the API, even if a copy of it loads.
constexpr DWORD WINDOWS_11_25H2_BUILD = 26200;

const char *const LOOPBACK_HINT = " Until then, create a virtual port with a loopback driver such as loopMIDI and open it with open_port().";

struct PortState {
	Direction direction = Direction::INPUT;
	MessageCallback callback = nullptr;
	void *user_data = nullptr;

	// Held while a callback runs; close sets `closed` under it, so no callback
	// runs after close_virtual_port() returns.
	std::mutex callback_mutex;
	bool closed = false;
	UmpToMidi1 decoder;
	std::vector<unsigned char> message;
	uint64_t last_timestamp = 0;
	bool has_last_timestamp = false;
	double timestamp_frequency = 1.0;

	midi2::MidiSession session{ nullptr };
	midi2virtual::MidiVirtualDevice device{ nullptr };
	midi2::MidiEndpointConnection connection{ nullptr };
	winrt::event_token message_token{};
};

DWORD windows_build_number() {
	// GetVersionEx lies to processes without a Windows 10 manifest, and a
	// library can't control its host's manifest.
	using RtlGetVersionProc = LONG(WINAPI *)(PRTL_OSVERSIONINFOW);
	auto rtl_get_version = reinterpret_cast<RtlGetVersionProc>(::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
	RTL_OSVERSIONINFOW info{ sizeof(info) };
	if (rtl_get_version == nullptr || rtl_get_version(&info) != 0) {
		return 0;
	}
	return info.dwBuildNumber;
}

// Never initialize COM on the caller's thread (Godot owns it). Keeping the
// process's multi-threaded apartment alive lets threads without COM run in it
// implicitly. The cookie is deliberately never released. Same as RtMidi.
void ensure_mta() {
	static std::once_flag once;
	std::call_once(once, [] {
		CO_MTA_USAGE_COOKIE cookie = nullptr;
		CoIncrementMTAUsage(&cookie);
	});
}

// Runs f on its own thread and waits at most `timeout`. On timeout the thread
// is left behind (whatever it holds must be owned by f) and false is returned.
template <typename F>
bool run_with_timeout(F &&f, std::chrono::milliseconds timeout) {
	auto task = std::make_shared<std::packaged_task<void()>>(std::forward<F>(f));
	std::future<void> done = task->get_future();
	std::thread([task] { (*task)(); }).detach();
	return done.wait_for(timeout) == std::future_status::ready;
}

std::string winrt_error_text(const winrt::hresult_error &e) {
	return winrt::to_string(e.message()) + " (HRESULT 0x" + [&] {
		char hex[9];
		snprintf(hex, sizeof(hex), "%08X", static_cast<uint32_t>(e.code()));
		return std::string(hex);
	}() + ")";
}

// Checks that the API exists and can be used, and starts the MIDI service.
// Success is remembered; a failure is checked again next time.
Error check_availability(String &error_message) {
	static std::mutex mutex;
	static bool available = false;
	std::lock_guard<std::mutex> lock(mutex);
	if (available) {
		return OK;
	}

	const DWORD build = windows_build_number();
	if (build < WINDOWS_11_25H2_BUILD) {
		error_message = String("Virtual MIDI ports on Windows need Windows MIDI Services, which needs Windows 11 25H2 or later.") + LOOPBACK_HINT;
		return ERR_UNAVAILABLE;
	}

	auto result = std::make_shared<std::pair<Error, std::string>>(ERR_BUG, "");
	bool finished = run_with_timeout([result] {
		try {
			winrt::hresult_error activation_error;
			auto statics = winrt::try_get_activation_factory<midi2::MidiApi, midi2::IMidiApiStatics>(activation_error);
			if (!statics) {
				*result = { ERR_UNAVAILABLE, "Virtual MIDI ports on Windows need the Windows MIDI Services API (Windows.Devices.Midi2), which this PC doesn't have yet. Windows 11 25H2 and later get it through Windows Update." };
				return;
			}
			// Also starts the service if it isn't running.
			if (!statics.EnsureServiceAvailable()) {
				if (statics.GetCurrentlySelectedApiMode() == midi2::MidiApiMode::LegacyMode) {
					*result = { ERR_UNAVAILABLE, "Virtual MIDI ports need Windows MIDI Services, but Windows is set to its legacy MIDI stack (see https://microsoft.github.io/MIDI/kb/how-to-change-api-mode/)." };
				} else {
					*result = { ERR_UNAVAILABLE, "Virtual MIDI ports need Windows MIDI Services, but the MIDI service is turned off or couldn't start." };
				}
				return;
			}
			if (statics.GetCurrentlySelectedApiMode() != midi2::MidiApiMode::FullWindowsMidiServicesMode) {
				*result = { ERR_UNAVAILABLE, "Virtual MIDI ports need Windows MIDI Services, but Windows is set to its hybrid legacy MIDI stack (see https://microsoft.github.io/MIDI/kb/how-to-change-api-mode/)." };
				return;
			}
			if (!midi2virtual::MidiVirtualDeviceManager::IsTransportAvailable()) {
				*result = { ERR_UNAVAILABLE, "Windows MIDI Services is running, but its virtual device transport isn't available." };
				return;
			}
			*result = { OK, "" };
		} catch (const winrt::hresult_error &e) {
			*result = { ERR_UNAVAILABLE, "Windows MIDI Services couldn't be used: " + winrt_error_text(e) };
		} catch (const std::exception &e) {
			*result = { ERR_UNAVAILABLE, std::string("Windows MIDI Services couldn't be used: ") + e.what() };
		}
	},
			SERVICE_TIMEOUT);

	if (!finished) {
		error_message = "Windows MIDI Services didn't answer within 20 seconds. Restarting the MIDI service (midisrv) or Windows may help.";
		return ERR_TIMEOUT;
	}
	if (result->first != OK) {
		error_message = String::utf8(result->second.c_str());
		if (result->first == ERR_UNAVAILABLE) {
			error_message += LOOPBACK_HINT;
		}
		return result->first;
	}
	available = true;
	return OK;
}

// Endpoint IDs are derived from the product instance ID, which must be 1-32
// characters from [A-Za-z0-9_-] and unique among live virtual devices.
std::wstring product_instance_id(const String &name, Direction direction, bool unique) {
	std::wstring suffix = direction == Direction::INPUT ? L"-in" : L"-out";
	if (unique) {
		suffix += L"-" + std::to_wstring(::GetCurrentProcessId());
	}
	std::wstring base;
	for (int64_t i = 0; i < name.length(); i++) {
		char32_t c = name[i];
		bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
		base.push_back(allowed ? static_cast<wchar_t>(c) : L'_');
	}
	if (base.empty()) {
		base = L"port";
	}
	const size_t max_base = 32 - suffix.size();
	if (base.size() > max_base) {
		base.resize(max_base);
	}
	return base + suffix;
}

midi2virtual::MidiVirtualDeviceCreationConfig creation_config(const winrt::hstring &name, Direction direction, const std::wstring &instance_id) {
	midi2enum::MidiDeclaredEndpointInfo info;
	info.Name(name);
	info.ProductInstanceId(instance_id);
	info.HasStaticFunctionBlocks(true);
	info.SupportsMidi10Protocol(true);
	info.SupportsMidi20Protocol(false);
	info.SupportsReceivingJitterReductionTimestamps(false);
	info.SupportsSendingJitterReductionTimestamps(false);
	info.SpecificationVersionMajor(1);
	info.SpecificationVersionMinor(1);

	midi2virtual::MidiVirtualDeviceCreationConfig config(name, L"Virtual MIDI port created by a Godot app", L"godot-rtmidi", info);

	// One block on group 0. Its direction is from our side: an input port
	// receives, so other apps see it as a MIDI output, and the other way round.
	midi2enum::MidiFunctionBlock block;
	block.Number(0);
	block.IsActive(true);
	block.Name(name);
	block.FirstGroup(midi2::MidiGroup(static_cast<uint8_t>(0)));
	block.GroupCount(1);
	if (direction == Direction::INPUT) {
		block.Direction(midi2enum::MidiFunctionBlockDirection::BlockInput);
		block.UIHint(midi2enum::MidiFunctionBlockUIHint::Receiver);
	} else {
		block.Direction(midi2enum::MidiFunctionBlockDirection::BlockOutput);
		block.UIHint(midi2enum::MidiFunctionBlockUIHint::Sender);
	}
	block.RepresentsMidi10Connection(midi2enum::MidiFunctionBlockRepresentsMidi10Connection::YesBandwidthUnrestricted);
	config.FunctionBlocks().Append(block);
	return config;
}

void on_message_received(const std::shared_ptr<PortState> &state, const midi2::MidiMessageReceivedEventArgs &args) {
	uint32_t words[4] = {};
	const uint8_t word_count = args.FillWords(words[0], words[1], words[2], words[3]);
	const uint64_t timestamp = args.Timestamp();

	std::lock_guard<std::mutex> lock(state->callback_mutex);
	if (state->closed || !state->callback) {
		return;
	}
	if (!state->decoder.feed(words, word_count, state->message)) {
		return;
	}
	double deltatime = 0.0;
	if (state->has_last_timestamp && timestamp >= state->last_timestamp) {
		deltatime = static_cast<double>(timestamp - state->last_timestamp) / state->timestamp_frequency;
	}
	state->last_timestamp = timestamp;
	state->has_last_timestamp = true;
	state->callback(deltatime, state->message, state->user_data);
}

// Releases everything the state holds. Runs on a helper thread.
void teardown(const std::shared_ptr<PortState> &state) {
	try {
		if (state->connection) {
			if (state->message_token) {
				state->connection.MessageReceived(state->message_token);
			}
			if (state->session) {
				state->session.DisconnectEndpointConnection(state->connection.ConnectionId());
			}
		}
		if (state->session) {
			state->session.Close();
		}
	} catch (...) {
	}
	state->connection = nullptr;
	state->session = nullptr;
	state->device = nullptr;
}

} // namespace

class VirtualPort {
public:
	std::shared_ptr<PortState> state;
};

VirtualPort *open_virtual_port(Direction direction, const String &name, MessageCallback callback, void *user_data, Error &error, String &error_message) {
	ensure_mta();

	error = check_availability(error_message);
	if (error != OK) {
		return nullptr;
	}

	auto state = std::make_shared<PortState>();
	state->direction = direction;
	state->callback = callback;
	state->user_data = user_data;

	auto result = std::make_shared<std::pair<Error, std::string>>(ERR_BUG, "");
	const winrt::hstring hname = winrt::to_hstring(std::string(name.utf8().get_data()));
	const std::wstring instance_id = product_instance_id(name, direction, false);
	const std::wstring unique_instance_id = product_instance_id(name, direction, true);

	bool finished = run_with_timeout([state, result, hname, direction, instance_id, unique_instance_id] {
		try {
			state->timestamp_frequency = static_cast<double>(midi2::MidiClock::TimestampFrequency());
			state->session = midi2::MidiSession::Create(hname);
			if (!state->session) {
				*result = { ERR_CANT_OPEN, "couldn't create a Windows MIDI Services session." };
				return;
			}

			// Right after the service starts, the first creation can return
			// null; the next one works. A null can also mean the ID is taken
			// (another app instance with the same port name), so the last try
			// uses an ID unique to this process.
			const std::wstring *ids[] = { &instance_id, &instance_id, &unique_instance_id };
			for (const std::wstring *id : ids) {
				state->device = midi2virtual::MidiVirtualDeviceManager::CreateVirtualDevice(creation_config(hname, direction, *id));
				if (state->device) {
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
			if (!state->device) {
				*result = { ERR_CANT_OPEN, "Windows MIDI Services couldn't create the virtual device." };
				return;
			}

			state->connection = state->session.CreateEndpointConnection(state->device.DeviceEndpointDeviceId());
			if (!state->connection) {
				*result = { ERR_CANT_OPEN, "couldn't connect to the virtual device." };
				return;
			}
			if (direction == Direction::INPUT) {
				std::weak_ptr<PortState> weak = state;
				state->message_token = state->connection.MessageReceived([weak](const midi2::IMidiMessageReceivedEventSource &, const midi2::MidiMessageReceivedEventArgs &args) {
					if (auto locked = weak.lock()) {
						on_message_received(locked, args);
					}
				});
			}
			// The virtual device is a message processing plugin; without it the
			// device sends but never receives.
			if (state->connection.AddMessageProcessingPlugin(state->device) != midi2::MidiMessageProcessingPluginAddResult::Succeeded) {
				*result = { ERR_CANT_OPEN, "couldn't attach the virtual device to its connection." };
				return;
			}
			// Opening creates the endpoint (and port) other apps see.
			if (!state->connection.Open()) {
				*result = { ERR_CANT_OPEN, "couldn't open the virtual device's connection." };
				return;
			}
			*result = { OK, "" };
		} catch (const winrt::hresult_error &e) {
			*result = { ERR_CANT_OPEN, winrt_error_text(e) };
		} catch (const std::exception &e) {
			*result = { ERR_CANT_OPEN, e.what() };
		}
		bool abandoned;
		{
			std::lock_guard<std::mutex> lock(state->callback_mutex);
			abandoned = state->closed; // open_virtual_port() timed out and gave up
		}
		if (result->first != OK || abandoned) {
			teardown(state);
		}
	},
			CREATE_TIMEOUT);

	if (!finished) {
		// The helper thread keeps the state alive. Make sure nothing is ever
		// delivered, even if it finishes later.
		{
			std::lock_guard<std::mutex> lock(state->callback_mutex);
			state->closed = true;
		}
		error = ERR_TIMEOUT;
		error_message = "Windows MIDI Services didn't create the virtual port within 10 seconds. Installing the latest Windows updates or restarting the MIDI service (midisrv) may help.";
		return nullptr;
	}
	if (result->first != OK) {
		error = result->first;
		error_message = String("Couldn't create the virtual MIDI port: ") + String::utf8(result->second.c_str());
		return nullptr;
	}
	error = OK;
	VirtualPort *port = new VirtualPort();
	port->state = state;
	return port;
}

void close_virtual_port(VirtualPort *port) {
	if (port == nullptr) {
		return;
	}
	std::shared_ptr<PortState> state = port->state;
	delete port;
	{
		std::lock_guard<std::mutex> lock(state->callback_mutex);
		state->closed = true;
		state->callback = nullptr;
	}
	// Disconnecting hung on Windows builds without the fix for
	// microsoft/MIDI#1047. Don't let that freeze the game.
	if (!run_with_timeout([state] { teardown(state); }, CLOSE_TIMEOUT)) {
		WARN_PRINT("Windows MIDI Services didn't remove the virtual MIDI port within 5 seconds. Installing the latest Windows updates or restarting the MIDI service (midisrv) may help.");
	}
}

Error send_message(VirtualPort *port, const unsigned char *data, size_t size, String &error_message) {
	ERR_FAIL_NULL_V(port, ERR_UNCONFIGURED);
	std::vector<uint32_t> words;
	if (!midi1_to_ump(data, size, words)) {
		error_message = "Not a complete MIDI 1.0 message (Windows MIDI Services virtual ports send one complete message at a time, without running status).";
		return ERR_INVALID_PARAMETER;
	}
	try {
		const midi2::MidiEndpointConnection &connection = port->state->connection;
		// Type 1/2 packets are one word, SysEx7 packets two. Timestamp 0 sends now.
		for (size_t i = 0; i < words.size();) {
			const uint8_t count = (words[i] >> 28) == 0x3 ? 2 : 1;
			const midi2::MidiSendMessageResults sent = count == 2
					? connection.SendSingleMessageWords(0, words[i], words[i + 1])
					: connection.SendSingleMessageWords(0, words[i]);
			if ((static_cast<uint32_t>(sent) & static_cast<uint32_t>(midi2::MidiSendMessageResults::Succeeded)) == 0) {
				char hex[9];
				snprintf(hex, sizeof(hex), "%08X", static_cast<uint32_t>(sent));
				error_message = String("Windows MIDI Services couldn't send the message (result 0x") + hex + ").";
				return FAILED;
			}
			i += count;
		}
	} catch (const winrt::hresult_error &e) {
		error_message = String("Windows MIDI Services couldn't send the message: ") + String::utf8(winrt_error_text(e).c_str());
		return FAILED;
	}
	return OK;
}

} // namespace godot_rtmidi::wms
