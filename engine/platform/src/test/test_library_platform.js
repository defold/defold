// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const nativeHeader = fs.readFileSync(path.join(__dirname, "../native/native.h"), "utf8");
const joystickParams = ["PRESENT", "AXES", "BUTTONS", "HATS"].map(name =>
    Number(nativeHeader.match(new RegExp("^#define NATIVE_" + name + "\\s+(0x[0-9A-Fa-f]+)", "m"))[1]));

function loadEnvironment() {
    const listeners = new Map();
    const context = vm.createContext({
        console,
        Module: {},
        MainLoop: { currentFrameNumber: 1 },
        navigator: { getGamepads: () => [] },
        window: { addEventListener() {}, removeEventListener() {} },
        document: {
            fullscreenElement: null,
            addEventListener: (type, callback) => listeners.set(type, callback),
            removeEventListener: type => listeners.delete(type),
            exitFullscreen() {
                this.fullscreenElement = null;
                listeners.get("fullscreenchange")();
            }
        },
        autoAddDeps() {},
        addToLibrary() {},
        stringToNewUTF8: value => value,
        _free() {}
    });
    const libraryPath = path.join(__dirname, "../native/web/library_platform.js");
    // Call JavaScript test callbacks directly in place of Emscripten's WASM function pointers.
    const source = fs.readFileSync(libraryPath, "utf8").replace(
        /\{\{\{\s*makeDynCall\('[^']+', '([^']+)'\)\s*\}\}\}/g, "$1");
    vm.runInContext(source, context, { filename: libraryPath });
    context.DefoldPlatform = context.LibraryDefoldPlatform.$DefoldPlatform;
    context.LibraryDefoldPlatform.dmNativeInitJS();
    context.createFullscreenElement = () => ({
        requestFullscreen() {
            context.document.fullscreenElement = this;
            listeners.get("fullscreenchange")();
        }
    });
    return context;
}

// Verify native capability queries drive browser gamepad input and report disconnection after the GLFW removal.
function testGamepadCapabilitiesAndInput() {
    const context = loadEnvironment();
    const library = context.LibraryDefoldPlatform;
    const gamepads = [{
        id: "Test Controller",
        mapping: "standard",
        axes: [0.25, -0.5],
        buttons: [{ pressed: true }, { pressed: false }, { pressed: true }]
    }];
    const events = [];
    const values = [];
    context.navigator.getGamepads = () => gamepads;
    context.setValue = (address, value) => values[address] = Number(value);
    assert.strictEqual(library.dmNativeSetGamepadCallback((id, connected) => events.push([id, connected])), 1);
    assert.deepStrictEqual(events, [[0, 1]]);
    assert.deepStrictEqual(joystickParams.map(param => library.dmNativeGetJoystickParam(0, param)), [1, 2, 3, 0]);
    assert.deepStrictEqual(joystickParams.map(param => library.dmNativeGetJoystickParam(1, param)), [0, 0, 0, 0]);

    library.dmNativeGetJoystickPos(0, 0, library.dmNativeGetJoystickParam(0, joystickParams[1]));
    library.dmNativeGetJoystickButtons(0, 8, library.dmNativeGetJoystickParam(0, joystickParams[2]));
    assert.deepStrictEqual([values[0], values[4], values[8], values[9], values[10]], [0.25, -0.5, 1, 0, 1]);

    gamepads[0] = null;
    context.DefoldPlatform.onJoystickDisconnected();
    assert.deepStrictEqual(events, [[0, 1], [0, 0]]);
    assert.deepStrictEqual(joystickParams.map(param => library.dmNativeGetJoystickParam(0, param)), [0, 0, 0, 0]);
}

// Verify the bundled loader enters and exits fullscreen through the renamed backend for every supported target.
function testLoaderFullscreenToggle() {
    const loaderPath = path.resolve(__dirname,
        "../../../../com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/bundle/resources/web/dmloader.js");
    // Fullscreen has no template inputs; omit optional sections and fill scalar placeholders for evaluation.
    const loader = fs.readFileSync(loaderPath, "utf8")
        .replace(/\{\{![\s\S]*?\}\}/g, "")
        .replace(/\{\{[#^]([^}]+)\}\}[\s\S]*?\{\{\/\1\}\}/g, "")
        .replace(/\{\{[^}]+\}\}/g, "0");
    for (const target of ["canvas", "container", "explicit"]) {
        const context = loadEnvironment();
        vm.runInContext(loader, context, { filename: loaderPath });
        context.Module.canvas = context.createFullscreenElement();
        if (target !== "canvas")
            context.Module.fullScreenContainer = context.createFullscreenElement();
        const explicit = target === "explicit" ? context.createFullscreenElement() : undefined;
        const expected = explicit || context.Module.fullScreenContainer || context.Module.canvas;

        context.Module.toggleFullscreen(explicit);
        assert.strictEqual(context.document.fullscreenElement, expected);
        assert.strictEqual(context.DefoldPlatform.isFullscreen, true);
        context.Module.toggleFullscreen(explicit);
        assert.strictEqual(context.document.fullscreenElement, null);
        assert.strictEqual(context.DefoldPlatform.isFullscreen, false);
    }
}

function loadPointerEnvironment() {
    const context = loadEnvironment();
    context.Module.canvas = {
        width: 100,
        height: 100,
        getBoundingClientRect: () => ({ left: 0, top: 0, right: 100, bottom: 100 })
    };
    context.Browser = {
        mouseX: 0,
        mouseY: 0,
        calculateMouseEvent(event) {
            this.mouseX = event.clientX;
            this.mouseY = event.clientY;
        },
        getMouseWheelDelta: () => 1
    };
    return context;
}

function touchEvent(context, changedTouches, touches = changedTouches) {
    return { target: context.Module.canvas, changedTouches, touches, preventDefault() {} };
}

function mouseEvent(context, button = 0) {
    return { target: context.Module.canvas, button, clientX: 70, clientY: 80, preventDefault() {} };
}

// Verifies primary-touch emulation tracks its origin, ignores secondary touches, and clears the held marker on end and cancellation.
function testTouchMouseSources() {
    for (const finish of ["onTouchEnd", "onTouchCancel"]) {
        const context = loadPointerEnvironment();
        const library = context.LibraryDefoldPlatform;
        const platform = context.DefoldPlatform;
        const primary = { identifier: 1, clientX: 10, clientY: 20 };
        const secondary = { identifier: 2, clientX: 30, clientY: 40 };

        assert.strictEqual(library.dmNativeIsMouseLeftButtonFromTouch(), false);
        assert.strictEqual(library.dmNativeIsMousePositionFromTouch(), false);
        platform.onTouchStart(touchEvent(context, [primary]));
        assert.strictEqual(library.dmNativeGetMouseButton(0), true);
        assert.strictEqual(library.dmNativeIsMouseLeftButtonFromTouch(), true);
        assert.strictEqual(library.dmNativeIsMousePositionFromTouch(), true);

        platform.onMousemove(mouseEvent(context));
        assert.strictEqual(library.dmNativeIsMouseLeftButtonFromTouch(), true);
        assert.strictEqual(library.dmNativeIsMousePositionFromTouch(), false);
        platform.onTouchStart(touchEvent(context, [secondary], [primary, secondary]));
        platform.onTouchMove(touchEvent(context, [secondary], [primary, secondary]));
        assert.strictEqual(library.dmNativeIsMousePositionFromTouch(), false);

        primary.clientX = 15;
        platform.onTouchMove(touchEvent(context, [primary], [primary, secondary]));
        assert.strictEqual(context.Browser.mouseX, 15);
        assert.strictEqual(library.dmNativeIsMousePositionFromTouch(), true);

        platform[finish](touchEvent(context, [secondary], [primary]));
        assert.strictEqual(library.dmNativeGetMouseButton(0), true);
        assert.strictEqual(library.dmNativeIsMouseLeftButtonFromTouch(), true);
        platform[finish](touchEvent(context, [primary], []));
        assert.strictEqual(library.dmNativeGetMouseButton(0), false);
        assert.strictEqual(library.dmNativeIsMouseLeftButtonFromTouch(), false);

        platform.onMouseButtonDown(mouseEvent(context));
        assert.strictEqual(library.dmNativeGetMouseButton(0), true);
        assert.strictEqual(library.dmNativeIsMouseLeftButtonFromTouch(), false);
        platform.onMouseButtonUp(mouseEvent(context));
        assert.strictEqual(library.dmNativeGetMouseButton(0), false);
    }
}

// Verifies real mouse buttons and movement keep their own origin while a touch-emulated button is held.
function testMixedTouchAndMouseSources() {
    const context = loadPointerEnvironment();
    const library = context.LibraryDefoldPlatform;
    const platform = context.DefoldPlatform;
    const primary = { identifier: 1, clientX: 10, clientY: 20 };
    platform.onTouchStart(touchEvent(context, [primary]));
    platform.mouseButtonFunc = () => {};

    // DOM right and middle buttons are swapped in the native API.
    for (const [domButton, nativeButton] of [[2, 1], [1, 2]]) {
        platform.onMouseButtonDown(mouseEvent(context, domButton));
        assert.strictEqual(library.dmNativeGetMouseButton(nativeButton), true);
        assert.strictEqual(library.dmNativeIsMouseLeftButtonFromTouch(), true);
        assert.strictEqual(library.dmNativeIsMousePositionFromTouch(), false);
        platform.onMouseButtonUp(mouseEvent(context, domButton));
        assert.strictEqual(library.dmNativeGetMouseButton(nativeButton), false);
        assert.strictEqual(library.dmNativeIsMouseLeftButtonFromTouch(), true);
    }

    platform.onTouchMove(touchEvent(context, [primary]));
    platform.onMouseWheel(mouseEvent(context));
    assert.strictEqual(library.dmNativeGetMouseWheel(), 1);
    assert.strictEqual(library.dmNativeIsMouseLeftButtonFromTouch(), true);
    assert.strictEqual(library.dmNativeIsMousePositionFromTouch(), true);

    platform.onMouseButtonDown(mouseEvent(context));
    assert.strictEqual(library.dmNativeIsMouseLeftButtonFromTouch(), false);
    assert.strictEqual(library.dmNativeIsMousePositionFromTouch(), false);
    platform.onTouchMove(touchEvent(context, [primary]));
    assert.strictEqual(library.dmNativeIsMouseLeftButtonFromTouch(), false);
    assert.strictEqual(library.dmNativeIsMousePositionFromTouch(), true);
}

for (const test of [testGamepadCapabilitiesAndInput, testLoaderFullscreenToggle,
                   testTouchMouseSources, testMixedTouchAndMouseSources]) {
    try {
        test();
        process.stdout.write(test.name + " passed\n");
    } catch (error) {
        console.error(test.name, error);
        process.exitCode = 1;
    }
}
