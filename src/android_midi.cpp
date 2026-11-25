// Android only. See android_midi.h.

#include "android_midi.h"
#include "bluetooth_midi.h"

#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/string.hpp>

#include <dlfcn.h>
#include <jni.h>
#include <mutex>

// Defined in RtMidi.cpp (AMidi backend); called by the listener's onDeviceOpened().
extern "C" JNIEXPORT void JNICALL Java_com_yellowlab_rtmidi_MidiDeviceOpenedListener_midiDeviceOpened(
		JNIEnv *env, jclass clazz, jobject midi_device, jlong target, jboolean is_output);

namespace {

constexpr const char *LISTENER_CLASS = "com.yellowlab.rtmidi.MidiDeviceOpenedListener";
constexpr const char *PLUGIN_CLASS = "org.godotrtmidi.GodotRtMidiPlugin";

JNIEnv *get_env() {
	// JNI_GetCreatedJavaVMs is in libnativehelper, which the NDK only links
	// from API 31; look it up at runtime like RtMidi does.
	typedef jint (*GetCreatedJavaVMs)(JavaVM **, jsize, jsize *);
	auto get_vms = (GetCreatedJavaVMs)dlsym(RTLD_DEFAULT, "JNI_GetCreatedJavaVMs");
	if (!get_vms) {
		void *nativehelper = dlopen("libnativehelper.so", RTLD_NOW);
		if (nativehelper) {
			get_vms = (GetCreatedJavaVMs)dlsym(nativehelper, "JNI_GetCreatedJavaVMs");
		}
	}
	JavaVM *vm = nullptr;
	jsize count = 0;
	if (!get_vms || get_vms(&vm, 1, &count) != JNI_OK || count < 1) {
		return nullptr;
	}
	JNIEnv *env = nullptr;
	if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
		vm->AttachCurrentThreadAsDaemon(&env, nullptr);
	}
	return env;
}

void clear_exception(JNIEnv *env) {
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
	}
}

// Loads one of the .aar's classes through the application's class loader:
// FindClass() only sees system classes on threads that were attached natively.
jclass load_app_class(JNIEnv *env, const char *class_name) {
	jclass activity_thread = env->FindClass("android/app/ActivityThread");
	jmethodID current_application = activity_thread ? env->GetStaticMethodID(activity_thread, "currentApplication", "()Landroid/app/Application;") : nullptr;
	jobject application = current_application ? env->CallStaticObjectMethod(activity_thread, current_application) : nullptr;
	clear_exception(env);
	if (!application) {
		return nullptr;
	}
	jclass context_class = env->FindClass("android/content/Context");
	jmethodID get_class_loader = env->GetMethodID(context_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
	jobject loader = env->CallObjectMethod(application, get_class_loader);
	clear_exception(env);
	if (!loader) {
		return nullptr;
	}
	jclass loader_class = env->FindClass("java/lang/ClassLoader");
	jmethodID load_class = env->GetMethodID(loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
	jstring name = env->NewStringUTF(class_name);
	jobject loaded = env->CallObjectMethod(loader, load_class, name);
	clear_exception(env); // ClassNotFoundException when the .aar isn't in the APK.
	return (jclass)loaded;
}

godot::Error register_listener() {
	JNIEnv *env = get_env();
	if (!env) {
		return godot::ERR_UNAVAILABLE;
	}
	if (env->PushLocalFrame(16) != JNI_OK) {
		clear_exception(env);
		return godot::ERR_OUT_OF_MEMORY;
	}
	godot::Error result = godot::ERR_UNCONFIGURED;
	jclass listener = load_app_class(env, LISTENER_CLASS);
	if (listener) {
		JNINativeMethod method = {
			(char *)"midiDeviceOpened",
			(char *)"(Landroid/media/midi/MidiDevice;JZ)V",
			(void *)&Java_com_yellowlab_rtmidi_MidiDeviceOpenedListener_midiDeviceOpened,
		};
		if (env->RegisterNatives(listener, &method, 1) == JNI_OK) {
			result = godot::OK;
		} else {
			clear_exception(env);
			result = godot::FAILED;
		}
	}
	env->PopLocalFrame(nullptr);
	return result;
}

} // namespace

godot::Error godot_rtmidi::prepare_android_open_port(const char *class_name) {
	static std::mutex mutex;
	static bool registered = false;
	std::lock_guard<std::mutex> lock(mutex);
	if (registered) {
		return godot::OK;
	}
	godot::Error err = register_listener();
	switch (err) {
		case godot::OK:
			registered = true;
			break;
		case godot::ERR_UNCONFIGURED:
			ERR_PRINT(godot::String(class_name) + ": Can't open MIDI ports: the app doesn't include godot_rtmidi's Android library. "
					"Enable the godot_rtmidi plugin (Project Settings > Plugins) and \"Use Gradle Build\" in the Android export preset.");
			break;
		default:
			ERR_PRINT(godot::String(class_name) + ": Can't open MIDI ports: failed to set up the Java side of the Android MIDI API.");
			break;
	}
	return err;
}

// Bluetooth MIDI device picker, implemented by the .aar's Godot plugin
// (android/src/org/godotrtmidi/GodotRtMidiPlugin.java).
godot::Error godot_rtmidi::show_bluetooth_midi_devices() {
	JNIEnv *env = get_env();
	ERR_FAIL_NULL_V_MSG(env, godot::ERR_UNAVAILABLE, "Can't show the Bluetooth MIDI device list: no Java environment.");
	if (env->PushLocalFrame(16) != JNI_OK) {
		clear_exception(env);
		return godot::ERR_OUT_OF_MEMORY;
	}
	jclass plugin = load_app_class(env, PLUGIN_CLASS);
	jmethodID show = plugin ? env->GetStaticMethodID(plugin, "showBluetoothDevices", "()I") : nullptr;
	clear_exception(env);
	godot::Error result = godot::ERR_UNCONFIGURED;
	if (show) {
		result = (godot::Error)env->CallStaticIntMethod(plugin, show);
		clear_exception(env);
	}
	env->PopLocalFrame(nullptr);
	if (result == godot::ERR_UNCONFIGURED) {
		ERR_PRINT("Can't show the Bluetooth MIDI device list: the app doesn't include godot_rtmidi's Android library. "
				"Enable the godot_rtmidi plugin (Project Settings > Plugins) and \"Use Gradle Build\" in the Android export preset.");
	} else if (result == godot::ERR_UNAVAILABLE) {
		ERR_PRINT("Can't show the Bluetooth MIDI device list: this device doesn't support Bluetooth LE MIDI or the companion device picker.");
	}
	return result;
}
