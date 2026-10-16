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

function loadLoaderEnvironment(hasWebGPU = false) {
    const loaderPath = path.resolve(__dirname,
        "../../../../com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/bundle/resources/web/dmloader.js");
    const flags = { "html5.verify_downloaded_file_size": true, DEFOLD_HAS_WASM_ENGINE: true,
                    DEFOLD_HAS_WEBGPU: hasWebGPU };
    const loader = fs.readFileSync(loaderPath, "utf8")
        .replace(/\{\{![\s\S]*?\}\}/g, "")
        .replace(/\{\{([#^])([^}]+)\}\}([\s\S]*?)\{\{\/\2\}\}/g,
            (_, kind, name, content) => (kind === "#" ? !!flags[name] : !flags[name]) ? content : "")
        .replace(/\{\{[^}]+\}\}/g, "0");
    const context = loadEnvironment();
    vm.runInContext(loader, context, { filename: loaderPath });
    return context;
}

function setupLoaderStartup(hasWebGPU, hasWebGL) {
    const context = loadLoaderEnvironment(hasWebGPU);
    const calls = { started: 0, unsupported: 0, probes: 0 };
    context.Module.setupCanvas = () => {
        context.Module.canvas = { focus() {}, addEventListener() {} };
    };
    context.Module.hasWebGLSupport = () => hasWebGL;
    context.Module._preloadAndCallMain = () => ++calls.started;
    context.CUSTOM_PARAMETERS.full_screen_container = null;
    context.CUSTOM_PARAMETERS.unsupported_webgl_callback = () => ++calls.unsupported;
    context.document.createElement = () => ({ getContext: () => ({}) });
    context.window.top = context.window;
    context.document.hasFocus = () => true;
    return { context, calls };
}

// Verify WebGL-only builds start synchronously without waiting for or querying a WebGPU adapter.
function testLoaderWebGLSkipsWebGPUProbe() {
    for (const hasWebGL of [true, false]) {
        const { context, calls } = setupLoaderStartup(false, hasWebGL);
        context.navigator.gpu = { requestAdapter() {
            ++calls.probes;
            return new Promise(() => {});
        } };

        context.Module.runApp();
        assert.strictEqual(calls.probes, 0);
        assert.strictEqual(calls.started, hasWebGL ? 1 : 0);
        assert.strictEqual(calls.unsupported, hasWebGL ? 0 : 1);
        assert.strictEqual(context.Module.probeWebGPUSupport, undefined);
        assert.strictEqual(context.Module.hasWebGPUSupport, undefined);
    }
}

// Verifies startup focuses an idle page without scrolling, but preserves an element already chosen by the user.
function testLoaderStartupFocus() {
    for (const active of ["body", "documentElement", "canvas", "input", "none"]) {
        const { context, calls } = setupLoaderStartup(false, true);
        let focusCalls = 0;
        const canvas = { addEventListener() {}, focus(options) {
            assert.strictEqual(options.preventScroll, true);
            ++focusCalls;
            context.document.activeElement = canvas;
        } };
        context.Module.setupCanvas = () => context.Module.canvas = canvas;
        context.document.body = {};
        context.document.documentElement = {};
        const activeElement = active === "canvas" ? canvas : active === "input" ? {} :
                              active === "none" ? null : context.document[active];
        context.document.activeElement = activeElement;

        context.Module.runApp();
        const shouldFocus = active !== "input" && active !== "canvas";
        assert.strictEqual(focusCalls, shouldFocus ? 1 : 0);
        assert.strictEqual(context.document.activeElement, shouldFocus ? canvas : activeElement);
        assert.strictEqual(calls.started, 1);
    }
}

// Verifies embedded startup only focuses an idle canvas document that already owns focus, preserving host input.
function testLoaderIframeStartupFocus() {
    for (const hasFocus of [true, false]) {
        for (const activeInput of [true, false]) {
            const { context, calls } = setupLoaderStartup(false, true);
            let focusCalls = 0;
            context.Module.setupCanvas = () => context.Module.canvas = {
                addEventListener() {},
                focus() { ++focusCalls; }
            };
            // A cross-origin host cannot be inspected; comparing its window reference is safe.
            context.window.top = new Proxy({}, { get() { assert.fail("Inspected the host window"); } });
            context.document.hasFocus = () => hasFocus;
            context.document.body = {};
            context.document.activeElement = activeInput ? {} : context.document.body;

            context.Module.runApp();
            assert.strictEqual(focusCalls, hasFocus && !activeInput ? 1 : 0);
            assert.strictEqual(calls.started, 1);
        }
    }
}

// Verifies asynchronous engine startup does not steal focus acquired while browser capability detection was pending.
async function testLoaderDelayedStartupPreservesFocus() {
    const { context, calls } = setupLoaderStartup(true, true);
    let resolveProbe;
    let focusCalls = 0;
    let signalStarted;
    const started = new Promise(resolve => signalStarted = resolve);
    context.Module._preloadAndCallMain = () => {
        ++calls.started;
        signalStarted();
    };
    context.navigator.gpu = { requestAdapter: () => new Promise(resolve => resolveProbe = resolve) };
    context.Module.setupCanvas = () => context.Module.canvas = {
        addEventListener() {},
        focus() { ++focusCalls; }
    };
    context.document.body = {};
    context.document.activeElement = context.document.body;
    context.Module.runApp();
    assert.strictEqual(calls.started, 0);

    const input = {};
    context.document.activeElement = input;
    resolveProbe({});
    await started;
    assert.strictEqual(calls.started, 1);
    assert.strictEqual(focusCalls, 0);
    assert.strictEqual(context.document.activeElement, input);
}

// Verify WebGPU builds await adapter discovery and retain WebGL fallback when discovery fails.
async function testLoaderWebGPUProbeGatesStartup() {
    for (const hasWebGL of [true, false]) {
        for (const outcome of ["adapter", "null", "rejected", "throws", "unavailable"]) {
            const { context, calls } = setupLoaderStartup(true, hasWebGL);
            let resolveProbe, rejectProbe;
            context.console = { log() {} };
            if (outcome !== "unavailable") {
                context.navigator.gpu = { requestAdapter() {
                    ++calls.probes;
                    if (outcome === "throws") throw new Error("Adapter unavailable");
                    return new Promise((resolve, reject) => { resolveProbe = resolve; rejectProbe = reject; });
                } };
            }

            context.Module.runApp();
            if (resolveProbe) {
                assert.strictEqual(calls.started, 0);
                assert.strictEqual(calls.unsupported, 0);
                assert.strictEqual(context.Module._isEngineLoaded, false);
                if (outcome === "rejected") rejectProbe(new Error("Adapter unavailable"));
                else resolveProbe(outcome === "adapter" ? {} : null);
                await Promise.resolve();
            }

            const supported = hasWebGL || outcome === "adapter";
            assert.strictEqual(calls.probes, outcome === "unavailable" ? 0 : 1);
            assert.strictEqual(calls.started, supported ? 1 : 0);
            assert.strictEqual(calls.unsupported, supported ? 0 : 1);
            assert.strictEqual(context.Module.hasWebGPUSupport(), outcome === "adapter");
        }
    }
}

// Verify the bundled loader enters and exits fullscreen through the renamed backend for every supported target.
function testLoaderFullscreenToggle() {
    for (const target of ["canvas", "container", "explicit"]) {
        const context = loadLoaderEnvironment();
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

// Verify limited and unlimited downloads reconstruct every file and finish when pieces arrive out of order.
async function testLoaderArchiveConcurrency() {
    for (const limit of [1, 2, 6, undefined]) {
        const context = loadLoaderEnvironment();
        const loader = context.GameArchiveLoader;
        const pending = [];
        const responses = new Map();
        const expectedFiles = [];
        const loadedFiles = [];
        let completed = 0;
        let maxPending = 0;
        loader.MAX_CONCURRENT_XHR = limit;
        loader._files = [8, 3].map((pieceCount, fileIndex) => {
            const file = { name: "file" + fileIndex, size: pieceCount * 2, pieces: [] };
            const data = [];
            for (let index = 0; index < pieceCount; ++index) {
                const name = file.name + "-piece" + index;
                const bytes = Uint8Array.of(fileIndex, index);
                file.pieces.push({ name, offset: index * 2 });
                responses.set("split/" + name, bytes.buffer);
                data.push(...bytes);
            }
            expectedFiles.push({ name: file.name, data });
            return file;
        });
        loader.addFileLoadedListener(file => loadedFiles.push({ name: file.name, data: Array.from(file.data) }));
        loader.addArchiveLoadedListener(() => ++completed);
        context.FileLoader.load = (url, responseType, onprogress, onerror, onload) => {
            assert.strictEqual(responseType, "arraybuffer");
            pending.push(() => onload(responses.get(url(0))));
            maxPending = Math.max(maxPending, pending.length);
        };

        await loader.downloadContent();
        while (pending.length > 0) {
            pending.pop()();
            // Let completed file verification enqueue the next file's downloads.
            await Promise.resolve();
        }

        assert.strictEqual(completed, 1, "archive did not complete with concurrency " + limit);
        assert.deepStrictEqual(loadedFiles, expectedFiles);
        assert.strictEqual(maxPending, limit === undefined ? 8 : limit);
    }
}

// Verify preloading transfers archive buffers to the filesystem, releases the queue, and remains idempotent.
function testLoaderArchiveBufferOwnership() {
    const context = loadLoaderEnvironment();
    const files = new Map();
    const archives = [
        { name: "game.arci", data: Uint8Array.of(1, 2, 3) },
        { name: "game.arcd", data: new Uint8Array(4096).fill(7) }
    ];
    let preloadedFiles = 0;
    context.FS = {
        createPreloadedFile(parent, name, data, canRead, canWrite, onload, onerror, dontCreateFile, canOwn) {
            files.set(name, canOwn ? data.subarray(0) : data.slice());
            ++preloadedFiles;
        }
    };
    for (const archive of archives) {
        context.Module.onArchiveFileLoaded(archive);
    }

    context.Module.preloadAll();
    assert.strictEqual(context.Module._filesToPreload.length, 0);
    for (const archive of archives) {
        const contents = files.get(archive.name);
        assert.deepStrictEqual(contents, archive.data);
        assert.strictEqual(contents.buffer, archive.data.buffer);
    }

    context.Module.preloadAll();
    assert.strictEqual(preloadedFiles, archives.length);
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

// Verifies focus is requested only for deliberate canvas presses/taps, never by queries, scrolling, cancellation, or synthetic input.
function testCanvasFocusRequests() {
    const context = loadPointerEnvironment();
    const platform = context.DefoldPlatform;
    const canvas = context.Module.canvas;
    const other = { focus() { assert.fail("Engine focused an unrelated page element"); } };
    let focusCalls = 0;
    canvas.focus = options => {
        assert.strictEqual(options.preventScroll, true);
        ++focusCalls;
        context.document.activeElement = canvas;
    };
    context.document.activeElement = other;

    assert.strictEqual(platform.isCanvasActive({ target: canvas }), true);
    assert.strictEqual(focusCalls, 0);
    platform.onMouseWheel({ ...mouseEvent(context), type: "mousewheel", isTrusted: true, cancelable: true });
    platform.onMouseButtonUp({ ...mouseEvent(context), type: "mouseup", isTrusted: true });
    for (const handler of ["onTouchMove", "onTouchCancel"]) {
        platform[handler]({ ...touchEvent(context, []), isTrusted: true });
    }
    platform.onMouseButtonDown({ ...mouseEvent(context), type: "mousedown", isTrusted: false });
    platform.onTouchStart({ ...touchEvent(context, []), type: "touchstart", isTrusted: false });
    platform.onTouchEnd({ ...touchEvent(context, []), type: "touchend", isTrusted: false });
    assert.strictEqual(focusCalls, 0);
    assert.strictEqual(context.document.activeElement, other);

    let prevented = false;
    platform.mouseButtonFunc = () => assert.strictEqual(context.document.activeElement, canvas);
    platform.onMouseButtonDown({ ...mouseEvent(context), type: "mousedown", isTrusted: true,
                                preventDefault() { prevented = true; } });
    assert.strictEqual(focusCalls, 1);
    assert.strictEqual(prevented, true);

    platform.onMouseWheel({ ...mouseEvent(context), target: other, isTrusted: true });
    platform.onKeydown({ target: canvas, keyCode: 65, code: "KeyA", isTrusted: true });
    assert.strictEqual(focusCalls, 1);

    context.document.activeElement = other;
    platform.onTouchStart({ ...touchEvent(context, []), type: "touchstart", isTrusted: true });
    assert.strictEqual(focusCalls, 2);
    assert.strictEqual(context.document.activeElement, canvas);
    platform.onTouchEnd({ ...touchEvent(context, []), type: "touchend", isTrusted: true });
    assert.strictEqual(focusCalls, 2);
    assert.strictEqual(context.document.activeElement, canvas);
}

// Verifies touch input starts immediately and touchend retries focus when the browser required activation.
function testTouchFocusRetry() {
    const context = loadPointerEnvironment();
    const platform = context.DefoldPlatform;
    const canvas = context.Module.canvas;
    const other = {};
    const phases = [];
    const touch = { identifier: 1, clientX: 10, clientY: 20 };
    let allowFocus = false;
    let focusCalls = 0;
    context.document.activeElement = other;
    canvas.focus = options => {
        assert.strictEqual(options.preventScroll, true);
        ++focusCalls;
        if (allowFocus) context.document.activeElement = canvas;
    };
    platform.touchFunc = (id, x, y, phase) => phases.push(phase);

    platform.onTouchStart({ ...touchEvent(context, [touch]), type: "touchstart", isTrusted: true });
    assert.strictEqual(focusCalls, 1);
    assert.strictEqual(context.document.activeElement, other);
    assert.deepStrictEqual(phases, [platform.NATIVE_PHASE_BEGAN]);

    allowFocus = true;
    platform.onTouchEnd({ ...touchEvent(context, [touch], []), type: "touchend", isTrusted: true });
    assert.strictEqual(focusCalls, 2);
    assert.strictEqual(context.document.activeElement, canvas);
    assert.deepStrictEqual(phases, [platform.NATIVE_PHASE_BEGAN, platform.NATIVE_PHASE_ENDED]);
}

// Verifies mouse input still reaches the game if the browser declines a focus request.
function testCanvasInputWhenFocusIsDenied() {
    const context = loadPointerEnvironment();
    const active = {};
    const buttons = [];
    let focusCalls = 0;
    context.document.activeElement = active;
    context.Module.canvas.focus = () => ++focusCalls;
    context.DefoldPlatform.mouseButtonFunc = (button, status) => buttons.push([button, status]);
    context.DefoldPlatform.onMouseButtonDown({ ...mouseEvent(context), type: "mousedown", isTrusted: true });
    assert.strictEqual(focusCalls, 1);
    assert.strictEqual(context.document.activeElement, active);
    assert.deepStrictEqual(buttons, [[0, 1]]);
}

// Verifies keyboard and pointer callbacks forward the browser event to audio without resuming from unfocused input.
function testInputAudioActivation() {
    const context = loadPointerEnvironment();
    const platform = context.DefoldPlatform;
    const events = [];
    context.DefoldSoundDevice = { TryResumeAudio: event => events.push(event) };
    const key = { target: context.Module.canvas, type: "keydown", keyCode: 32, code: "Space",
                  isTrusted: true, preventDefault() {}, stopPropagation() {} };
    platform.onKeydown(key);
    assert.strictEqual(events.pop(), key);
    assert.strictEqual(platform.keys[32], 1);

    for (const handler of ["onMouseButtonDown", "onMouseButtonUp", "onTouchStart", "onTouchEnd", "onTouchCancel"]) {
        const event = handler.includes("Touch") ? touchEvent(context, []) : mouseEvent(context);
        platform[handler](event);
        assert.strictEqual(events.pop(), event);
    }

    context.document.activeElement = {};
    platform.onKeydown({ ...key, target: context.document.activeElement });
    assert.strictEqual(events.length, 0);

    // Keyboard-only games must still work when the sound device is stripped.
    delete context.DefoldSoundDevice;
    platform.onKeydown(key);
    assert.strictEqual(platform.keys[32], 1);
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

async function runTests() {
    for (const test of [testGamepadCapabilitiesAndInput, testLoaderFullscreenToggle,
                       testLoaderWebGLSkipsWebGPUProbe, testLoaderWebGPUProbeGatesStartup,
                       testLoaderStartupFocus, testLoaderIframeStartupFocus,
                       testLoaderDelayedStartupPreservesFocus, testLoaderArchiveConcurrency,
                       testLoaderArchiveBufferOwnership,
                       testCanvasFocusRequests, testTouchFocusRetry, testCanvasInputWhenFocusIsDenied,
                       testInputAudioActivation, testTouchMouseSources, testMixedTouchAndMouseSources]) {
        try {
            await test();
            process.stdout.write(test.name + " passed\n");
        } catch (error) {
            console.error(test.name, error);
            process.exitCode = 1;
        }
    }
}

runTests();
