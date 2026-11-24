extends Node

# Opens the first MIDI input and output port, prints incoming messages and
# plays a test note on the output at start and whenever Space is pressed.
# MidiIn and MidiOut are child nodes of this scene; their midi_message and
# midi_error signals are connected in example.tscn.
#
# The output is not echoed back from the input on purpose: if both ports are
# the same loopback bus (e.g. the macOS IAC Driver or loopMIDI), every message
# would loop forever.

@onready var midi_in: MidiIn = $MidiIn
@onready var midi_out: MidiOut = $MidiOut


func _ready() -> void:
	print("MIDI inputs: ", MidiIn.get_port_names())
	print("MIDI outputs: ", MidiOut.get_port_names())

	# Receive SysEx, timing clock and active sensing messages too (ignored by default).
	midi_in.ignore_types(false, false, false)

	if MidiIn.get_port_count() > 0:
		var err := midi_in.open_port(0)
		if err != OK:
			print("Could not open MIDI input: ", error_string(err))

	if MidiOut.get_port_count() > 0:
		var err := midi_out.open_port(0)
		if err != OK:
			print("Could not open MIDI output: ", error_string(err))

	play_test_note()


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_SPACE:
		play_test_note()


func play_test_note() -> void:
	if not midi_out.is_port_open():
		return
	midi_out.send_message([0x90, 60, 100]) # Note on: channel 1, middle C, velocity 100
	await get_tree().create_timer(0.5).timeout
	midi_out.send_message([0x80, 60, 0]) # Note off


func _on_midi_in_midi_message(deltatime: float, message: PackedByteArray) -> void:
	print("Received ", message, " (%.3f s since the previous message)" % deltatime)


# Because these handlers are connected, RtMidi errors are no longer printed
# by the extension: reporting them is up to us.
func _on_midi_in_midi_error(error_type: int, error_message: String) -> void:
	push_warning("MIDI input error %d: %s" % [error_type, error_message])


func _on_midi_out_midi_error(error_type: int, error_message: String) -> void:
	push_warning("MIDI output error %d: %s" % [error_type, error_message])


# Polling alternative: set midi_in.use_signals = false (in code or in the
# inspector), then read the queued messages every frame instead:
#
# func _process(_delta: float) -> void:
#     var message := midi_in.get_message()
#     while not message.is_empty():
#         print("Polled ", message)
#         message = midi_in.get_message()
