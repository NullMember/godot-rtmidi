# godot-rtmidi

Realtime MIDI IO extension for Godot game engine.
This extension built using (https://github.com/thestk/rtmidi)[rtmidi].

## How to use

godot-rtmidi exposes two classes to godot: `MidiIn` and `MidiOut`. You can use these classes to interact with MIDI devices. Here is an example:

```gd
extends Node

var midi_in = MidiIn.new()
var midi_out = MidiOut.new()

func _ready():
    midi_in.open_port(0)
    midi_in.midi_message.connect(_on_midi_message)

    midi_out.open_port(0)

func _on_midi_message(delta, message):
    print("MIDI message: ", message)
    midi_out.send_message(message)
```

This code will open the first MIDI input and output ports and echo all incoming messages to the output port. If you need to get sysex, realtime or sense messages, you should call  

```
midi_in.ignore_types(sysex: bool, time: bool, sense: bool)
```

method with appropriate arguments.  

If you need to list available ports, you can use `get_port_count()`, `get_port_name(port_number)` and `get_port_names()` methods. These methods are static, so you can call them without creating an instance of `MidiIn` or `MidiOut` classes. For example:

```gd
for i in range(MidiIn.get_port_count()):
    print(MidiIn.get_port_name(i))
```

or

```gd
print(MidiOut.get_port_names())
```

Same MIDI device could have different MidiIn and MidiOut port numbers. For example you can have MidiIn port 0 and MidiOut port 1 for the same device.
  
To send a message, you can use `send_message(message: PackedByteArray)` method. For example:

```gd
var message = PackedByteArray()
message.append(0x90) # Note on
message.append(60) # Note number
message.append(127) # Velocity
midi_out.send_message(message)
```

or

```gd
midi_out.send_message([0x90, 60, 127])
```

## API

### MidiIn

#### Methods

- `open_port(port: int) -> void`: Open a MIDI input port.
- `open_virtual_port(port_name: String) -> void`: Open a virtual MIDI input port.
- `close_port() -> void`: Close the currently open MIDI input port.
- `is_port_open() -> bool`: Check if a MIDI input port is open.
- `ignore_types(sysex: bool, time: bool, sense: bool) -> void`: Ignore sysex, time and sense messages.

- `static get_port_count() -> int`: Get the number of available MIDI input ports.
- `static get_port_name(port: int) -> String`: Get the name of a MIDI input port.
- `static get_port_names() -> PackedStringArray`: Get the names of all available MIDI input ports.

#### Signals

- `midi_message(delta: float, message: PackedByteArray)`: Emitted when a MIDI message is received.

### MidiOut

#### Methods

- `open_port(port: int) -> void`: Open a MIDI output port.
- `open_virtual_port(port_name: String) -> void`: Open a virtual MIDI output port.
- `close_port() -> void`: Close the currently open MIDI output port.
- `is_port_open() -> bool`: Check if a MIDI output port is open.
- `send_message(message: PackedByteArray) -> void`: Send a MIDI message.

- `static get_port_count() -> int`: Get the number of available MIDI output ports.
- `static get_port_name(port: int) -> String`: Get the name of a MIDI output port.
- `static get_port_names() -> PackedStringArray`: Get the names of all available MIDI output ports.

## Installation

Latest release can be downloaded from [releases](https://github.com/NullMember/godot-rtmidi/releases) page. Download the zip file and extract it to your project's `addons` directory.

### MacOS Notes

Because of MacOS binary not signed, you need to right click on the dylib file in lib/Darwin-universal and click open. Then you will see a dialog box that says the file is from an unidentified developer. Click open and the next time you run the project it will work.

## Known issues

- Adding MidiIn node directly to scene and connecting to midi_message signal will cause error. This is probably a threading error either in Godot or this extension. To avoid this, add MidiIn node to scene from code and connect to signal from code like example above. Thanks to @CsRic for reporting this [issue](https://github.com/NullMember/godot-rtmidi/issues/1).
