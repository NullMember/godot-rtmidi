package org.godotrtmidi;

import android.app.Activity;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.companion.AssociationRequest;
import android.companion.BluetoothLeDeviceFilter;
import android.companion.CompanionDeviceManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.PackageManager;
import android.media.midi.MidiDevice;
import android.media.midi.MidiManager;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;
import android.util.Log;

import org.godotengine.godot.Godot;
import org.godotengine.godot.plugin.GodotPlugin;

import java.util.ArrayList;
import java.util.List;

/**
 * Android side of godot_rtmidi. Shipped in godot_rtmidi.aar together with
 * RtMidi's Java helper classes.
 *
 * Bluetooth MIDI: Android has no system screen that turns a Bluetooth LE MIDI
 * device into a MIDI port, so the app has to open it with
 * MidiManager.openBluetoothDevice(). showBluetoothDevices() lets the user pick
 * a device with the system's companion device picker (no Bluetooth or location
 * permissions needed), opens it, and keeps it open until the app exits. While
 * open, it shows up in MidiManager.getDevices(), so RtMidi lists its ports.
 *
 * Called from native code (src/android_midi.cpp) through JNI.
 */
// CompanionDeviceManager.Callback.onDeviceFound() and getParcelableExtra(String)
// are deprecated in newer API levels but are what works from API 29 on.
@SuppressWarnings("deprecation")
public class GodotRtMidiPlugin extends GodotPlugin {
	private static final String TAG = "godot_rtmidi";
	// The Bluetooth LE MIDI service, from the BLE MIDI specification.
	private static final ParcelUuid MIDI_SERVICE = ParcelUuid.fromString("03B80E5A-EDE8-4B33-A751-6CE34EC4C700");
	private static final int REQUEST_CODE = 0x7A1D;

	// Godot's Error values.
	private static final int OK = 0;
	private static final int ERR_UNAVAILABLE = 2;
	private static final int ERR_BUSY = 44;

	private static volatile GodotRtMidiPlugin instance;

	private final List<MidiDevice> bluetoothDevices = new ArrayList<>();
	private boolean pickerShowing = false;

	public GodotRtMidiPlugin(Godot godot) {
		super(godot);
		instance = this;
	}

	@Override
	public String getPluginName() {
		return "GodotRtMidi";
	}

	/** Returns a Godot Error value. Called from native code on any thread. */
	public static int showBluetoothDevices() {
		GodotRtMidiPlugin plugin = instance;
		if (plugin == null) {
			Log.e(TAG, "showBluetoothDevices: the plugin is not loaded");
			return ERR_UNAVAILABLE;
		}
		return plugin.show();
	}

	private int show() {
		Activity activity = getActivity();
		if (activity == null) {
			return ERR_UNAVAILABLE;
		}
		PackageManager pm = activity.getPackageManager();
		if (!pm.hasSystemFeature(PackageManager.FEATURE_COMPANION_DEVICE_SETUP) ||
				!pm.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE) ||
				!pm.hasSystemFeature(PackageManager.FEATURE_MIDI)) {
			Log.e(TAG, "showBluetoothDevices: this device doesn't support Bluetooth LE MIDI or the companion device picker");
			return ERR_UNAVAILABLE;
		}
		synchronized (this) {
			if (pickerShowing) {
				return ERR_BUSY;
			}
			pickerShowing = true;
		}

		activity.runOnUiThread(() -> {
			CompanionDeviceManager cdm = (CompanionDeviceManager) activity.getSystemService(Context.COMPANION_DEVICE_SERVICE);
			BluetoothLeDeviceFilter filter = new BluetoothLeDeviceFilter.Builder()
					.setScanFilter(new ScanFilter.Builder().setServiceUuid(MIDI_SERVICE).build())
					.build();
			AssociationRequest request = new AssociationRequest.Builder()
					.addDeviceFilter(filter)
					.setSingleDevice(false)
					.build();
			cdm.associate(request, new CompanionDeviceManager.Callback() {
				@Override
				public void onDeviceFound(IntentSender chooser) {
					try {
						activity.startIntentSenderForResult(chooser, REQUEST_CODE, null, 0, 0, 0);
					} catch (IntentSender.SendIntentException e) {
						Log.e(TAG, "Can't show the Bluetooth MIDI device picker", e);
						pickerClosed();
					}
				}

				@Override
				public void onFailure(CharSequence error) {
					// Also called when no device is found or the user cancels.
					Log.i(TAG, "Bluetooth MIDI device picker: " + error);
					pickerClosed();
				}
			}, new Handler(Looper.getMainLooper()));
		});
		return OK;
	}

	private synchronized void pickerClosed() {
		pickerShowing = false;
	}

	@Override
	public void onMainActivityResult(int requestCode, int resultCode, Intent data) {
		super.onMainActivityResult(requestCode, resultCode, data);
		if (requestCode != REQUEST_CODE) {
			return;
		}
		pickerClosed();
		if (resultCode != Activity.RESULT_OK || data == null) {
			return;
		}
		// A ScanResult for Bluetooth LE filters, a BluetoothDevice otherwise.
		Object extra = data.getParcelableExtra(CompanionDeviceManager.EXTRA_DEVICE);
		BluetoothDevice device = null;
		if (extra instanceof ScanResult) {
			device = ((ScanResult) extra).getDevice();
		} else if (extra instanceof BluetoothDevice) {
			device = (BluetoothDevice) extra;
		}
		if (device == null) {
			Log.e(TAG, "Bluetooth MIDI device picker returned no device");
			return;
		}
		openBluetoothDevice(device);
	}

	private void openBluetoothDevice(BluetoothDevice device) {
		Activity activity = getActivity();
		if (activity == null) {
			return;
		}
		MidiManager midi = (MidiManager) activity.getSystemService(Context.MIDI_SERVICE);
		midi.openBluetoothDevice(device, opened -> {
			if (opened == null) {
				Log.e(TAG, "Can't open Bluetooth MIDI device " + device.getAddress());
				return;
			}
			synchronized (bluetoothDevices) {
				bluetoothDevices.add(opened);
			}
			Log.i(TAG, "Opened Bluetooth MIDI device " + device.getAddress());
		}, new Handler(Looper.getMainLooper()));
	}

	@Override
	public void onMainDestroy() {
		synchronized (bluetoothDevices) {
			for (MidiDevice device : bluetoothDevices) {
				try {
					device.close();
				} catch (Exception e) {
					// Closing anyway.
				}
			}
			bluetoothDevices.clear();
		}
		instance = null;
		super.onMainDestroy();
	}
}
