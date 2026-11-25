# godot-rtmidi

Realtime MIDI input and output for the Godot game engine, as a GDExtension built on [RtMidi](https://github.com/thestk/rtmidi).

It adds two nodes, `MidiIn` and `MidiOut`, for receiving and sending MIDI messages through hardware and software MIDI ports. The full class reference is available in Godot's built-in help (search for `MidiIn` or `MidiOut`).

## Supported platforms

Requires Godot 4.3 or newer.

| Platform | Architectures | Requirements | Virtual ports |
|---|---|---|---|
| Windows | x86_64, x86_32 | — | No (use [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html)) |
| macOS | universal (x86_64, arm64) | macOS 10.13+ (Apple Silicon: 11.0+) | Yes |
| Linux | x86_64, arm64 | glibc 2.34+ (e.g. Ubuntu 22.04, Debian 12) and ALSA (`libasound.so.2`) | Yes |
| iOS | arm64 (device and simulator) | iOS 12+ | Yes |
| Android | arm64, x86_64 | Android 10+ (API level 29) | No |

Android needs a few export settings, see [Android export settings](#android-export-settings). The web platform is not supported.

## Installation

Download the latest zip from the [releases](https://github.com/NullMember/godot-rtmidi/releases) page and extract it into your project's root folder, so you get `res://addons/godot_rtmidi/`.

When updating from an older version, replace the whole `addons/godot_rtmidi/` folder, including `godot_rtmidi.gdextension`.

## Usage

### Receiving messages

```gdscript
extends Node

var midi_in := MidiIn.new()

func _ready():
    add_child(midi_in)
    midi_in.midi_message.connect(_on_midi_message)

    var err := midi_in.open_port(0)
    if err != OK:
        print("Could not open MIDI input: ", error_string(err))

func _on_midi_message(deltatime: float, message: PackedByteArray):
    print("MIDI message: ", message, " (", deltatime, " s since the previous one)")
```

`MidiIn` and `MidiOut` are nodes, so you can also add them to a scene in the editor and connect their signals there. The `demo/` folder contains a complete example project.

By default, SysEx, timing clock and active sensing messages are ignored. To receive them, call:

```gdscript
midi_in.ignore_types(false, false, false) # sysex, time, sense
```

### Polling instead of signals

Set `use_signals` to `false` (in code or in the inspector) and read queued messages yourself, for example every frame:

```gdscript
func _ready():
    midi_in.use_signals = false
    midi_in.open_port(0)

func _process(_delta):
    var message := midi_in.get_message()
    while not message.is_empty():
        print("MIDI message: ", message)
        message = midi_in.get_message()
```

### Sending messages

```gdscript
var midi_out := MidiOut.new()

func _ready():
    midi_out.open_port(0)
    midi_out.send_message([0x90, 60, 100]) # Note on: channel 1, middle C, velocity 100
    midi_out.send_message([0x80, 60, 0])   # Note off
```

### Listing ports

The port methods are static, so you can call them without creating an instance:

```gdscript
print(MidiIn.get_port_names())
for i in MidiOut.get_port_count():
    print(i, ": ", MidiOut.get_port_name(i))
```

The same device can have different input and output port numbers.

On Android, an output port can only be open in one `MidiOut` at a time (also across apps). To reopen it, call `close_port()` on the old `MidiOut` first; `queue_free()` only closes it at the end of the frame.

### Virtual ports

`open_virtual_port(name)` creates a port that other applications can connect to. This works on macOS, iOS and Linux. On Windows and Android it returns `ERR_UNAVAILABLE`; on Windows, create a virtual port with [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) and open it with `open_port()` instead.

On iOS, virtual ports need the `audio` background mode (see [iOS export settings](#ios-export-settings)), or `open_virtual_port()` returns `ERR_CANT_OPEN`.

### Bluetooth MIDI

Bluetooth MIDI devices show up as regular ports once they're connected to the system. On macOS, connect them in Audio MIDI Setup. On iOS and Android, an app has to connect them itself:

```gdscript
func _on_bluetooth_button_pressed():
    MidiIn.show_bluetooth_devices() # Also available as MidiOut.show_bluetooth_devices()
```

- **iOS** shows the system's Bluetooth MIDI device list. The user connects a device and taps **Done**; then check `get_port_names()` again. The connection is system-wide, so a device connected from another app (e.g. GarageBand) works too. This needs a Bluetooth usage description in the app (see [iOS export settings](#ios-export-settings)); without it the method returns `ERR_UNCONFIGURED`.
- **Android** shows the system's device picker, filtered to Bluetooth MIDI devices (no permissions needed). The chosen device stays connected until the app exits and its ports appear shortly after. This needs the [Android export settings](#android-export-settings). Computers that support both classic Bluetooth and Bluetooth LE, such as a Mac sharing MIDI over Bluetooth, may disconnect right away: Android connects to them over classic Bluetooth, where MIDI isn't available.
- Other platforms return `ERR_UNAVAILABLE`.

### iOS export settings

Some features need extra entries in the app's Info.plist. In the iOS export preset, add them to **Application → Additional Plist Content**:

```xml
<!-- Virtual ports (open_virtual_port) -->
<key>UIBackgroundModes</key><array><string>audio</string></array>
<!-- Bluetooth MIDI device list (show_bluetooth_devices) -->
<key>NSBluetoothAlwaysUsageDescription</key><string>Connects to Bluetooth MIDI devices.</string>
```

Network MIDI sessions and USB MIDI devices need nothing extra.

### Android export settings

RtMidi needs a few Java classes inside the app to open MIDI ports on Android, and the Bluetooth MIDI device picker needs a small Godot Android plugin. The addon ships both as an Android library (`android/godot_rtmidi.aar`), which its editor plugin adds to Android exports:

1. Enable the **godot_rtmidi** plugin in **Project → Project Settings → Plugins**.
2. Install the Android build template (**Project → Install Android Build Template**).
3. In the Android export preset, enable **Gradle Build → Use Gradle Build** and set **Gradle Build → Min SDK** to `29` or higher (Android's MIDI API needs Android 10).

Without these, listing ports still works, but `open_port()` returns `ERR_UNCONFIGURED` with an error explaining what's missing. The plugin also shows warnings in the export dialog when a setting is wrong.

### Error handling

`open_port()`, `open_virtual_port()`, `close_port()` and `send_message()` return a Godot `Error` (`OK` on success). See each method in the built-in help for the possible values.

Errors reported by the MIDI system are emitted through the `midi_error(error_type, error_message)` signal, where `error_type` is one of the `MidiError` constants. If nothing is connected to `midi_error`, they are printed to the console instead.

```gdscript
midi_in.midi_error.connect(func(error_type, error_message):
    push_warning("MIDI error %d: %s" % [error_type, error_message]))
```

If the MIDI system can't be initialized at all (for example when ALSA is unavailable inside a container), the nodes print an error and every method fails gracefully instead of crashing Godot.

## Building from source

Requirements: CMake 3.24+, Python 3, a C++17 compiler, and on Linux the ALSA development files (`libasound2-dev` or `alsa-lib-devel`). For Android, also the NDK, and JDK 17+ with an Android SDK platform for `android/build_aar.sh`, which builds `godot_rtmidi.aar`.

```sh
git clone --recursive https://github.com/NullMember/godot-rtmidi.git
cd godot-rtmidi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build
```

The library is copied into `demo/addons/godot_rtmidi/<platform>/`. Build with `-DGODOTCPP_TARGET=template_release` for release exports. See `.github/workflows/build.yml` for the exact commands used for every platform, including iOS and Android.

Build options:

- `-DGODOT_RTMIDI_JACK=ON` (Linux): adds the JACK backend. The library then requires `libjack` at runtime, so the official builds don't include it. Requires the JACK development files.
- `build_profile.json` limits the generated Godot bindings to the classes this extension uses, which keeps builds fast. If you change the code to use another Godot class, add it there.

## Known issues

- On macOS, virtual ports with non-ASCII names (e.g. `"Tést"`) show up garbled in other applications. This is a bug in RtMidi; a fix is being contributed upstream.
- On Android, the `deltatime` of received messages is always `0`. This is a bug in RtMidi.

## License

godot-rtmidi is released under the [MIT License](LICENSE.md). The release zip also includes the licenses of [RtMidi](https://github.com/thestk/rtmidi) and [godot-cpp](https://github.com/godotengine/godot-cpp), which are linked into the library.
