## Defold Input system

### dmHID

This library collects the information from the "Human Interface Devices".
It may be a keyboard, mouse or a gamepad.

Each platform has its own implementation of how the interaction is done with the hardware.
See the hid library for further details.

### dmInput

The input library uses the `dmHID` as the abstraction for the input.

This library uses a `.gamepads` file for translating from the raw hardware gamepad information to how the Defold engine expects the data to be. E.g. which hardware button corresponds to which engine enum.

For a gamepad axis, the engine wants the final range to be in [-1,1].
In order to get this behavior, the translation file may use scaling, negating and clamping to convert from hardware to engine values. The translation also supports deadzone values.

It is this library that handles the mappings between the HID input events to user defined actions ([Input Bindings](https://defold.com/manuals/input/#setting-up-input-bindings)).
In the input library, these mappings are called "triggers".

If a trigger is said to be "active", it gets sent to a callback that the `engine.cpp` has registered with the input library.

### The code flow

The `engine.cpp` gets callbacks from the `dmInput` system for each input event.
It then loops over the [Input Stack](https://defold.com/manuals/input/#input-focus), i.e. the game objects that are registered to receive input events.

For each game object's components, the engine checks if the comopnent has an "on input" function.
If so, it gets called with the input event.

Two such components are `comp_script.cpp` and `comp_gui.cpp` (which calls `gui.cpp`). They each work essentially the same way.
In the input function, the component creates the Lua data to be sent to the components Lua `on_input` life cycle function.



