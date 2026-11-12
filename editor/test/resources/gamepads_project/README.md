This small project is the fixture for the editor's gamepad build tests.

To check input manually, open `game.project` and build and run it. The built-in
debug text renderer displays the latest keyboard or gamepad action, its pressed
or released state, and its value. Use the arrow keys, Space, Enter, or gamepad
buttons and sticks. No custom graphics or fonts are needed.

The integration tests add their own gamepad mappings and SDL controller database
to a scratch copy of this project, then check compilation and rebuilds.
