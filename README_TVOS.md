# tvOS

Right now tvOS is similar to iOS, most of what is written for iOS apply here as well. The main differences being various things have been dropped/deprecated in the SDK. UIDeviceFamily array in Info.plist identifies tvOS by an integer 3 (TARGETED_DEVICE_FAMILY in Xcode Build Settings). tvOS only runs arm64.


## User input

Input on tvOS is done through a gamepad device so mapping a few triggers is required.

* GAMEPAD\_LSTICK\_CLICK for touchpad clicks.
* GAMEPAD\_START for play/pause button.
* GAMEPAD\_LSTICK\_{LEFT,RIGHT,UP_DOWN} for pad direction.

Right now tvOS does not fire any touch events to Defold. Touch position from the touch pad does not behave as on iOS. Touch/drag is reported but when pressing or re-touching the pad, the position resets back to a center position of 960,540. 

Text input is always fullscreen, one line, and disables joystick input for the game while active. An optional title can be set for this view, to clarify the reason/what to enter.


## Data storage

tvOS does not support storing data files reliably (only for caching purposes, it seems), however storing 0.5 MB per app is permitted for settings and alike using NSUserDefaults.

* sys.get_save_file still returns a path based on the input.
* sys.load/save will handle the serialized data as NSData stored in NSUserDefaults using the path as key.


## Misc

While ios-deploy [ios-deploy](https://github.com/phonegap/ios-deploy) works fine for deploying on tvOS, lldb cannot be attached with it.

You can easily modify the ios-deploy source and build a "tvos-deploy", by simply altering a few partial paths referring to "iPhoneOS" resources to "AppleTVOS" (for any calls to copy_xcode_path_for). This will produce a binary capable of attaching lldb on deploy.
