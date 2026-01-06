extends Node

@onready var midi_in := $MidiIn
@onready var midi_out := $MidiOut

func _ready() -> void:
	print(midi_in.get_port_names())
	midi_in.open_port(3)
	midi_in.ignore_types(false, false, false)
	print(midi_out.get_port_names())
	midi_out.open_port(3)

func _process(delta):
	pass

func _on_midi_in_midi_message(deltatime, message):
	print(deltatime, message)
	pass # Replace with function body.

func _on_midi_in_midi_error(error_type, error_message):
	print("In Error: ", error_type, ", ", error_message)
	pass # Replace with function body.

func _on_midi_out_midi_error(error_type, error_message):
	print("Out Error: ", error_type, ", ", error_message)
	pass # Replace with function body.
