@tool
extends EditorPlugin

# Adds godot_rtmidi.aar (RtMidi's Java helper classes) to Android exports.
# RtMidi's Android backend needs them to open MIDI ports. Android libraries
# (.aar) are only included in Gradle builds.

var export_plugin: AndroidExportPlugin


func _enter_tree() -> void:
	export_plugin = AndroidExportPlugin.new()
	add_export_plugin(export_plugin)


func _exit_tree() -> void:
	remove_export_plugin(export_plugin)
	export_plugin = null


class AndroidExportPlugin extends EditorExportPlugin:
	const MIN_SDK := 29 # Android's MIDI API (AMidi)

	func _get_name() -> String:
		return "godot_rtmidi"

	func _supports_platform(platform: EditorExportPlatform) -> bool:
		return platform is EditorExportPlatformAndroid

	func _get_android_libraries(_platform: EditorExportPlatform, _debug: bool) -> PackedStringArray:
		# Relative to res://addons/.
		return PackedStringArray(["godot_rtmidi/android/godot_rtmidi.aar"])

	func _get_export_option_warning(_platform: EditorExportPlatform, option: String) -> String:
		if option == "gradle_build/use_gradle_build" and not get_option(option):
			return "godot_rtmidi: MIDI ports can only be opened on Android with \"Use Gradle Build\" enabled (listing ports works without it)."
		# Min SDK can only be changed in Gradle builds.
		if option == "gradle_build/min_sdk" and get_option("gradle_build/use_gradle_build"):
			# Empty means Godot's default, which is lower.
			var min_sdk := str(get_option(option))
			if not min_sdk.is_valid_int() or min_sdk.to_int() < MIN_SDK:
				return "godot_rtmidi: Android's MIDI API needs a minimum SDK of at least %d; the Gradle build fails with a lower value." % MIN_SDK
		return ""
