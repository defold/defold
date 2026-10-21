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
    const dispatch = type => {
        for (const callback of Array.from(listeners.get(type) || [])) callback();
    };
    const context = vm.createContext({
        console,
        Module: {},
        MainLoop: { currentFrameNumber: 1 },
        navigator: { getGamepads: () => [] },
        window: { addEventListener() {}, removeEventListener() {} },
        document: {
            fullscreenElement: null,
            addEventListener(type, callback) {
                if (!listeners.has(type)) listeners.set(type, new Set());
                listeners.get(type).add(callback);
            },
            removeEventListener: (type, callback) => listeners.get(type)?.delete(callback),
            exitFullscreen() {
                this.fullscreenElement = null;
                dispatch("fullscreenchange");
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
            dispatch("fullscreenchange");
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

function loadLoaderEnvironment(hasWebGPU = false, settings = {}) {
    const loaderPath = path.resolve(__dirname,
        "../../../../com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/bundle/resources/web/dmloader.js");
    const flags = { "html5.verify_downloaded_file_size": true, DEFOLD_HAS_WASM_ENGINE: true,
                    DEFOLD_HAS_WEBGPU: hasWebGPU, ...settings };
    const loader = fs.readFileSync(loaderPath, "utf8")
        .replace(/\{\{![\s\S]*?\}\}/g, "")
        .replace(/\{\{([#^])([^}]+)\}\}([\s\S]*?)\{\{\/\2\}\}/g,
            (_, kind, name, content) => (kind === "#" ? !!flags[name] : !flags[name]) ? content : "")
        .replace(/\{\{([^}]+)\}\}/g, (_, name) => String(flags[name] ?? 0));
    const context = loadEnvironment();
    vm.runInContext(loader, context, { filename: loaderPath });
    return context;
}

function loadResizeEnvironment(settings = {}, scaleMode = "stretch") {
    const context = loadLoaderEnvironment(false, {
        "display.width": 640, "display.height": 480, "display.high_dpi": true,
        ["DEFOLD_SCALE_MODE_IS_" + scaleMode.toUpperCase()]: true, ...settings
    });
    const container = { style: {} };
    const canvas = {
        width: 640, height: 480,
        getBoundingClientRect: () => ({ left: 10, top: 20,
            right: 10 + parseFloat(container.style.width), bottom: 20 + parseFloat(container.style.height) })
    };
    context.navigator.userAgent = "";
    Object.assign(context.window, { innerWidth: 900, innerHeight: 700, devicePixelRatio: 3 });
    context.document.getElementById = id => id === "canvas" ? canvas : container;
    context.Module.canvas = canvas;
    context.Browser = { setCanvasSize: (width, height) => Object.assign(canvas, { width, height }) };
    context._dmNativeSetWindowSize = (width, height) => context.LibraryDefoldPlatform.dmNativeSetWindowSize(width, height);
    context.LibraryDefoldPlatform.dmNativeOpenWindowJS(640, 480, 8, 0, 0,
        settings["display.high_dpi"] ?? true, false, 2);
    context.resizes = [];
    context.DefoldPlatform.resizeFunc = (width, height) => context.resizes.push([width, height]);
    context.resize = () => {
        context.CUSTOM_PARAMETERS.resize_window_callback();
        context.LibraryDefoldPlatform.dmNativeSwapBuffers();
    };
    context.container = container;
    return context;
}

// Verify bounds keep backing pixels and display scale consistent, including fractional DPR and High DPI off (#8040).
function testLoaderPixelRatioBounds() {
    for (const [highDPI, dpr, min, max, expected] of [
        [true, 3, 0, 0, 3], [true, undefined, 0, 0, 1], [true, 0.75, 0, 0, 0.75],
        [true, 3, 0, 2, 2], [true, 3, 0, 1.5, 1.5], [true, 3, 0, 0.5, 0.5],
        [true, 1, 1.5, 0, 1.5], [true, 3, 1.5, 1.5, 1.5], [true, 1.25, 1, 2, 1.25],
        [false, 3, 1.5, 2, 1]
    ]) {
        const context = loadResizeEnvironment({ "display.high_dpi": highDPI,
            DEFOLD_MIN_PIXEL_RATIO: min, DEFOLD_MAX_PIXEL_RATIO: max });
        context.window.devicePixelRatio = dpr;
        context.resize();
        assert.deepStrictEqual(context.resizes, [[Math.floor(900 * expected), Math.floor(700 * expected)]]);
        const displayScale = context.LibraryDefoldPlatform.dmNativeGetDisplayScaleFactor();
        assert.strictEqual(displayScale, expected);
        // Fixed orthographic cameras divide the buffer size by this scale to obtain the visible world size.
        assert.deepStrictEqual([context.Module.canvas.width / displayScale, context.Module.canvas.height / displayScale], [900, 700]);
        assert.strictEqual(context.container.style.width, "900px");
        assert.strictEqual(context.container.style.height, "700px");
        // Touch conversion must use the actual buffer/CSS ratio, not the device's uncapped DPR.
        const coordinates = context.DefoldPlatform.convertCoordinatesFromMonitorToWebGLPixels(460, 370);
        assert.deepStrictEqual(Array.from(coordinates), [context.Module.canvas.width / 2, context.Module.canvas.height / 2]);
        context.resize();
        assert.strictEqual(context.resizes.length, 1);
    }
}

// Verify older/custom loaders without a pixel-ratio helper retain their existing display scale and High DPI behavior.
function testDisplayScaleWithoutLoaderPixelRatio() {
    for (const highDPI of [false, true]) {
        const context = loadResizeEnvironment({ "display.high_dpi": highDPI });
        delete context.Module.getPixelRatio;
        assert.strictEqual(context.LibraryDefoldPlatform.dmNativeGetDisplayScaleFactor(), highDPI ? 3 : 1);
    }
}

// Verify all four scale modes retain their CSS layout while applying the cap, including fractional backing dimensions.
function testLoaderPixelRatioScaleModes() {
    for (const [mode, viewport, css, pixels] of [
        ["stretch", [900, 700], [900, 700], [1350, 1050]],
        ["fit", [900, 700], [900, 675], [1350, 1012]],
        ["fit", [900, 600], [800, 600], [1200, 900]],
        ["downscale_fit", [900, 700], [640, 480], [960, 720]],
        ["downscale_fit", [300, 210], [280, 210], [420, 315]],
        ["no_scale", [300, 200], [640, 480], [960, 720]]
    ]) {
        const context = loadResizeEnvironment({ DEFOLD_MAX_PIXEL_RATIO: 1.5 }, mode);
        [context.window.innerWidth, context.window.innerHeight] = viewport;
        context.resize();
        assert.deepStrictEqual(context.resizes, [pixels]);
        assert.deepStrictEqual([parseFloat(context.container.style.width), parseFloat(context.container.style.height)], css);
    }
}

// Verify DPR-only changes resize the buffer, rearm the resolution listener and preserve custom callbacks on older browsers.
function testLoaderPixelRatioChanges() {
    for (const legacy of [false, true]) {
        const context = loadResizeEnvironment({ DEFOLD_MAX_PIXEL_RATIO: 2 });
        const queries = [];
        context.window.matchMedia = query => {
            const media = { query, listener: null };
            media[legacy ? "addListener" : "addEventListener"] = (...args) => media.listener = args.at(-1);
            media[legacy ? "removeListener" : "removeEventListener"] = (...args) => {
                assert.strictEqual(media.listener, args.at(-1));
                media.listener = null;
            };
            queries.push(media);
            return media;
        };
        context.EngineLoader.setupResizeCallback();
        context.LibraryDefoldPlatform.dmNativeSwapBuffers();
        assert.deepStrictEqual(context.resizes, [[1800, 1400]]);
        assert.strictEqual(context.LibraryDefoldPlatform.dmNativeGetDisplayScaleFactor(), 2);
        for (const dpr of [1.5, 1]) {
            context.window.devicePixelRatio = dpr;
            const previous = queries.at(-1);
            previous.listener();
            assert.strictEqual(previous.listener, null);
            assert.strictEqual(queries.at(-1).query, "(resolution: " + dpr + "dppx)");
            context.LibraryDefoldPlatform.dmNativeSwapBuffers();
            context.LibraryDefoldPlatform.dmNativeSwapBuffers();
            assert.strictEqual(context.LibraryDefoldPlatform.dmNativeGetDisplayScaleFactor(), dpr);
        }
        assert.deepStrictEqual(context.resizes, [[1800, 1400], [1350, 1050], [900, 700]]);

        const custom = loadResizeEnvironment({ DEFOLD_MAX_PIXEL_RATIO: 1.5 });
        custom.window.matchMedia = context.window.matchMedia;
        let customCalls = 0;
        custom.CUSTOM_PARAMETERS.resize_window_callback = () => {
            ++customCalls;
            custom.Module.canvas.width = 321;
            custom.Module.canvas.height = 123;
        };
        custom.EngineLoader.setupResizeCallback();
        custom.window.devicePixelRatio = 1;
        queries.at(-1).listener();
        custom.LibraryDefoldPlatform.dmNativeSwapBuffers();
        assert.deepStrictEqual(custom.resizes, [[321, 123]]);
        assert.strictEqual(customCalls, 2);
    }
}

// Verify fullscreen shares the loader's bounds, restores windowed dimensions and avoids repeated resize notifications.
function testLoaderFullscreenPixelRatio() {
    const context = loadResizeEnvironment({ DEFOLD_MAX_PIXEL_RATIO: 1.5 }, "no_scale");
    context.EngineLoader.setupResizeCallback();
    context.LibraryDefoldPlatform.dmNativeSwapBuffers();
    assert.deepStrictEqual(context.resizes, [[960, 720]]);
    assert.strictEqual(context.LibraryDefoldPlatform.dmNativeGetDisplayScaleFactor(), 1.5);
    context.Module.fullScreenContainer = context.createFullscreenElement();
    context.Module.toggleFullscreen();
    context.LibraryDefoldPlatform.dmNativeSwapBuffers();
    assert.deepStrictEqual(context.resizes, [[960, 720], [1350, 1050]]);
    assert.strictEqual(context.LibraryDefoldPlatform.dmNativeGetDisplayScaleFactor(), 1.5);
    // A fullscreen frame must notice a changed ratio even before a resize event is delivered.
    context.window.devicePixelRatio = 1;
    context.LibraryDefoldPlatform.dmNativeSwapBuffers();
    context.resize();
    assert.deepStrictEqual(context.resizes, [[960, 720], [1350, 1050], [900, 700]]);
    assert.strictEqual(context.LibraryDefoldPlatform.dmNativeGetDisplayScaleFactor(), 1);
    context.Module.toggleFullscreen();
    context.LibraryDefoldPlatform.dmNativeSwapBuffers();
    context.resize();
    assert.deepStrictEqual(context.resizes, [[960, 720], [1350, 1050], [900, 700], [640, 480]]);
    assert.strictEqual(context.LibraryDefoldPlatform.dmNativeGetDisplayScaleFactor(), 1);

    // Hosts with an older/custom loader keep the existing fullscreen scaling fallback.
    delete context.Module.getPixelRatio;
    context.DefoldPlatform.isFullscreen = true;
    context.LibraryDefoldPlatform.dmNativeSwapBuffers();
    assert.deepStrictEqual(context.resizes.at(-1), [2700, 2100]);
    assert.strictEqual(context.LibraryDefoldPlatform.dmNativeGetDisplayScaleFactor(), 3);

    // Fullscreen changes that keep the same backing dimensions must not emit another resize.
    const unchanged = loadResizeEnvironment({ DEFOLD_MAX_PIXEL_RATIO: 1.5 });
    unchanged.EngineLoader.setupResizeCallback();
    unchanged.LibraryDefoldPlatform.dmNativeSwapBuffers();
    unchanged.Module.fullScreenContainer = unchanged.createFullscreenElement();
    for (let transition = 0; transition < 2; ++transition) {
        unchanged.Module.toggleFullscreen();
        unchanged.LibraryDefoldPlatform.dmNativeSwapBuffers();
    }
    assert.deepStrictEqual(unchanged.resizes, [[1350, 1050]]);
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
                       testLoaderWebGLSkipsWebGPUProbe, testLoaderWebGPUProbeGatesStartup, testLoaderArchiveConcurrency,
                       testLoaderArchiveBufferOwnership,
                       testLoaderPixelRatioBounds, testDisplayScaleWithoutLoaderPixelRatio, testLoaderPixelRatioScaleModes,
                       testLoaderPixelRatioChanges, testLoaderFullscreenPixelRatio,
                       testTouchMouseSources, testMixedTouchAndMouseSources]) {
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
