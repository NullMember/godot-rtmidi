extends Node

@onready var midi_in := $MidiIn
@onready var midi_out := $MidiOut

func _ready() -> void:
	print(midi_in.get_port_names())
	print(midi_out.get_port_names())
