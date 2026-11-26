# godot-rtmidi

Realtime MIDI input and output for the Godot game engine, as a GDExtension built on [RtMidi](https://github.com/thestk/rtmidi).

It adds two nodes, `MidiIn` and `MidiOut`, for receiving and sending MIDI messages through hardware and software MIDI ports. The full class reference is available in Godot's built-in help (search for `MidiIn` or `MidiOut`).

## Supported platforms

Requires Godot 4.3 or newer.

| Platform | Architectures | Requirements | Virtual ports |
|---|---|---|---|
| Windows | x86_64, x86_32 | — | Windows 11 25H2+ with Windows MIDI Services, x86_64 only ([details](#virtual-ports)) |
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

`open_virtual_port(name)` creates a port that other applications can connect to. This works on macOS, iOS, Linux and Windows. On Android it returns `ERR_UNAVAILABLE`.

On iOS, virtual ports need the `audio` background mode (see [iOS export settings](#ios-export-settings)), or `open_virtual_port()` returns `ERR_CANT_OPEN`.

On Windows, virtual ports use [Windows MIDI Services](https://microsoft.github.io/MIDI/), Microsoft's new MIDI system:

- They need Windows 11 25H2 or later with the Windows MIDI Services API (`Windows.Devices.Midi2`), which Windows Update installs. Nothing has to be shipped with your game, but until Windows includes the API you can [ship Microsoft's preview copy](#shipping-the-windows-midi-services-api-with-your-game). Where it's missing (older Windows, a PC without the update, Windows set to its legacy MIDI stack, or the 32-bit build), `open_virtual_port()` returns `ERR_UNAVAILABLE`. As a fallback, create a virtual port with [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) and open it with `open_port()`.
- Other applications see the port under its name, including older ones that use the classic Windows MIDI API (which shows at most 31 characters). A `MidiIn` virtual port shows up there as an output, a `MidiOut` one as an input.
- The first call can take a few seconds while Windows starts its MIDI service.
- `send_message()` takes one complete message per call (no running status).

#### Shipping the Windows MIDI Services API with your game

Until Windows includes the API, you can ship Microsoft's preview copy of it with your game. godot-rtmidi doesn't include it. Microsoft allows this under the "go live" terms in the [Preview 11 release notes](https://github.com/microsoft/MIDI/releases/tag/inbox-preview-11). By shipping it, you agree to update your game to newer previews and remove the files once Windows includes the API, to tell your players that the game uses a preview Windows MIDI Services API, not to ship it to Windows versions older than Windows 11 25H2, and to report problems on Microsoft's GitHub. On a PC whose Windows has its own copy, Windows uses that one instead of yours.

1. Download `Windows.Devices.Midi2.0.99.92-preview.11.nupkg` from the [Preview 11 release](https://github.com/microsoft/MIDI/releases/tag/inbox-preview-11) and unpack it with Windows' own `tar` (in cmd or PowerShell; Git Bash's `tar` can't read it):

   ```
   mkdir wms
   tar -xf Windows.Devices.Midi2.0.99.92-preview.11.nupkg -C wms
   ```

   You need two files from `wms/runtimes/win-x64/native/`: `Windows.Devices.Midi2.dll` and `Windows.Devices.Midi2.pri`. Always copy both.

2. To use virtual ports while running your game from the editor, copy both files next to the Godot editor executable (`Godot_v4.x-stable_win64.exe`). Windows looks for them next to the program that's running, not in your project.

3. For exported games, copy both files into your project, for example to `res://addons/godot_rtmidi/windows/wms/`, and add this to the end of `res://addons/godot_rtmidi/godot_rtmidi.gdextension`:

   ```ini
   [dependencies]

   windows.x86_64 = {
       "res://addons/godot_rtmidi/windows/wms/Windows.Devices.Midi2.dll": "",
       "res://addons/godot_rtmidi/windows/wms/Windows.Devices.Midi2.pri": ""
   }
   ```

   Godot then puts both files next to the exported `.exe`. Updating godot-rtmidi replaces `godot_rtmidi.gdextension`, so add the section again after an update. You can also copy the two files next to the exported `.exe` yourself instead.

Your players need Windows 11 25H2 or later. This works with the 64-bit (x86_64) build only.

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

On Windows: Visual Studio 2022+ (MSVC) with a Windows SDK 10.0.22000 or newer, and the Windows MIDI Services API metadata for virtual ports. Download `Windows.Devices.Midi2.0.99.92-preview.11.nupkg` from [Microsoft's release page](https://github.com/microsoft/MIDI/releases/tag/inbox-preview-11), unpack it with Windows' own `tar` (`tar -xf Windows.Devices.Midi2.0.99.92-preview.11.nupkg -C wms`) and add `-DWindowsMidiServices_ROOT=<path>/wms` to the `cmake -S` command. Only its `ref/native/Windows.Devices.Midi2.winmd` is used; nothing from it is shipped.

```sh
git clone --recursive https://github.com/NullMember/godot-rtmidi.git
cd godot-rtmidi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGODOTCPP_TARGET=template_debug
cmake --build build
```

The library is copied into `demo/addons/godot_rtmidi/<platform>/`. Build with `-DGODOTCPP_TARGET=template_release` for release exports. See `.github/workflows/build.yml` for the exact commands used for every platform, including iOS and Android.

Build options:

- `-DGODOT_RTMIDI_WMS=OFF` (Windows): builds without Windows MIDI Services, so the API metadata isn't needed. `open_virtual_port()` then always returns `ERR_UNAVAILABLE` on Windows.
- `-DGODOT_RTMIDI_JACK=ON` (Linux): adds the JACK backend. The library then requires `libjack` at runtime, so the official builds don't include it. Requires the JACK development files.
- `build_profile.json` limits the generated Godot bindings to the classes this extension uses, which keeps builds fast. If you change the code to use another Godot class, add it there.

## Known issues

- On macOS, virtual ports with non-ASCII names (e.g. `"Tést"`) show up garbled in other applications. This is a bug in RtMidi; a fix is being contributed upstream.
- On Android, the `deltatime` of received messages is always `0`. This is a bug in RtMidi.
- On Windows builds from before the November 2026 update (KB5132057), Windows MIDI Services can hang while creating or removing a virtual port, especially after an app using one was force-closed ([microsoft/MIDI#1047](https://github.com/microsoft/MIDI/issues/1047)). `open_virtual_port()` then returns `ERR_TIMEOUT` instead of freezing the game. Install the latest Windows updates; until Windows has fully rolled out the fix (up to 30 days after the update), restarting the PC or the MIDI service (`midisrv`) clears it.

## License

godot-rtmidi is released under the [MIT License](LICENSE.md). The release zip also includes the licenses of [RtMidi](https://github.com/thestk/rtmidi) and [godot-cpp](https://github.com/godotengine/godot-cpp), which are linked into the library.
