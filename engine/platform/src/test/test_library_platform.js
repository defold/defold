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

for (const test of [testGamepadCapabilitiesAndInput, testLoaderFullscreenToggle]) {
    try {
        test();
        process.stdout.write(test.name + " passed\n");
    } catch (error) {
        console.error(test.name, error);
        process.exitCode = 1;
    }
}
