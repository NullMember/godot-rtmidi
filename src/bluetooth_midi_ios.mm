// iOS: presents CoreAudioKit's Bluetooth MIDI device list
// (CABTMIDICentralViewController) on top of Godot's view.

#include "bluetooth_midi.h"

#import <CoreAudioKit/CoreAudioKit.h>
#import <UIKit/UIKit.h>

// Closes the device list when "Done" is tapped. CABTMIDICentralViewController
// has no close button of its own.
@interface GodotRtMidiBluetoothDoneHandler : NSObject
@property(nonatomic, weak) UIViewController *presented;
- (void)done;
@end

@implementation GodotRtMidiBluetoothDoneHandler
- (void)done {
	[self.presented dismissViewControllerAnimated:YES completion:nil];
}
@end

static GodotRtMidiBluetoothDoneHandler *done_handler = nil;

static UIWindow *find_key_window() {
	for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
		if (![scene isKindOfClass:UIWindowScene.class]) {
			continue;
		}
		for (UIWindow *window in ((UIWindowScene *)scene).windows) {
			if (window.isKeyWindow) {
				return window;
			}
		}
	}
	id<UIApplicationDelegate> delegate = UIApplication.sharedApplication.delegate;
	return [delegate respondsToSelector:@selector(window)] ? delegate.window : nil;
}

static UIViewController *find_top_view_controller() {
	UIViewController *controller = find_key_window().rootViewController;
	while (controller.presentedViewController) {
		controller = controller.presentedViewController;
	}
	return controller;
}

static godot::Error present_device_list() {
	UIViewController *top = find_top_view_controller();
	ERR_FAIL_NULL_V_MSG(top, godot::ERR_UNAVAILABLE, "Can't show the Bluetooth MIDI device list: no view controller to present it from.");
	if ([top isKindOfClass:UINavigationController.class] &&
			[((UINavigationController *)top).viewControllers.firstObject isKindOfClass:CABTMIDICentralViewController.class]) {
		return godot::ERR_BUSY; // Already showing.
	}

	CABTMIDICentralViewController *central = [[CABTMIDICentralViewController alloc] init];
	UINavigationController *navigation = [[UINavigationController alloc] initWithRootViewController:central];

	done_handler = [[GodotRtMidiBluetoothDoneHandler alloc] init];
	done_handler.presented = navigation;
	central.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
			initWithBarButtonSystemItem:UIBarButtonSystemItemDone
								 target:done_handler
								 action:@selector(done)];

	[top presentViewController:navigation animated:YES completion:nil];
	return godot::OK;
}

godot::Error godot_rtmidi::show_bluetooth_midi_devices() {
	// Without this key, iOS terminates the app as soon as it uses Bluetooth.
	if (![NSBundle.mainBundle objectForInfoDictionaryKey:@"NSBluetoothAlwaysUsageDescription"]) {
		ERR_FAIL_V_MSG(godot::ERR_UNCONFIGURED,
				"Can't show the Bluetooth MIDI device list: the app's Info.plist has no NSBluetoothAlwaysUsageDescription. "
				"Add it in the iOS export preset under Application > Additional Plist Content.");
	}

	if (NSThread.isMainThread) {
		return present_device_list();
	}
	dispatch_async(dispatch_get_main_queue(), ^{
		present_device_list();
	});
	return godot::OK;
}
