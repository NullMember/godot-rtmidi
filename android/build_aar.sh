#!/bin/bash
# Builds godot_rtmidi.aar, the addon's Android library:
#   - RtMidi's Java helper classes (extern/rtmidi/contrib/java), which RtMidi's
#     Android backend needs inside the APK to open ports;
#   - the godot_rtmidi Godot Android plugin (android/src), for the Bluetooth MIDI
#     device picker.
# The godot_rtmidi editor plugin adds the .aar to Android Gradle builds.
#
# Usage: android/build_aar.sh [output.aar]
# Needs a JDK (javac, jar), curl, and an Android SDK platform (ANDROID_HOME or
# ANDROID_SDK_ROOT; the newest installed platform's android.jar is used).
# Godot's Android library is downloaded from Maven Central to compile against;
# it isn't bundled (every Godot app already contains it).

set -euo pipefail

# Oldest supported Godot version (compatibility_minimum in the .gdextension).
GODOT_LIB_VERSION="4.3.0.stable"

root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$root/demo/addons/godot_rtmidi/android/godot_rtmidi.aar}"
sdk="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}}"

android_jar=$(ls -d "$sdk"/platforms/android-*/android.jar 2>/dev/null | sort -V | tail -1)
if [ -z "$android_jar" ]; then
	echo "No Android SDK platform found in $sdk/platforms" >&2
	exit 1
fi

cache="$root/build-android-aar"
godot_jar="$cache/godot-$GODOT_LIB_VERSION-classes.jar"
if [ ! -f "$godot_jar" ]; then
	mkdir -p "$cache"
	curl -fsSL -o "$cache/godot.aar" \
		"https://repo1.maven.org/maven2/org/godotengine/godot/$GODOT_LIB_VERSION/godot-$GODOT_LIB_VERSION.aar"
	(cd "$cache" && unzip -o -q godot.aar classes.jar && mv classes.jar "$godot_jar" && rm godot.aar)
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/classes" "$work/aar"

# Java 8 bytecode works with every Android Gradle plugin version (lambdas are
# desugared by the app's build).
javac --release 8 -nowarn -Xlint:-options -classpath "$android_jar:$godot_jar" -d "$work/classes" \
	"$root"/extern/rtmidi/contrib/java/*.java \
	$(find "$root/android/src" -name '*.java')
jar cf "$work/aar/classes.jar" -C "$work/classes" .
cp "$root/android/AndroidManifest.xml" "$work/aar/"
touch "$work/aar/R.txt"

mkdir -p "$(dirname "$out")"
out="$(cd "$(dirname "$out")" && pwd)/$(basename "$out")"
rm -f "$out"
(cd "$work/aar" && jar cfM "$out" AndroidManifest.xml classes.jar R.txt)
echo "Built $out using $(basename "$(dirname "$android_jar")") and Godot $GODOT_LIB_VERSION"
