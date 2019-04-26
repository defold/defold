# Engine
* ## 1.2.152
    * `DEF-2994` - **Added**: iOS simulator platform support.
    * `DEF-2933` - **Fixed**: Luajit 2.1.0-beta3 with arm64 support upgrade.
    * `DEF-3910` - **Fixed**: Added support for `html5.engine_arguments` to add runtime arguments.
* ## 1.2.151
    * `DEF-2249` - **Added**: Added cursor and playback rate control for flipbook animations.
    * `DEF-3418` - **Fixed**: Improved retry logic when loading resources for a game in HTML5
    * `DEF-3873` - **Fixed**: Fixed issue with facebook dialogs not working in non-NE builds.
    * `DEF-3888` - **Fixed**: Crash when exiting the game in the project without display profiles
    * `DEF-3898` - **Fixed**: Local push notifications crash
    * `DEF-3896` - **Fixed**: NE - Added vectormath_aos.h to dmsdk/sdk.h

# Editor
* `DEFEDIT-1599` - **Added**: `F5`, `F6` and `F7` now toggle visibility of the surrounding panes.
* `DEFEDIT-1653` - **Fixed**: Outline panel did not refresh after a drag and drop operation.
* `DEFEDIT-1654` - **Fixed**: Window size and position is now remembered between sessions on macOS.
* `DEFEDIT-1659` - **Changed**: The editor will now use more of the available memory to support editing of larger projects.
* `DEFEDIT-1664` - **Fixed**: Update link from the Welcome dialog now works again.
* `DEFEDIT-1666` - **Fixed**: Occasional error message when closing `.script` file.
* `DEFEDIT-1667` - **Changed**: The editor now detects low-memory conditions and will attempt to free up some memory.
