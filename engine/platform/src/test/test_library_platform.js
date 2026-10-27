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
const crypto = require("crypto");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const nativeHeader = fs.readFileSync(path.join(__dirname, "../native/native.h"), "utf8");
const joystickParams = ["PRESENT", "AXES", "BUTTONS", "HATS"].map(name =>
    Number(nativeHeader.match(new RegExp("^#define NATIVE_" + name + "\\s+(0x[0-9A-Fa-f]+)", "m"))[1]));

function loadEnvironment() {
    const listeners = new Map();
    const windowListeners = new Map();
    const dispatch = type => {
        for (const callback of Array.from(listeners.get(type) || [])) callback();
    };
    const context = vm.createContext({
        console,
        Module: {},
        MainLoop: { currentFrameNumber: 1 },
        navigator: { getGamepads: () => [] },
        window: {
            addEventListener(type, callback) {
                if (!windowListeners.has(type)) windowListeners.set(type, new Set());
                windowListeners.get(type).add(callback);
            },
            removeEventListener(type, callback) {
                if (windowListeners.has(type)) windowListeners.get(type).delete(callback);
            },
            dispatchEvent(event) {
                for (const callback of windowListeners.get(event.type) || []) callback(event);
            },
            matchMedia: () => ({ addEventListener() {}, removeEventListener() {} })
        },
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

function loadLoaderEnvironment(hasWebGPU = false, verifySize = true, parameters = {}) {
    const loaderPath = path.resolve(__dirname,
        "../../../../com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/bundle/resources/web/dmloader.js");
    const values = { "html5.verify_downloaded_file_size": verifySize, DEFOLD_HAS_WASM_ENGINE: true,
                     DEFOLD_HAS_WEBGPU: hasWebGPU, ...parameters };
    const loader = fs.readFileSync(loaderPath, "utf8")
        .replace(/\{\{![\s\S]*?\}\}/g, "")
        .replace(/\{\{([#^])([^}]+)\}\}([\s\S]*?)\{\{\/\2\}\}/g,
            (_, kind, name, content) => (kind === "#" ? !!values[name] : !values[name]) ? content : "")
        .replace(/\{\{([^}]+)\}\}/g, (_, name) => String(values[name.trim()] ?? 0));
    const context = loadEnvironment();
    context.setTimeout = setTimeout;
    context.clearTimeout = clearTimeout;
    vm.runInContext(loader, context, { filename: loaderPath });
    return context;
}

function loadResizeEnvironment(settings = {}, scaleMode = "stretch") {
    const context = loadLoaderEnvironment(false, true, {
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
    if (hasWebGL !== undefined) context.Module.hasWebGLSupport = () => hasWebGL;
    const session = context.GameStartup.begin();
    context.console = { log() {}, error() {} };
    const ready = session.runtime.resolve;
    session.runtime.resolve = () => { ++calls.started; ready(); };
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

// Missing, throwing, or experimental-only WebGL contexts must reach the supported/unsupported path safely (#550).
function testLoaderWebGLDetection() {
    for (const scenario of ["webgl", "experimental-webgl", "missing", "throws"]) {
        const { context, calls } = setupLoaderStartup(false);
        context.console = { log() {}, error() {} };
        context.WebGLRenderingContext = class {};
        context.document.createElement = () => ({ getContext(type) {
            if (scenario === "throws") throw new Error("context unavailable");
            return type === scenario ? new context.WebGLRenderingContext() : null;
        } });
        context.Module.runApp();
        const supported = scenario === "webgl" || scenario === "experimental-webgl";
        assert.strictEqual(calls.started, supported ? 1 : 0);
        assert.strictEqual(calls.unsupported, supported ? 0 : 1);
    }
}

// Native crash reporting must wait for runtime initialization, even when ccall already exists (#9841).
function testLoaderErrorHandlerReadiness() {
    for (const initialized of [false, true]) {
        const { context } = setupLoaderStartup(false, true);
        const errors = [], dumps = [];
        context.Module.setStatus = () => {};
        context.CUSTOM_PARAMETERS.start_error = error => errors.push(error);
        context.Module.ccall = (...args) => dumps.push(args);
        if (initialized) context.Module.onRuntimeInitialized();
        const error = { type: "error", message: "startup failed", filename: "game.js", lineno: 42, colno: 3 };
        context.window.dispatchEvent(error);
        assert.deepStrictEqual(errors, [error]);
        assert.strictEqual(dumps.length, initialized ? 1 : 0);
        if (initialized) {
            assert.strictEqual(dumps[0][0], "JSWriteDump");
            assert.deepStrictEqual(JSON.parse(dumps[0][3][0]), ["game.js:42:3"]);
        }
    }
}

// The canvas blocks the browser's right-click menu only when requested (#2710).
function testLoaderContextMenu() {
    for (const disabled of [false, true]) {
        const { context } = setupLoaderStartup(false, true);
        context.CUSTOM_PARAMETERS.disable_context_menu = disabled;
        context.Module.runApp();
        let prevented = false;
        if (disabled) {
            context.Module.canvas.oncontextmenu({ preventDefault() { prevented = true; } });
            assert.strictEqual(prevented, true);
        } else {
            assert.strictEqual(context.Module.canvas.oncontextmenu, undefined);
        }
    }
}

// Verify WebGPU builds await adapter discovery and retain WebGL fallback when discovery fails.
async function testLoaderWebGPUProbeGatesStartup() {
    for (const hasWebGL of [true, false]) {
        for (const outcome of ["adapter", "null", "rejected", "throws", "unavailable"]) {
            const { context, calls } = setupLoaderStartup(true, hasWebGL);
            let resolveProbe, rejectProbe;
            context.console = { log() {}, error() {} };
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

// A late graphics probe must not mark a replacement session's engine ready.
async function testLoaderWebGPUCancelledStartup() {
    const { context, calls } = setupLoaderStartup(true, true);
    const probe = deferred();
    context.navigator.gpu = { requestAdapter: () => probe.promise };
    const session = context.GameStartup._session;
    context.Module.runApp("canvas", session);
    context.GameStartup.begin();
    probe.resolve({});
    await probe.promise;
    assert.strictEqual(calls.started, 0);
    assert.strictEqual(context.Module.hasWebGPUSupport(), false);
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

// Resize must use drawing-buffer DPI (#8844) without clearing the canvas again for an unchanged viewport (#11869).
function testLoaderResizeCanvas() {
    for (const highDpi of [false, true]) {
        for (const devicePixelRatio of [undefined, 1, 1.25, 2]) {
            const context = loadLoaderEnvironment(false, true, {
                "display.width": 320, "display.height": 240, "display.high_dpi": highDpi,
                DEFOLD_SCALE_MODE_IS_FIT: true, "html5.show_fullscreen_button": true
            });
            const container = { style: {} };
            const writes = [];
            const canvas = {
                set width(value) { writes.push(["width", value]); },
                set height(value) { writes.push(["height", value]); }
            };
            context.document.getElementById = id => ({ "app-container": container, canvas })[id];
            Object.assign(context.window, { innerWidth: 500, innerHeight: 342, devicePixelRatio });
            const resize = context.CUSTOM_PARAMETERS.resize_window_callback;
            resize();
            const dpi = highDpi ? devicePixelRatio || 1 : 1;
            assert.deepStrictEqual(writes, [["width", 400 * dpi], ["height", 300 * dpi]]);
            assert.strictEqual(container.style.width, "400px");
            assert.strictEqual(container.style.height, "342px");
            resize();
            assert.strictEqual(writes.length, 2, "focus/orientation events must not clear an unchanged canvas");

            context.window.innerWidth = 301;
            resize();
            assert.deepStrictEqual(writes.slice(2), [["width", Math.floor(301 * dpi)], ["height", Math.floor(225.75 * dpi)]]);
            assert.strictEqual(container.style.width, "301px");
            assert.strictEqual(container.style.height, "267.75px");
            resize();
            assert.strictEqual(writes.length, 4);
        }
    }
}

// Preserve valid progress markup (#8730), cap its width (#8075), and clear the Safari splash without forcing an opaque background (#2188/#10093).
function testLoaderProgressView() {
    const context = loadLoaderEnvironment();
    const elements = new Map();
    let removed = 0;
    const parent = { removeChild(element) { ++removed; element.parentElement = null; } };
    const canvas = {
        style: { background: "url(splash.png)", backgroundColor: "transparent" },
        insertAdjacentHTML(position, html) {
            assert.strictEqual(position, "afterend");
            for (const [, id] of html.matchAll(/id="([^"]+)"/g)) {
                elements.set(id, { style: {}, parentElement: parent });
            }
        }
    };
    context.Module.canvas = canvas;
    context.document.getElementById = id => elements.get(id) || null;
    context.ProgressView.addProgress(canvas);
    const bar = elements.get("defold-progress-bar");
    assert(bar, "progress bar must be addressable by its expected ID");
    assert(elements.has("defold-progress"));
    for (const [percentage, scale] of [[0, 0], [25, 0.25], [100, 1], [125, 1]]) {
        context.ProgressView.updateProgress(percentage);
        assert.strictEqual(bar.style.transform, "scaleX(" + scale + ")");
    }
    context.ProgressView.removeProgress();
    context.ProgressView.removeProgress();
    assert.strictEqual(removed, 1);
    assert.strictEqual(canvas.style.background, "none");
    assert.strictEqual(canvas.style.backgroundColor, "transparent");
}

// Verify the shared XHR limit covers all files and out-of-order downloads preserve data and notification order.
async function testLoaderArchiveConcurrency() {
    for (const limit of [1, 2, 6, undefined]) {
        const responses = new Map();
        const expectedFiles = [];
        const loadedFiles = [];
        const files = [8, 3].map((pieceCount, fileIndex) => {
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
        const { loader, requests, calls, waitFor } = setupArchiveDownloads(files);
        loader.MAX_CONCURRENT_XHR = limit;
        loader.addFileLoadedListener(file => loadedFiles.push({ name: file.name, data: Array.from(file.data) }));
        loader.downloadContent();
        await waitFor(() => requests.length > 0);
        while (!calls.completed) {
            const pending = requests.filter(request => request.pending);
            const xhr = pending[pending.length - 1];
            assert(xhr, "no request or archive completion");
            assert.strictEqual(xhr.responseType, "arraybuffer");
            xhr.complete(responses.get(xhr.url));
            await waitFor(() => calls.completed || requests.some(request => request.pending));
        }
        assert.strictEqual(calls.completed, 1, "archive did not complete with concurrency " + limit);
        assert.deepStrictEqual(loadedFiles, expectedFiles);
        assert.strictEqual(calls.maxPending, limit || 6);
    }
}

function archiveFile(name, pieceCount = 1) {
    return { name, size: pieceCount, pieces: Array.from({ length: pieceCount }, (_, index) => ({ name: name + index, offset: index })) };
}

function deferred() {
    let resolve, reject;
    const promise = new Promise((yes, no) => { resolve = yes; reject = no; });
    return { promise, resolve, reject };
}

// Observe existing async operations without replacing their implementation or changing the XHR callback contract.
function observeAsyncMethod(target, name) {
    const results = [];
    const started = deferred();
    results.started = started.promise;
    const method = target[name].bind(target);
    target[name] = (...args) => {
        const result = method(...args);
        results.push(result);
        started.resolve();
        return result;
    };
    return results;
}

function setupArchiveDownloads(files = [archiveFile("a")], verifySize = true, fullStartup = false) {
    const context = loadLoaderEnvironment(false, verifySize);
    const loader = context.GameArchiveLoader;
    const timers = new Map();
    const requests = [];
    const waiters = new Set();
    const calls = { loaded: [], errors: [], consoleErrors: [], completed: 0, engines: 0, maxPending: 0 };
    let nextTimer = 1;
    function notify() {
        for (const waiter of waiters) {
            if (waiter.predicate()) {
                waiters.delete(waiter);
                waiter.resolve();
            }
        }
    }
    function waitFor(predicate) {
        if (predicate()) return Promise.resolve();
        return new Promise(resolve => waiters.add({ predicate, resolve }));
    }
    context.console = { log() {}, warn() {}, error(error) { calls.consoleErrors.push(error); } };
    context.setTimeout = callback => {
        const id = nextTimer++;
        timers.set(id, callback);
        notify();
        return id;
    };
    context.clearTimeout = id => timers.delete(id);
    context.XMLHttpRequest = class {
        static DONE = 4;
        open(method, url) { this.method = method; this.url = url; }
        send() {
            this.pending = true;
            requests.push(this);
            calls.maxPending = Math.max(calls.maxPending, requests.filter(request => request.pending).length);
            notify();
        }
        abort() {
            this.aborted = true;
            this.pending = false;
            this.readyState = 0;
            if (this.onabort) this.onabort({});
        }
        progress(loaded) {
            assert.strictEqual(this.pending, true, "progress requires an active request");
            this.onprogress({ loaded });
        }
        complete(response, status = 200) {
            assert.strictEqual(this.pending, true, "a request can complete only once");
            this.readyState = 4;
            this.status = status;
            this.response = response;
            this.progress(typeof response === "string" ? Buffer.byteLength(response) : response.byteLength);
            if (this.aborted) return;
            this.pending = false;
            this.onload({});
        }
        fail(type = "error") {
            assert.strictEqual(this.pending, true, "only an active request can fail");
            this.pending = false;
            if (this["on" + type]) this["on" + type]({});
        }
    };
    context.FileLoader.options.retryCount = 2;
    context.CUSTOM_PARAMETERS.start_error = error => { calls.errors.push(String(error)); notify(); };
    context.Module.isWASMSupported = true;
    context.EngineLoader.arc_sha1 = "";
    if (!fullStartup) {
        context.Module._filesystemReady = true;
        context.Module.preloadAll = () => {};
        context.Module._callMain = () => {};
        context.EngineLoader.loadWasmAsync = () => ++calls.engines;
    }
    loader._files = files;
    loader.addFileLoadedListener(file => { calls.loaded.push(file.name); notify(); });
    loader.addArchiveLoadedListener(() => { ++calls.completed; notify(); });
    async function runRetry() {
        await waitFor(() => timers.size > 0);
        const count = requests.length;
        assert.strictEqual(timers.size, 1);
        const [id, callback] = timers.entries().next().value;
        timers.delete(id);
        callback();
        await waitFor(() => requests.length > count);
    }
    return { context, loader, timers, requests, calls, runRetry, waitFor, notify };
}

// Interleaved XHR events count each byte once (71c81f5d01), including downloads without Content-Length (#604).
async function testLoaderArchiveProgressWithoutLength() {
    const file = { name: "a", size: 8, pieces: [{ name: "a0", offset: 0 }, { name: "a1", offset: 4 }] };
    const { context, loader, requests, waitFor } = setupArchiveDownloads([file]);
    const percentages = [];
    context.ProgressUpdater.setupTotal(file.size);
    context.ProgressUpdater.addListener(value => percentages.push(value));
    const completion = loader.downloadContent();
    await waitFor(() => requests.length === 2);
    for (const [piece, loaded, expected] of [[0, 1, 1], [1, 2, 3], [0, 1, 3], [0, 3, 5], [1, 4, 7]]) {
        requests[piece].onprogress({ loaded, total: 0, lengthComputable: false });
        assert.strictEqual(context.ProgressUpdater.current, expected);
    }
    requests[1].complete(Uint8Array.of(5, 6, 7, 8).buffer);
    requests[0].complete(Uint8Array.of(1, 2, 3, 4).buffer);
    await completion;
    assert.strictEqual(context.ProgressUpdater.current, 8);
    assert.strictEqual(percentages[percentages.length - 1], 100);
    assert(percentages.every((value, index) => Number.isFinite(value) && value <= 100 &&
        (index === 0 || value >= percentages[index - 1])));
}

// Network errors, timeouts, and external aborts release their request slot and progress before retrying.
async function testLoaderArchiveRetryReleasesSlot() {
    for (const failure of ["error", "timeout", "abort"]) {
        const { context, loader, requests, calls, timers, runRetry, waitFor } = setupArchiveDownloads([archiveFile("a"), archiveFile("b")]);
        loader.MAX_CONCURRENT_XHR = 1;
        const completion = loader.downloadContent();
        await waitFor(() => requests.length === 1);
        requests[0].progress(5);
        const lateFailure = requests[0].onerror;
        if (failure === "abort") requests[0].abort();
        else requests[0].fail(failure);
        await waitFor(() => requests.length === 2 && timers.size === 1);
        assert.strictEqual(requests[1].url, "split/b0");
        assert.strictEqual(context.ProgressUpdater.current, 0);
        lateFailure({});
        assert.strictEqual(timers.size, 1, "late events must not schedule another retry");
        for (const event of ["load", "error", "timeout", "abort", "progress"]) {
            assert.strictEqual(requests[0]["on" + event], null, "settled requests release every handler");
        }
        requests[1].complete(Uint8Array.of(2).buffer);
        await runRetry();
        assert.strictEqual(requests[2].url, "split/a0?dmretry=1");
        requests[2].complete(Uint8Array.of(1).buffer);
        await completion;
        assert.strictEqual(calls.maxPending, 1);
        assert.strictEqual(context.ProgressUpdater.current, 2);
        assert.deepStrictEqual(calls.loaded, ["a", "b"]);
        assert.deepStrictEqual(calls.errors, []);
    }
}

// Cancelling an archive aborts active requests and removes queued pieces without starting another request.
async function testLoaderArchiveQueuedCancellation() {
    const { loader, requests, calls, waitFor } = setupArchiveDownloads([archiveFile("a", 2), archiveFile("b", 2)]);
    loader.MAX_CONCURRENT_XHR = 2;
    const completion = loader.downloadContent();
    await waitFor(() => requests.length === 2);
    loader.cleanUp();
    await assert.rejects(completion, { name: "AbortError" });
    assert.strictEqual(requests.length, 2);
    assert(requests.every(request => request.aborted));
    assert.deepStrictEqual(calls.loaded, []);
    assert.deepStrictEqual(calls.errors, []);
    assert.deepStrictEqual(calls.consoleErrors, [], "intentional cancellation is not a fatal error");
}

// Files download concurrently, but verification gates ordered notifications and archive completion.
async function testLoaderArchiveVerificationCompletion() {
    for (const verifySize of [true, false]) {
        const file = archiveFile("a", 2);
        file.sha1 = "00";
        const { loader, requests, calls, context, waitFor } = setupArchiveDownloads([file, archiveFile("b")], verifySize);
        const digest = deferred();
        context.window.crypto = { subtle: { digest: () => digest.promise } };
        const downloads = observeAsyncMethod(loader, "downloadFile");
        const completion = loader.downloadContent();
        await waitFor(() => requests.length === 3);
        requests[0].complete(Uint8Array.of(1).buffer);
        requests[1].complete(Uint8Array.of(2).buffer);
        requests[2].complete(Uint8Array.of(3).buffer);
        await downloads[1];
        assert.deepStrictEqual(calls.loaded, []);
        assert.strictEqual(calls.completed, 0);
        digest.resolve(Uint8Array.of(0).buffer);
        await completion;
        assert.strictEqual(requests.length, 3);
        assert.deepStrictEqual(calls.loaded, ["a", "b"]);
        assert.strictEqual(calls.completed, 1);
        assert.strictEqual(loader.isCompleted, true);
        assert.strictEqual(loader._files.length, 0, "completion releases the archive metadata");
        assert.deepStrictEqual(calls.errors, []);
    }
}

// Retrying one file must preserve completed peers and their progress while keeping notifications in manifest order.
async function testLoaderArchiveRetrySequence() {
    const { loader, requests, calls, timers, runRetry, context, waitFor } = setupArchiveDownloads([archiveFile("a"), archiveFile("b")]);
    const downloads = observeAsyncMethod(loader, "downloadFile");
    const completion = loader.downloadContent();
    await waitFor(() => requests.length === 2);
    requests[0].complete(new ArrayBuffer(0));
    requests[1].complete(Uint8Array.of(2).buffer);
    await downloads[1];
    await waitFor(() => timers.size === 1);
    assert.strictEqual(context.ProgressUpdater.current, 1);
    assert.deepStrictEqual(calls.loaded, []);
    await runRetry();
    assert.strictEqual(requests[2].url, "split/a0?dmretry=3");
    requests[2].complete(Uint8Array.of(1).buffer);
    await completion;
    assert.deepStrictEqual(calls.loaded, ["a", "b"]);
    assert.strictEqual(context.ProgressUpdater.current, 2);
    assert.strictEqual(requests.length, 3);
    assert.strictEqual(calls.completed, 1);
    assert.deepStrictEqual(calls.errors, []);
    assert.strictEqual(timers.size, 0);
}

// Verification retries use fresh buffers and counters without changing frozen manifest metadata.
async function testLoaderArchiveRetryStateOwnership() {
    for (const verifySize of [true, false]) {
        const file = archiveFile("a", 3);
        const expected = Uint8Array.of(1, 2, 3);
        file.sha1 = crypto.createHash("sha1").update(expected).digest("hex");
        file.pieces.forEach(Object.freeze);
        Object.freeze(file.pieces);
        Object.freeze(file);
        const metadata = JSON.stringify(file);
        const { context, loader, requests, calls, timers, runRetry, waitFor } = setupArchiveDownloads([file], verifySize);
        context.window.crypto = crypto.webcrypto;
        let loadedData;
        loader.addFileLoadedListener(result => loadedData = result.data);
        loader.downloadContent();
        await waitFor(() => requests.length > 0);
        requests.slice().forEach(xhr => xhr.complete(Uint8Array.of(9).buffer));
        await waitFor(() => timers.size === 1);
        assert.strictEqual(context.ProgressUpdater.current, 0);
        await runRetry();
        requests.slice(3).forEach(xhr => xhr.complete(Uint8Array.of(8).buffer));
        await waitFor(() => timers.size === 1);
        await runRetry();
        requests[8].complete(Uint8Array.of(3).buffer);
        requests[6].complete(Uint8Array.of(1).buffer);
        requests[7].complete(Uint8Array.of(2).buffer);
        await waitFor(() => calls.completed === 1);
        assert.deepStrictEqual(Array.from(loadedData), Array.from(expected));
        assert.strictEqual(context.Module._filesToPreload[0].data, loadedData);
        assert.strictEqual(context.ProgressUpdater.current, 3);
        assert.strictEqual(JSON.stringify(file), metadata);
        assert.deepStrictEqual(calls.errors, []);
    }
}

// A transport retry must discard failed progress and still assemble the archive exactly once through both loaders.
async function testLoaderArchiveTransportRetry() {
    const { context, loader, requests, calls, timers, runRetry, waitFor } = setupArchiveDownloads([archiveFile("a", 2)]);
    loader.downloadContent();
    await waitFor(() => requests.length > 0);
    requests[0].progress(10);
    requests[0].fail();
    await waitFor(() => timers.size === 1);
    assert.strictEqual(context.ProgressUpdater.current, 0);
    requests[1].complete(Uint8Array.of(2).buffer);
    await runRetry();
    assert.strictEqual(requests[2].url, "split/a0?dmretry=1");
    requests[2].complete(Uint8Array.of(1).buffer);
    await waitFor(() => calls.completed === 1);
    assert.deepStrictEqual(calls.loaded, ["a"]);
    assert.deepStrictEqual(calls.errors, []);
    assert.strictEqual(timers.size, 0);
    assert.strictEqual(context.ProgressUpdater.current, 2);
}

// Cancelling from the retry progress notification must not leave a timer capable of restarting the file.
async function testLoaderArchiveProgressCancellation() {
    for (const failure of ["transport", "verification"]) {
        const { context, loader, requests, calls, timers, waitFor } = setupArchiveDownloads();
        const cancelled = deferred();
        context.ProgressUpdater.addListener(progress => {
            if (progress === 0) {
                loader.cleanUp();
                cancelled.resolve();
            }
        });
        loader.downloadContent();
        await waitFor(() => requests.length > 0);
        if (failure === "transport") {
            requests[0].progress(1);
            requests[0].fail();
        } else {
            requests[0].complete(Uint8Array.of(9, 9).buffer);
        }
        await cancelled.promise;
        assert.strictEqual(timers.size, 0);
        assert.strictEqual(requests.length, 1);
        assert.deepStrictEqual(calls.loaded, []);
        assert.deepStrictEqual(calls.errors, []);
    }
}

// Repeated corrupt responses must stop at the verification budget and report failure only once.
async function testLoaderArchiveVerificationRetryLimit() {
    const { loader, requests, calls, timers, runRetry, waitFor } = setupArchiveDownloads();
    loader.downloadContent();
    await waitFor(() => requests.length > 0);
    for (let attempt = 0; attempt <= loader.MAX_VERIFICATION_RETRIES; ++attempt) {
        requests[attempt].complete(new ArrayBuffer(0));
        await waitFor(() => timers.size === 1 || calls.errors.length === 1);
        if (attempt < loader.MAX_VERIFICATION_RETRIES) await runRetry();
    }
    assert.strictEqual(requests.length, 3);
    assert.strictEqual(calls.errors.length, 1);
    assert.strictEqual(calls.completed, 0);
    assert.strictEqual(timers.size, 0);
}

// Cleanup cancels active XHRs and both retry timers before loading a replacement archive.
async function testLoaderArchiveCancellation() {
    for (const phase of ["downloading", "transport-retry", "verification-retry"]) {
        const { loader, requests, calls, timers, waitFor } = setupArchiveDownloads();
        loader.downloadContent();
        await waitFor(() => requests.length > 0);
        if (phase === "transport-retry") requests[0].fail();
        if (phase === "verification-retry") requests[0].complete(new ArrayBuffer(0));
        if (phase !== "downloading") await waitFor(() => timers.size === 1);
        loader.cleanUp();
        if (phase === "downloading") assert.strictEqual(requests[0].aborted, true);
        assert.strictEqual(timers.size, 0);
        loader.loadArchiveDescription("/new.json");
        requests[1].complete(JSON.stringify({ total_size: 1, content: [archiveFile("new")] }));
        await waitFor(() => requests.length === 3);
        assert.strictEqual(requests.length, 3);
        requests[2].complete(Uint8Array.of(1).buffer);
        await waitFor(() => calls.completed === 1);
        assert.deepStrictEqual(calls.loaded, ["new"]);
        assert.deepStrictEqual(calls.errors, []);
        assert.strictEqual(calls.completed, 1);
    }
}

// Async verification of a replaced archive must not complete, retry or report errors against the replacement.
async function testLoaderArchiveStaleVerification() {
    for (const rejects of [false, true]) {
        const file = archiveFile("a");
        file.sha1 = "00";
        const { context, loader, requests, calls, timers, waitFor } = setupArchiveDownloads([file]);
        const digest = deferred();
        context.window.crypto = { subtle: { digest: () => digest.promise } };
        const verifications = observeAsyncMethod(loader, "verifyFile");
        loader.downloadContent();
        await waitFor(() => requests.length > 0);
        requests[0].complete(Uint8Array.of(1).buffer);
        await verifications.started;
        loader.loadArchiveDescription("/new.json");
        requests[1].complete(JSON.stringify({ total_size: 1, content: [archiveFile("new")] }));
        await waitFor(() => requests.length === 3);
        if (rejects) {
            digest.reject(new Error("stale hash failure"));
            await assert.rejects(verifications[0], /stale hash failure/);
        } else {
            digest.resolve(Uint8Array.of(0).buffer);
            await verifications[0];
        }
        assert.strictEqual(calls.engines, 1);
        assert.strictEqual(timers.size, 0);
        assert.deepStrictEqual(calls.loaded, []);
        requests[2].complete(Uint8Array.of(2).buffer);
        await waitFor(() => calls.completed === 1);
        assert.deepStrictEqual(calls.loaded, ["new"]);
        assert.deepStrictEqual(calls.errors, []);
    }
}

// An obsolete manifest hash result must not replace a newer archive or restart its engine download.
async function testLoaderArchiveStaleDescription() {
    const { context, loader, requests, calls, waitFor } = setupArchiveDownloads([]);
    const digest = deferred();
    context.EngineLoader.arc_sha1 = "00";
    context.TextEncoder = TextEncoder;
    context.window.crypto = { subtle: { digest: () => digest.promise } };
    const descriptions = observeAsyncMethod(loader, "parseDescription");
    loader.loadArchiveDescription("/old.json");
    requests[0].complete(JSON.stringify({ total_size: 1, content: [archiveFile("old")] }));
    await descriptions.started;
    loader.loadArchiveDescription("/new.json");
    context.EngineLoader.arc_sha1 = "";
    requests[1].complete(JSON.stringify({ total_size: 1, content: [archiveFile("new")] }));
    await waitFor(() => requests.length === 3);
    digest.resolve(Uint8Array.of(0).buffer);
    await assert.rejects(descriptions[0], { name: "AbortError" });
    assert.strictEqual(calls.engines, 2);
    assert.strictEqual(requests.length, 3);
    assert.strictEqual(requests[2].url, "split/new0");
    requests[2].complete(Uint8Array.of(1).buffer);
    await waitFor(() => calls.completed === 1);
    assert.deepStrictEqual(calls.loaded, ["new"]);
    assert.deepStrictEqual(calls.errors, []);
}

// Cancelling a DMFS cache check must close its own stream once and never reopen the old file after hashing.
async function testLoaderArchiveStaleCachedFile() {
    for (const verifySize of [true, false]) {
        const oldFile = archiveFile("old");
        oldFile.sha1 = "00";
        const { context, loader, requests, calls, waitFor } = setupArchiveDownloads([oldFile], verifySize);
        const digest = deferred();
        const streams = [];
        context.Module.isDMFSSupported = true;
        context.DMSYS = { GetUserPersistentDataRoot: () => "/data" };
        context.window.crypto = { subtle: { digest: () => digest.promise } };
        context.FS = {
            unlink() {},
            stat(name) { if (name === "/data/old") return { size: 1 }; throw new Error("not found"); },
            open(name, mode) { const stream = { name, mode, closed: false }; streams.push(stream); return stream; },
            mmap() { return Uint8Array.of(0); },
            close(stream) { assert.strictEqual(stream.closed, false); stream.closed = true; },
            write(stream) { assert.strictEqual(stream.closed, false); }
        };
        const cacheReads = observeAsyncMethod(context.ArchiveStorage, "readCached");
        const oldDownload = loader.downloadContent();
        await cacheReads.started;
        loader.cleanUp();
        assert.strictEqual(streams[0].closed, true);
        loader._files = [archiveFile("new")];
        loader.downloadContent();
        await waitFor(() => requests.length > 0);
        digest.resolve(Uint8Array.of(0).buffer);
        await assert.rejects(oldDownload, { name: "AbortError" });
        assert.deepStrictEqual(streams.map(stream => [stream.name, stream.mode]), [["/data/old", "r"], ["/data/new", "w+"]]);
        assert.deepStrictEqual(calls.loaded, []);
        requests[0].complete(Uint8Array.of(1).buffer);
        await waitFor(() => calls.completed === 1);
        assert.deepStrictEqual(calls.loaded, ["new"]);
        assert(streams.every(stream => stream.closed));
        assert.deepStrictEqual(calls.errors, []);
    }
}

function setupDMFSVerification(cached = []) {
    const files = ["a", "b", "c"].map(name => Object.assign(archiveFile(name), { sha1: "00" }));
    const environment = setupArchiveDownloads(files);
    const { context, notify } = environment;
    const streams = [], hashes = [], mappings = [], removed = [];
    context.Module.isDMFSSupported = true;
    context.DMSYS = { GetUserPersistentDataRoot: () => "/data" };
    context.FS = {
        stat(name) { if (cached.includes(name.slice(6))) return { size: 1 }; throw new Error("not cached"); },
        open(name, mode) { const stream = { name, mode, closed: false, mapped: false }; streams.push(stream); return stream; },
        write(stream) { assert.strictEqual(stream.closed, false); },
        mmap(stream) {
            assert.strictEqual(stream.closed, false);
            stream.mapped = true;
            mappings.push({ name: stream.name, active: streams.filter(stream => stream.mapped).length });
            return Uint8Array.of(1);
        },
        close(stream) {
            assert.strictEqual(stream.closed, false, "streams close only once");
            stream.closed = true;
            stream.mapped = false;
        },
        unlink(name) { removed.push(name); }
    };
    context.window.crypto = { subtle: { digest() {
        const hash = deferred();
        hashes.push(hash);
        notify();
        return hash.promise;
    } } };
    const downloads = observeAsyncMethod(environment.loader, "downloadPieces");
    const verifications = observeAsyncMethod(context.FileLoader, "verifyHash");
    return { ...environment, streams, hashes, mappings, removed, downloads, verifications };
}

// Cached and downloaded DMFS files share one mapping/hash slot while network downloads still overlap.
async function testLoaderArchiveDMFSVerificationConcurrency() {
    for (const cached of [[], ["a", "b", "c"], ["c"]]) {
        const { loader, requests, calls, waitFor, streams, hashes, mappings, downloads } = setupDMFSVerification(cached);
        const completion = loader.downloadContent();
        const count = 3 - cached.length;
        if (count) {
            await waitFor(() => requests.length === count);
            assert.strictEqual(calls.maxPending, count, "verification must not serialize network downloads");
            requests.forEach(xhr => xhr.complete(Uint8Array.of(1).buffer));
            await Promise.all(downloads);
        }
        for (let index = 0; index < 3; ++index) {
            await waitFor(() => hashes.length > index);
            hashes[index].resolve(Uint8Array.of(0).buffer);
        }
        await completion;
        assert.strictEqual(mappings.length, 3);
        assert(mappings.every(mapping => mapping.active === 1), "each mapped stream closes before the next mapping");
        assert(streams.every(stream => stream.closed));
        assert.deepStrictEqual(calls.loaded, ["a", "b", "c"]);
        assert.deepStrictEqual(calls.errors, []);
        assert.strictEqual(requests.length, count);
    }
}

// Cancelling queued DMFS verification closes active streams and prevents late hashes from mapping another file.
async function testLoaderArchiveDMFSVerificationQueueCancellation() {
    for (const cached of [[], ["a", "b", "c"]]) {
        for (const rejects of [false, true]) {
            const { loader, requests, calls, timers, waitFor, streams, hashes, mappings, downloads, verifications } = setupDMFSVerification(cached);
            const completion = loader.downloadContent();
            if (!cached.length) {
                await waitFor(() => requests.length === 3);
                requests.forEach(xhr => xhr.complete(Uint8Array.of(1).buffer));
                await Promise.all(downloads);
            }
            await waitFor(() => hashes.length > 0);
            loader.cleanUp();
            await assert.rejects(completion, { name: "AbortError" });
            assert(streams.every(stream => stream.closed));
            for (const hash of hashes) {
                if (rejects) hash.reject(new Error("stale hash"));
                else hash.resolve(Uint8Array.of(0).buffer);
            }
            await Promise.allSettled(verifications);
            assert.strictEqual(mappings.length, 1, "cancelled queue entries must never map files");
            assert.strictEqual(timers.size, 0);
            assert.strictEqual(calls.completed, 0);
            assert.deepStrictEqual(calls.loaded, []);
            assert.deepStrictEqual(calls.errors, []);
        }
    }
}

// A rejected DMFS hash closes its mapping and releases the slot before its whole-file retry delay.
async function testLoaderArchiveDMFSVerificationQueueRetry() {
    const { loader, requests, calls, timers, runRetry, waitFor, streams, hashes, mappings, removed, downloads } = setupDMFSVerification();
    const completion = loader.downloadContent();
    await waitFor(() => requests.length === 3);
    requests.forEach(xhr => xhr.complete(Uint8Array.of(1).buffer));
    await Promise.all(downloads);
    await waitFor(() => hashes.length > 0);
    hashes[0].reject(new Error("hash failed"));
    await waitFor(() => hashes.length > 1 && timers.size === 1);
    assert.strictEqual(streams[0].closed, true);
    assert.deepStrictEqual(removed, ["/data/a"]);
    for (let index = 1; index < 3; ++index) {
        await waitFor(() => hashes.length > index);
        hashes[index].resolve(Uint8Array.of(0).buffer);
    }
    await runRetry();
    requests[3].complete(Uint8Array.of(1).buffer);
    await waitFor(() => hashes.length === 4);
    hashes[3].resolve(Uint8Array.of(0).buffer);
    await completion;
    assert(mappings.every(mapping => mapping.active === 1), "failed hashes release mappings before the next verification");
    assert(streams.every(stream => stream.closed));
    assert.deepStrictEqual(calls.loaded, ["a", "b", "c"]);
    assert.deepStrictEqual(calls.errors, []);
    assert.strictEqual(calls.completed, 1);
}

// DMFS verification retries close each stream once and truncate oversized persisted data before the next attempt.
async function testLoaderArchiveDMFSRetry() {
    for (const verifySize of [true, false]) {
        const file = archiveFile("a", 2);
        const metadata = JSON.stringify(file);
        const { context, loader, requests, calls, timers, runRetry, waitFor } = setupArchiveDownloads([file], verifySize);
        const streams = [];
        let persisted = new Uint8Array(0);
        context.Module.isDMFSSupported = true;
        context.DMSYS = { GetUserPersistentDataRoot: () => "/data" };
        context.FS = {
            unlink() {},
            stat() { throw new Error("not found"); },
            open(name, mode) {
                assert.strictEqual(mode, "w+");
                const stream = { data: new Uint8Array(0), closed: false };
                streams.push(stream);
                return stream;
            },
            write(stream, data, offset, length, position) {
                assert.strictEqual(stream.closed, false);
                const grown = new Uint8Array(Math.max(stream.data.length, position + length));
                grown.set(stream.data);
                grown.set(data.subarray(offset, offset + length), position);
                stream.data = grown;
            },
            close(stream) {
                assert.strictEqual(stream.closed, false);
                stream.closed = true;
                persisted = stream.data;
            }
        };
        loader.downloadContent();
        await waitFor(() => requests.length > 0);
        requests[0].complete(Uint8Array.of(9, 9, 9).buffer);
        requests[1].complete(Uint8Array.of(9).buffer);
        await waitFor(() => timers.size === 1);
        assert.strictEqual(streams[0].closed, true);
        assert.strictEqual(persisted.length, 3);
        await runRetry();
        requests[3].complete(Uint8Array.of(2).buffer);
        requests[2].complete(Uint8Array.of(1).buffer);
        await waitFor(() => calls.completed === 1);
        assert.strictEqual(streams.length, 2);
        assert(streams.every(stream => stream.closed));
        assert.deepStrictEqual(Array.from(persisted), [1, 2]);
        assert.strictEqual(JSON.stringify(file), metadata);
        assert.deepStrictEqual(calls.errors, []);
    }
}

// Unreadable DMFS data must not reach SHA-1 (#10116); retry instead of accepting an unverified archive or stalling.
async function testLoaderArchiveUnreadableData() {
    const bytes = Uint8Array.of(7);
    for (const failure of ["null", "throws"]) {
        const file = archiveFile("a");
        file.sha1 = crypto.createHash("sha1").update(bytes).digest("hex");
        const { context, loader, requests, calls, timers, runRetry, waitFor } = setupArchiveDownloads([file]);
        const streams = [];
        let hashes = 0;
        context.window.crypto = { subtle: { digest(algorithm, data) {
            ++hashes;
            assert(data, "unreadable data must never be passed to digest");
            return crypto.webcrypto.subtle.digest(algorithm, data);
        } } };
        context.Module.isDMFSSupported = true;
        context.DMSYS = { GetUserPersistentDataRoot: () => "/data" };
        context.FS = {
            stat() { throw new Error("not cached"); },
            open() { const stream = { closed: false }; streams.push(stream); return stream; },
            write(stream, data) { stream.data = data; },
            mmap(stream) {
                if (streams.length > 1) return stream.data;
                if (failure === "throws") throw new Error("cannot map file");
                return null;
            },
            close(stream) { assert.strictEqual(stream.closed, false); stream.closed = true; },
            unlink() {}
        };
        const completion = loader.downloadContent();
        await waitFor(() => requests.length === 1);
        requests[0].complete(bytes.buffer);
        await waitFor(() => timers.size === 1);
        assert.strictEqual(hashes, 0);
        assert.deepStrictEqual(calls.loaded, []);
        assert.strictEqual(streams[0].closed, true);
        await runRetry();
        requests[1].complete(bytes.buffer);
        await completion;
        assert.strictEqual(hashes, 1);
        assert.deepStrictEqual(calls.loaded, ["a"]);
        assert.deepStrictEqual(calls.errors, []);
        assert(streams.every(stream => stream.closed));
    }
}

// A DMFS close failure after verification must retry with a fresh stream and report persistent failures instead of stalling.
async function testLoaderArchiveDMFSCloseFailure() {
    for (const alwaysFails of [false, true]) {
        const { context, loader, requests, calls, timers, runRetry, waitFor } = setupArchiveDownloads();
        const streams = [];
        const closeError = new Error("stream close failed");
        context.Module.isDMFSSupported = true;
        context.DMSYS = { GetUserPersistentDataRoot: () => "/data" };
        context.FS = {
            unlink() {},
            stat() { throw new Error("not found"); },
            open(name, mode) {
                assert.strictEqual(mode, "w+");
                const stream = { closed: false };
                streams.push(stream);
                return stream;
            },
            write(stream) { assert.strictEqual(stream.closed, false); },
            close(stream) {
                assert.strictEqual(stream.closed, false, "each stream must be closed only once");
                stream.closed = true;
                if (alwaysFails || streams.length === 1) throw closeError;
            }
        };
        loader.downloadContent();
        await waitFor(() => requests.length > 0);
        for (let attempt = 0; attempt <= loader.MAX_VERIFICATION_RETRIES; ++attempt) {
            requests[attempt].complete(Uint8Array.of(1).buffer);
            await waitFor(() => timers.size === 1 || calls.completed === 1 || calls.errors.length === 1);
            if (timers.size === 0) break;
            assert.deepStrictEqual(calls.loaded, []);
            assert.deepStrictEqual(calls.errors, []);
            assert.strictEqual(context.ProgressUpdater.current, 0);
            await runRetry();
        }
        assert.strictEqual(requests.length, alwaysFails ? loader.MAX_VERIFICATION_RETRIES + 1 : 2);
        assert.strictEqual(streams.length, requests.length);
        assert(streams.every(stream => stream.closed));
        assert.deepStrictEqual(calls.loaded, alwaysFails ? [] : ["a"]);
        assert.deepStrictEqual(calls.errors, alwaysFails ? [String(closeError)] : []);
        assert.strictEqual(calls.completed, alwaysFails ? 0 : 1);
        assert.strictEqual(timers.size, 0);
    }
}

// A terminal download error must still be reported once and cancel peers when closing the discarded DMFS stream fails.
async function testLoaderArchiveDMFSErrorCleanup() {
    const { context, loader, requests, calls, timers, runRetry, waitFor } = setupArchiveDownloads([archiveFile("a", 3)]);
    let closeCalls = 0;
    context.Module.isDMFSSupported = true;
    context.DMSYS = { GetUserPersistentDataRoot: () => "/data" };
    context.FileLoader.options.retryCount = 1;
    context.FS = {
        unlink() {},
        stat() { throw new Error("not found"); },
        open() { return {}; },
        close() { ++closeCalls; throw new Error("stream close failed"); }
    };
    loader.downloadContent();
    await waitFor(() => requests.length > 0);
    requests[0].fail();
    await runRetry();
    requests[1].fail();
    await waitFor(() => timers.size === 1);
    assert.strictEqual(timers.size, 1, "a peer has a pending transport retry");
    assert.doesNotThrow(() => requests[3].fail());
    await waitFor(() => calls.errors.length === 1);
    assert.strictEqual(requests[2].aborted, true, "the active peer must be cancelled");
    assert.strictEqual(timers.size, 0, "the peer's retry must be cancelled");
    assert.deepStrictEqual(calls.errors, ["Error loading 'split/a0?dmretry=1' ([object Object])"]);
    loader.notifyFileDownloadError("late error");
    assert.strictEqual(calls.errors.length, 1);
    assert.deepStrictEqual(calls.loaded, []);
    assert.strictEqual(calls.completed, 0);
    loader.cleanUp();
    assert.strictEqual(closeCalls, 1, "cleanup must not close the failed stream again");
}

// Cancelling downloaded-file hashing must close its stream without closing the replacement file's stream on settlement.
async function testLoaderArchiveDMFSVerificationCancellation() {
    for (const rejects of [false, true]) {
        const file = archiveFile("a");
        file.sha1 = "00";
        const { context, loader, requests, calls, timers, waitFor } = setupArchiveDownloads([file]);
        const digest = deferred();
        const streams = [];
        context.Module.isDMFSSupported = true;
        context.DMSYS = { GetUserPersistentDataRoot: () => "/data" };
        context.window.crypto = { subtle: { digest: () => digest.promise } };
        context.FS = {
            unlink() {},
            stat() { throw new Error("not found"); },
            open() { const stream = { closed: false }; streams.push(stream); return stream; },
            mmap() { return Uint8Array.of(1); },
            write(stream) { assert.strictEqual(stream.closed, false); },
            close(stream) { assert.strictEqual(stream.closed, false); stream.closed = true; }
        };
        const verifications = observeAsyncMethod(loader, "verifyFile");
        loader.downloadContent();
        await waitFor(() => requests.length > 0);
        requests[0].complete(Uint8Array.of(1).buffer);
        await verifications.started;
        loader.loadArchiveDescription("/new.json");
        assert.strictEqual(streams[0].closed, true);
        requests[1].complete(JSON.stringify({ total_size: 1, content: [archiveFile("a")] }));
        context.Module.postRun[0]();
        await waitFor(() => requests.length === 3);
        if (rejects) {
            digest.reject(new Error("stale hash failure"));
            await assert.rejects(verifications[0], /stale hash failure/);
        } else {
            digest.resolve(Uint8Array.of(0).buffer);
            await verifications[0];
        }
        assert.strictEqual(streams.length, 2);
        assert.strictEqual(streams[1].closed, false);
        assert.deepStrictEqual(calls.loaded, []);
        assert.strictEqual(timers.size, 0);
        requests[2].complete(Uint8Array.of(2).buffer);
        await waitFor(() => calls.completed === 1);
        assert(streams.every(stream => stream.closed));
        assert.deepStrictEqual(calls.errors, []);
    }
}

// A terminal piece error cancels its peers; malformed manifests prevent archive downloads while an empty archive completes.
async function testLoaderArchiveTerminalStates() {
    const { context, loader, requests, calls, waitFor } = setupArchiveDownloads([archiveFile("a", 2)]);
    context.FileLoader.options.retryCount = 0;
    loader.downloadContent();
    await waitFor(() => requests.length > 0);
    requests[0].fail();
    await waitFor(() => calls.errors.length === 1);
    assert.strictEqual(requests[1].aborted, true);
    assert.strictEqual(calls.errors.length, 1);
    assert.strictEqual(calls.completed, 0);

    for (const description of [{}, { total_size: 1, content: [{ name: "a", size: 1 }] }]) {
        const invalid = setupArchiveDownloads();
        invalid.loader.loadArchiveDescription("/invalid.json");
        invalid.requests[0].complete(JSON.stringify(description));
        await invalid.waitFor(() => invalid.calls.errors.length === 1);
        assert.strictEqual(invalid.calls.errors.length, 1);
        assert.strictEqual(invalid.calls.engines, 1);
        assert.strictEqual(invalid.requests.length, 1);
        assert.strictEqual(invalid.calls.completed, 0);
    }
    const empty = setupArchiveDownloads();
    empty.loader.loadArchiveDescription("/empty.json");
    empty.requests[0].complete('{"total_size":0,"content":[]}');
    await empty.waitFor(() => empty.calls.completed === 1);
    assert.deepStrictEqual(empty.calls.errors, []);
}

// All XHR failures obey the retry budget; intentional cancellation releases handlers without retrying or reporting errors.
async function testLoaderTransportRetryLifecycle() {
    for (const failure of ["error", "timeout", "abort", "status", "verification"]) {
        const { context, requests, timers, calls, runRetry } = setupArchiveDownloads();
        let loaded = 0, progress = 0;
        const request = context.FileLoader.load("/piece", "arraybuffer", delta => progress += delta,
            error => calls.errors.push(error), () => ++loaded, delta => progress -= delta,
            () => failure === "verification" ? "bad response" : undefined);
        for (let attempt = 0; attempt < 3; ++attempt) {
            const xhr = requests[attempt];
            if (attempt === 0 && ["error", "timeout", "abort"].includes(failure)) xhr.progress(2);
            if (failure === "abort") xhr.abort();
            else if (failure === "error" || failure === "timeout") xhr.fail(failure);
            else xhr.complete(new ArrayBuffer(0), failure === "status" ? 503 : 200);
            if (attempt < 2) await runRetry();
        }
        await request.promise;
        assert.strictEqual(requests.length, 3);
        assert.strictEqual(calls.errors.length, 1);
        assert.strictEqual(loaded, 0);
        assert.strictEqual(progress, 0);
        assert.strictEqual(timers.size, 0);
        request.cancel();
    }
    for (const retryPending of [true, false]) {
        const { context, requests, timers } = setupArchiveDownloads();
        let loaded = 0, errors = 0;
        const request = context.FileLoader.load("/piece", "arraybuffer", () => {}, () => ++errors, () => ++loaded, () => {});
        if (retryPending) requests[0].fail();
        request.cancel();
        await request.promise;
        assert.strictEqual(requests.length, 1);
        assert.strictEqual(timers.size, 0);
        assert.strictEqual(loaded, 0);
        assert.strictEqual(errors, 0);
        if (!retryPending) assert.strictEqual(requests[0].aborted, true);
        for (const event of ["load", "error", "timeout", "abort", "progress"]) {
            assert.strictEqual(requests[0]["on" + event], null, "cancelled requests release every handler");
        }
    }
    const { context, requests, timers, calls, runRetry } = setupArchiveDownloads();
    let loaded = 0;
    const request = context.FileLoader.load("/piece", "arraybuffer", () => {},
        error => calls.errors.push(error), () => ++loaded, () => {});
    requests[0].fail();
    await runRetry();
    requests[1].complete(Uint8Array.of(1).buffer);
    await request.promise;
    request.cancel();
    assert.strictEqual(loaded, 1, "a successful retry must complete only once");
    assert.strictEqual(requests.length, 2);
    assert.strictEqual(timers.size, 0);
    assert.deepStrictEqual(calls.errors, []);
    assert.strictEqual(requests[1].aborted, undefined, "completed requests must not be aborted");
}

// Legacy request callbacks retain progress, explicit response retries, cancellation, and HEAD size queries.
async function testLoaderTransportAdapters() {
    const { context, requests, timers, runRetry } = setupArchiveDownloads();
    const retries = [], progress = [], responses = [];
    const request = context.FileLoader.request(attempt => "/signed?attempt=" + attempt, "GET", "text");
    request.onprogress = (xhr, event, previous) => progress.push(event.loaded - previous);
    request.onretry = (xhr, event, loaded, attempt) => retries.push([loaded, attempt]);
    request.onerror = () => assert.fail("unexpected transport error");
    request.onload = xhr => {
        if (xhr.status !== 200) assert.strictEqual(request.retry(xhr, {}), true);
        else responses.push(xhr.response);
    };
    request.send();
    requests[0].complete("bad", 503);
    await runRetry();
    requests[1].complete("good");
    await request.promise;
    request.cancel();
    assert.deepStrictEqual(requests.map(xhr => xhr.url), ["/signed?attempt=0", "/signed?attempt=1"]);
    assert.deepStrictEqual(retries, [[3, 0]]);
    assert.deepStrictEqual(progress, [3, 4]);
    assert.deepStrictEqual(responses, ["good"]);
    assert.strictEqual(requests[1].aborted, undefined);

    const size = deferred();
    context.FileLoader.size("/size", size.resolve);
    assert.strictEqual(requests[2].method, "HEAD");
    requests[2].getResponseHeader = name => name === "content-length" ? "42" : null;
    requests[2].complete("");
    assert.strictEqual(await size.promise, "42");
    const missingSize = deferred();
    context.FileLoader.size("/missing", missingSize.resolve);
    requests[3].complete("", 404);
    assert.strictEqual(await missingSize.promise, undefined);
    assert.strictEqual(timers.size, 0, "HEAD status failures do not retry");

    const callbackFailure = context.FileLoader.request("/data", "GET", "text");
    let errors = 0;
    callbackFailure.onload = () => { throw new Error("callback failed"); };
    callbackFailure.onerror = () => ++errors;
    callbackFailure.send();
    requests[4].complete("data");
    await callbackFailure.promise;
    assert.strictEqual(errors, 1);
    assert.strictEqual(timers.size, 0, "callback exceptions must not retry a successful request");
}

// Cache reads always complete asynchronously; corrupt or unreadable cached files are downloaded again (#10116).
async function testLoaderArchiveCacheResults() {
    const bytes = Uint8Array.of(7);
    for (const cached of ["valid", "corrupt", "unreadable"]) {
        const file = archiveFile("a");
        file.sha1 = crypto.createHash("sha1").update(bytes).digest("hex");
        const { context, loader, requests, waitFor } = setupArchiveDownloads([file]);
        const miss = context.ArchiveStorage.readCached(file, context.createLoaderScope());
        assert.strictEqual(typeof miss.then, "function");
        assert.strictEqual(await miss, false);
        const streams = [];
        context.Module.isDMFSSupported = true;
        context.window.crypto = crypto.webcrypto;
        context.DMSYS = { GetUserPersistentDataRoot: () => "/data" };
        context.FS = {
            stat: () => ({ size: 1 }),
            open(name, mode) { const stream = { mode, closed: false }; streams.push(stream); return stream; },
            mmap(stream) { return stream.mode === "r" ? { valid: bytes, corrupt: Uint8Array.of(8), unreadable: null }[cached] : bytes; },
            write() {}, unlink() {},
            close(stream) { assert.strictEqual(stream.closed, false); stream.closed = true; }
        };
        const completion = loader.downloadContent();
        if (cached !== "valid") {
            await waitFor(() => requests.length === 1);
            requests[0].complete(bytes.buffer);
        }
        await completion;
        assert.strictEqual(requests.length, cached === "valid" ? 0 : 1);
        assert.strictEqual(streams.length, cached === "valid" ? 1 : 2);
        assert(streams.every(stream => stream.closed));
    }
}

// Manifest verification hashes the original UTF-8 bytes, not the JSON string or a reserialized object (#9911).
async function testLoaderArchiveDescriptionHash() {
    const manifest = '{ "total_size": 1, "content": [' + JSON.stringify(archiveFile("caf\u00e9-\ud83d\ude00")) + '] }\n';
    for (const valid of [true, false]) {
        const { context, loader, requests } = setupArchiveDownloads();
        context.TextEncoder = TextEncoder;
        context.window.crypto = crypto.webcrypto;
        context.EngineLoader.arc_sha1 = crypto.createHash("sha1").update(valid ? manifest : "different manifest").digest("hex");
        const completion = loader.loadDescription("/archive_files.json", context.createLoaderScope());
        requests[0].complete(manifest);
        if (valid) {
            const parsed = await completion;
            assert.strictEqual(JSON.stringify(parsed), JSON.stringify(JSON.parse(manifest)));
        } else {
            await assert.rejects(completion, /Unexpected hash/);
        }
        assert.strictEqual(requests.length, 1, "hash failure must not start archive downloads");
    }
}

// Invalid engine hashes must reject before script insertion or WASM instantiation.
async function testLoaderEngineHashFailure() {
    for (const kind of ["script", "wasm"]) {
        const { context, requests } = setupArchiveDownloads();
        context.TextEncoder = TextEncoder;
        context.window.crypto = crypto.webcrypto;
        let executed = 0;
        context.document.createElement = () => ({});
        context.document.body = { appendChild() { ++executed; } };
        context.WebAssembly = { instantiate: async () => { ++executed; return { instance: {}, module: {} }; } };
        let completion;
        if (kind === "script") {
            completion = context.EngineLoader.loadAndRunScriptAsync({ url: "/engine.js", size: 1, sha1: "00" });
            assert.strictEqual(requests[0].responseType, "arraybuffer");
            requests[0].complete(Uint8Array.of(120).buffer);
        } else {
            context.EngineLoader.wasm_sha1 = "00";
            context.EngineLoader.wasm_size = 8;
            completion = context.EngineLoader.loadAndInstantiateWasmAsync(context.EngineLoader.selectEngine("/engine").wasm, {});
            requests[0].complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
        }
        await assert.rejects(completion, /Unexpected hash/);
        assert.strictEqual(executed, 0, kind + " must not execute after a hash mismatch");
    }
}

// Verify script bytes directly for every size/SHA setting without decoding or re-encoding non-ASCII text.
async function testLoaderScriptBytes() {
    const script = "// caf\u00e9 \ud83d\ude00";
    for (const verifySize of [true, false]) {
        for (const verifyHash of [true, false]) {
            const { context, requests } = setupArchiveDownloads([], verifySize);
            const inserted = deferred();
            let encodings = 0;
            context.TextEncoder = class {
                encode(text) { ++encodings; return new TextEncoder().encode(text); }
            };
            context.window.crypto = crypto.webcrypto;
            context.document.createElement = () => ({ remove() {} });
            context.document.body = { appendChild: inserted.resolve };
            const completion = context.EngineLoader.loadAndRunScriptAsync({
                url: "/engine.js", size: Buffer.byteLength(script),
                sha1: verifyHash ? crypto.createHash("sha1").update(script).digest("hex") : ""
            });
            assert.strictEqual(requests[0].responseType, "arraybuffer");
            requests[0].complete(new TextEncoder().encode(script).buffer);
            const element = await inserted.promise;
            element.onload();
            await completion;
            assert.strictEqual(encodings, 0);
            assert.strictEqual(element.src, "/engine.js");
        }
    }
}

// Reject truncated/oversized engine downloads before execution (fac83f2a90), preserving normal script URLs for CSP (#10825).
async function testLoaderEngineSizeRetry() {
    for (const kind of ["script", "wasm"]) {
        const { context, requests, runRetry } = setupArchiveDownloads();
        const bytes = kind === "script" ? Uint8Array.of(120) : Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0);
        const inserted = deferred();
        let executed = 0;
        context.document.createElement = () => ({ remove() {} });
        context.document.body = { appendChild(script) { ++executed; inserted.resolve(script); } };
        context.WebAssembly = { instantiate(data, imports) { ++executed; return WebAssembly.instantiate(data, imports); } };
        const completion = kind === "script"
            ? context.EngineLoader.loadAndRunScriptAsync({ url: "/engine.js", size: bytes.length })
            : context.EngineLoader.loadAndInstantiateWasmAsync({ url: "/engine.wasm", size: bytes.length }, {});
        for (const [attempt, length] of [bytes.length - 1, bytes.length + 1].entries()) {
            assert.strictEqual(requests[attempt].responseType, "arraybuffer");
            requests[attempt].complete(new ArrayBuffer(length));
            await Promise.race([runRetry(), completion]);
            assert.strictEqual(executed, 0, "unverified bytes must not execute");
        }
        assert.strictEqual(requests[2].responseType, "arraybuffer");
        requests[2].complete(bytes.buffer);
        if (kind === "script") {
            const script = await inserted.promise;
            assert.strictEqual(script.src, "/engine.js?dmretry=2");
            assert.strictEqual(context.Module.mainScriptUrlOrBlob, script.src);
            script.onload();
        }
        await completion;
        assert.strictEqual(executed, 1);
        assert.strictEqual(context.ProgressUpdater.current, bytes.length);
    }
}

// Streaming uses real WASM compilation and falls back after HTTP/network/MIME failures (#8336, d97392d511).
async function testLoaderStreamingWasm() {
    const wasm = Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0);
    for (const verifySize of [true, false]) {
        for (const scenario of ["success", "mime", "status", "network", "size", "hash", "no-streams"]) {
            const { context, requests, waitFor } = setupArchiveDownloads([], verifySize);
            Object.assign(context, { WebAssembly, Response, ReadableStream, TransformStream, AbortController });
            context.window.crypto = crypto.webcrypto;
            context.EngineLoader.wasm_size = wasm.length;
            context.EngineLoader.wasm_sha1 = scenario === "hash" ? crypto.createHash("sha1").update(wasm).digest("hex") : "";
            if (scenario === "no-streams") context.TransformStream = undefined;
            const progress = [];
            context.ProgressUpdater.addListener(value => progress.push(value));
            let fetched = 0, instantiated = 0;
            let fetchSignal;
            context.fetch = async (url, { signal }) => {
                fetchSignal = signal;
                ++fetched;
                if (scenario === "network") throw new TypeError("Failed to fetch");
                // A valid extra custom section makes the module larger than the manifest.
                const bytes = scenario === "size" ? Uint8Array.from([...wasm, 0, 1, 0]) : wasm;
                return new Response(bytes, {
                    status: scenario === "status" ? 503 : 200,
                    headers: { "Content-Type": scenario === "mime" ? "text/plain" : "application/wasm" }
                });
            };
            const asset = context.EngineLoader.selectEngine("game").wasm;
            asset.instantiateProgress = 2;
            const completion = context.EngineLoader.streamAndInstantiateWasmAsync(asset, {}).then(output => {
                assert(output.instance instanceof WebAssembly.Instance);
                ++instantiated;
            });
            const fallback = ["mime", "status", "network", "hash"].includes(scenario) ||
                (verifySize && ["size", "no-streams"].includes(scenario));
            if (fallback) {
                await Promise.race([waitFor(() => requests.length === 1), completion]);
                if (fetchSignal) assert.strictEqual(fetchSignal.aborted, true, "release streaming before the fallback completes");
                if (scenario === "status" || scenario === "network") {
                    assert(progress.every(value => value === 0), "failed fetches must not count as downloaded WASM");
                }
                requests[0].complete(wasm.buffer);
            }
            await completion;
            if (fetchSignal) assert.strictEqual(fetchSignal.aborted, true, "release streaming after completion");
            assert.strictEqual(instantiated, 1, scenario);
            assert.strictEqual(requests.length, fallback ? 1 : 0, scenario);
            assert.strictEqual(fetched, scenario === "hash" || (verifySize && scenario === "no-streams") ? 0 : 1);
            const streamed = scenario === "no-streams" ? 0 : scenario === "size" ? 11 : 8;
            assert.strictEqual(context.ProgressUpdater.current, fallback ? 8 : streamed + 2, scenario);
        }
    }
}

// Missing or compressed Content-Length must not produce NaN or inflated streaming progress (#7983/#5211).
async function testLoaderStreamingProgressWithoutLength() {
    const wasm = Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0);
    for (const contentLength of [undefined, "2"]) {
        const { context, requests } = setupArchiveDownloads();
        Object.assign(context, { WebAssembly, Response, ReadableStream, TransformStream, AbortController });
        const percentages = [];
        context.ProgressUpdater.setupTotal(wasm.length);
        context.ProgressUpdater.addListener(value => percentages.push(value));
        context.fetch = async () => {
            const headers = { "Content-Type": "application/wasm" };
            if (contentLength) headers["Content-Length"] = contentLength;
            return new Response(new ReadableStream({ start(controller) {
                controller.enqueue(wasm.subarray(0, 3));
                controller.enqueue(wasm.subarray(3));
                controller.close();
            } }), { headers });
        };
        const output = await context.EngineLoader.streamAndInstantiateWasmAsync({ url: "game.wasm", size: wasm.length }, {});
        assert(output.instance instanceof WebAssembly.Instance);
        assert.strictEqual(requests.length, 0);
        assert.strictEqual(percentages[0], 37.5, "progress must advance while chunks arrive");
        assert.strictEqual(percentages[percentages.length - 1], 100);
        assert(percentages.every(value => Number.isFinite(value) && value >= 0 && value <= 100));
    }
}

// Throwing progress listeners must not break WASM streaming, its fallback rollback, or later listeners.
async function testLoaderStreamingProgressErrors() {
    const wasm = Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0);
    for (const fallback of [false, true]) {
        const { context, requests, waitFor, calls } = setupArchiveDownloads();
        Object.assign(context, { WebAssembly, Response, ReadableStream, TransformStream, AbortController });
        const progressError = new Error("progress listener failed");
        const percentages = [];
        context.ProgressUpdater.setupTotal(wasm.length);
        context.ProgressUpdater.addListener(() => { throw progressError; });
        context.ProgressUpdater.addListener(value => percentages.push(value));
        context.fetch = async () => new Response(fallback ? Uint8Array.from([...wasm, 0, 1, 0]) : wasm,
            { headers: { "Content-Type": "application/wasm" } });
        const completion = context.EngineLoader.streamAndInstantiateWasmAsync({ url: "game.wasm", size: wasm.length }, {});
        if (fallback) {
            await waitFor(() => requests.length === 1);
            assert.strictEqual(context.ProgressUpdater.current, 0, "discarded stream bytes must be rolled back");
            requests[0].complete(wasm.buffer);
        }
        const output = await completion;
        assert(output.instance instanceof WebAssembly.Instance);
        assert.strictEqual(requests.length, fallback ? 1 : 0);
        assert.strictEqual(context.ProgressUpdater.current, wasm.length);
        assert.strictEqual(percentages[percentages.length - 1], 100);
        assert(calls.consoleErrors.length > 0);
        assert(calls.consoleErrors.every(error => error === progressError));
    }
}

// Cancelling a streaming request aborts fetch and must not initiate the XHR fallback.
async function testLoaderStreamingCancellation() {
    const { context, requests } = setupArchiveDownloads();
    Object.assign(context, { WebAssembly, Response, ReadableStream, TransformStream, AbortController });
    context.EngineLoader.wasm_sha1 = "";
    let aborted = false, instantiated = false;
    context.fetch = (url, { signal }) => new Promise((resolve, reject) => {
        signal.addEventListener("abort", () => { aborted = true; reject(signal.reason); });
    });
    const scope = context.createLoaderScope();
    const completion = context.EngineLoader.streamAndInstantiateWasmAsync(context.EngineLoader.selectEngine("game").wasm, {}, scope).then(() => instantiated = true);
    scope.cancel();
    await assert.rejects(completion, { name: "AbortError" });
    assert.strictEqual(aborted, true);
    assert.strictEqual(instantiated, false);
    assert.strictEqual(requests.length, 0);
}

// A storage write failure must close the stream, cancel peer downloads, and report one terminal error.
async function testLoaderArchiveWriteFailure() {
    const { context, loader, requests, calls, waitFor } = setupArchiveDownloads([archiveFile("a", 2)]);
    let closes = 0;
    context.Module.isDMFSSupported = true;
    context.DMSYS = { GetUserPersistentDataRoot: () => "/data" };
    context.FS = {
        unlink() {},
        stat() { throw new Error("not found"); },
        open() { return {}; },
        write() { throw new Error("disk full"); },
        close() { ++closes; }
    };
    loader.downloadContent();
    await waitFor(() => requests.length > 0);
    assert.doesNotThrow(() => requests[0].complete(Uint8Array.of(1).buffer));
    await waitFor(() => calls.errors.length === 1);
    assert.strictEqual(calls.errors.length, 1);
    assert.strictEqual(closes, 1);
    assert.strictEqual(requests[1].aborted, true);
    assert.strictEqual(calls.completed, 0);
}

// Replacing an archive must discard files already staged by the previous archive.
async function testLoaderArchiveReplacementDropsFiles() {
    const { context, loader, requests, calls, waitFor } = setupArchiveDownloads([archiveFile("old"), archiveFile("remaining")]);
    loader.downloadContent();
    await waitFor(() => requests.length > 0);
    requests[0].complete(Uint8Array.of(1).buffer);
    await waitFor(() => calls.loaded.length === 1);
    await waitFor(() => requests.length === 2);
    loader.loadArchiveDescription("/new.json");
    requests[2].complete(JSON.stringify({ total_size: 1, content: [archiveFile("new")] }));
    await waitFor(() => requests.length === 4);
    requests[3].complete(Uint8Array.of(2).buffer);
    await waitFor(() => calls.completed === 1);
    assert.deepStrictEqual(Array.from(context.Module._filesToPreload, file => file.path), ["new"]);
}

// Download completion includes all files and verification; cancellation settles even if hashing never finishes.
async function testLoaderArchiveCompletionPromise() {
    for (const cancel of [false, true]) {
        const file = archiveFile("a");
        file.sha1 = "00";
        const { context, loader, requests, calls, waitFor } = setupArchiveDownloads([file]);
        const digest = deferred();
        context.window.crypto = { subtle: { digest: () => digest.promise } };
        const verifications = observeAsyncMethod(loader, "verifyFile");
        const completion = loader.downloadContent();
        await waitFor(() => requests.length > 0);
        let settled = false;
        completion.then(() => settled = true, () => settled = true);
        requests[0].complete(Uint8Array.of(1).buffer);
        await verifications.started;
        assert.strictEqual(settled, false);
        if (cancel) {
            loader.cleanUp();
            await assert.rejects(completion, { name: "AbortError" });
            assert.strictEqual(calls.completed, 0);
        } else {
            digest.resolve(Uint8Array.of(0).buffer);
            await completion;
            assert.strictEqual(calls.completed, 1);
        }
        assert.strictEqual(settled, true);
        assert.deepStrictEqual(calls.errors, []);
    }
}

// Cancelling the promise transport settles active requests and retry waits without another request or callback.
async function testLoaderFetchCancellation() {
    for (const retryPending of [false, true]) {
        const { context, requests, timers } = setupArchiveDownloads();
        const scope = context.createLoaderScope();
        const completion = context.FileLoader.fetch("/data", { responseType: "arraybuffer" }, scope);
        if (retryPending) requests[0].fail();
        scope.cancel();
        await assert.rejects(completion, { name: "AbortError" });
        assert.strictEqual(requests.length, 1);
        assert.strictEqual(timers.size, 0);
        if (!retryPending) assert.strictEqual(requests[0].aborted, true);
    }
}

// Progress callback failures must preserve byte deltas, retry rollback, and successful request completion.
async function testLoaderFetchProgressErrors() {
    const { context, requests, runRetry, calls } = setupArchiveDownloads();
    const deltas = [];
    const progressError = new Error("progress widget failed");
    const completion = context.FileLoader.fetch("/data", {
        responseType: "arraybuffer",
        progress(delta) { deltas.push(delta); throw progressError; }
    }, context.createLoaderScope());
    requests[0].progress(2);
    requests[0].progress(5);
    assert.strictEqual(requests[0].pending, true);
    assert(!requests[0].aborted);
    requests[0].fail();
    await runRetry();
    requests[1].progress(1);
    const bytes = Uint8Array.of(1, 2, 3).buffer;
    requests[1].complete(bytes);
    const result = await completion;
    assert.strictEqual(result.response, bytes);
    assert.deepStrictEqual(deltas, [2, 3, -5, 1, 2]);
    assert.strictEqual(calls.consoleErrors.length, deltas.length);
    assert(calls.consoleErrors.every(error => error === progressError));
    assert(requests.every(xhr => !xhr.aborted));
}

// Synchronous request setup and validation failures must settle the promise transport.
async function testLoaderFetchExceptions() {
    for (const phase of ["open", "validation"]) {
        const { context, requests } = setupArchiveDownloads();
        const scope = context.createLoaderScope();
        context.FileLoader.options.retryCount = 0;
        if (phase === "open") context.XMLHttpRequest.prototype.open = () => { throw new Error("open failed"); };
        const completion = context.FileLoader.fetch("/data", {
            responseType: "arraybuffer",
            verify() { if (phase === "validation") throw new Error("validation failed"); }
        }, scope);
        if (phase !== "open") assert.doesNotThrow(() => requests[0].complete(new ArrayBuffer(0)));
        await assert.rejects(completion, new RegExp(phase + " failed"));
    }
    const { context, requests, timers, waitFor } = setupArchiveDownloads();
    const completion = context.FileLoader.fetch(attempt => {
        if (attempt) throw new Error("retry URL failed");
        return "/data";
    }, { responseType: "arraybuffer" }, context.createLoaderScope());
    requests[0].fail();
    await waitFor(() => timers.size === 1);
    const [id, retry] = timers.entries().next().value;
    timers.delete(id);
    assert.doesNotThrow(retry);
    await assert.rejects(completion, /retry URL failed/);
    assert.strictEqual(requests.length, 1);
    assert.strictEqual(timers.size, 0);
}

// An incomplete DMFS file must be removed so a later session cannot mistake its length for a valid cache entry.
async function testLoaderArchiveDiscardedStorage() {
    const { context, loader, requests, waitFor } = setupArchiveDownloads([archiveFile("a", 2)]);
    const written = deferred();
    let persisted = null, closed = 0, removed = 0;
    context.Module.isDMFSSupported = true;
    context.DMSYS = { GetUserPersistentDataRoot: () => "/data" };
    context.FS = {
        stat() { if (persisted) return { size: persisted.length }; throw new Error("not found"); },
        open() { persisted = new Uint8Array(2); return {}; },
        write(stream, data, offset, length, position) { persisted.set(data, position); written.resolve(); },
        close() { ++closed; },
        unlink() { persisted = null; ++removed; }
    };
    const completion = loader.downloadContent();
    await waitFor(() => requests.length === 2);
    requests[1].complete(Uint8Array.of(2).buffer);
    await written.promise;
    loader.cleanUp();
    await assert.rejects(completion, { name: "AbortError" });
    assert.strictEqual(persisted, null);
    assert.strictEqual(closed, 1);
    assert.strictEqual(removed, 1);
    loader._files = [archiveFile("a", 2)];
    loader.downloadContent();
    await waitFor(() => requests.length > 0);
    await waitFor(() => requests.length === 4);
    assert.strictEqual(requests[2].url, "split/a0");
    loader.cleanUp();
}

function setupEngineStartup() {
    const state = setupArchiveDownloads([], true, true);
    const { context, calls, notify, requests } = state;
    const scripts = [], files = new Map();
    const runtime = deferred();
    let restore;
    calls.main = 0;
    calls.success = 0;
    context.TextEncoder = TextEncoder;
    context.window.crypto = crypto.webcrypto;
    context.Module.persistentStorage = true;
    context.Module.setupCanvas = () => context.Module.canvas = { focus() {}, addEventListener() {} };
    context.Module.hasWebGLSupport = () => true;
    context.Module.callMain = () => { ++calls.main; notify(); };
    context.CUSTOM_PARAMETERS.start_success = () => { ++calls.success; notify(); };
    context.CUSTOM_PARAMETERS.full_screen_container = null;
    context.CUSTOM_PARAMETERS.resize_window_callback = null;
    context.CUSTOM_PARAMETERS.retry_count = 0;
    context.CUSTOM_PARAMETERS.archive_location_filter = path => "split" + path;
    context.ProgressView.addProgress = () => {};
    context.ProgressView.removeProgress = () => {};
    context.EngineLoader.wasmjs_size = 1;
    context.EngineLoader.wasmjs_sha1 = "";
    context.EngineLoader.wasm_size = 8;
    context.EngineLoader.wasm_sha1 = "";
    context.document.createElement = () => ({ remove() { this.removed = true; } });
    context.document.body = { appendChild(script) { scripts.push(script); notify(); } };
    context.DMSYS = { GetUserPersistentDataRoot: () => "/data" };
    context.IDBFS = {};
    context.FS = {
        mkdir() {}, mount() {}, close() {},
        syncfs(populate, callback) { assert(populate); restore = callback; },
        createPreloadedFile(parent, name, data) { files.set(name, Array.from(data)); }
    };
    context.WebAssembly = { instantiate: async () => ({ instance: {}, module: {} }) };
    const request = url => {
        const xhr = requests.find(xhr => xhr.pending && xhr.url === url);
        assert(xhr, "missing request for " + url);
        return xhr;
    };
    function runScript(imports = {}) {
        context.Module.preInit[0]();
        context.Module.instantiateWasm(imports, () => {
            context.Module.onRuntimeInitialized();
            context.Module.postRun[0]();
            runtime.resolve();
        });
        scripts[0].onload({});
    }
    return { ...state, scripts, files, request, runScript, runtime, restore: () => restore(null) };
}

// Progress view and custom callback failures must not stop startup or suppress the final update to other listeners.
async function testLoaderStartupProgressErrors() {
    const { context, scripts, request, runScript, restore, waitFor, calls } = setupEngineStartup();
    const viewError = new Error("progress view failed");
    const callbackError = new Error("custom progress callback failed");
    const percentages = [];
    context.ProgressView.updateProgress = () => { throw viewError; };
    context.CUSTOM_PARAMETERS.update_progress = () => { throw callbackError; };
    const completion = context.EngineLoader.load("canvas", "game");
    context.ProgressUpdater.addListener(value => percentages.push(value));
    request("split/archive_files.json").complete('{"total_size":0,"content":[]}');
    request("game_wasm.js").complete(Uint8Array.of(120).buffer);
    await waitFor(() => scripts.length === 1);
    runScript();
    restore();
    request("game.wasm").complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
    await completion;
    assert.strictEqual(context.GameStartup._session.state, "started");
    assert.strictEqual(calls.main, 1);
    assert.strictEqual(percentages[percentages.length - 1], 100);
    assert(calls.consoleErrors.includes(viewError));
    assert(calls.consoleErrors.includes(callbackError));
    assert.deepStrictEqual(calls.errors, []);
}

// Unrelated or unattributed page errors must not cancel downloads or prevent a ready engine from starting.
async function testLoaderUnrelatedPageError() {
    for (const phase of ["downloading", "initializing"]) {
        for (const filename of ["https://example.com/analytics.js", undefined]) {
            const { context, requests, scripts, request, runScript, restore, waitFor, calls } = setupEngineStartup();
            const completion = context.EngineLoader.load("canvas", "game");
            const error = { type: "error", message: "unrelated page error", filename };
            if (phase === "downloading") context.window.dispatchEvent(error);
            request("split/archive_files.json").complete('{"total_size":0,"content":[]}');
            request("game_wasm.js").complete(Uint8Array.of(120).buffer);
            await waitFor(() => scripts.length === 1);
            runScript();
            if (phase === "initializing") context.window.dispatchEvent(error);
            assert.strictEqual(context.GameStartup._session.state, "loading");
            assert(requests.every(xhr => !xhr.aborted));
            assert.strictEqual(calls.errors.length, 1, "page errors still reach the custom error callback");
            restore();
            request("game.wasm").complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
            await completion;
            assert.strictEqual(context.GameStartup._session.state, "started");
            assert.strictEqual(calls.main, 1);
        }
    }
}

// Engine script errors must still reject startup, including absolute browser URLs after a download retry.
async function testLoaderEngineScriptError() {
    for (const retry of [false, true]) {
        const { context, scripts, request, runRetry, waitFor, calls } = setupEngineStartup();
        context.CUSTOM_PARAMETERS.retry_count = 1;
        context.document.createElement = () => ({
            set src(url) { this.url = new URL(url, "https://example.com/game/").href; },
            get src() { return this.url; },
            remove() { this.removed = true; }
        });
        const completion = context.EngineLoader.load("canvas", "game");
        const manifest = request("split/archive_files.json");
        const wasm = request("game.wasm");
        if (retry) {
            request("game_wasm.js").complete(new ArrayBuffer(0));
            await runRetry();
        }
        request("game_wasm.js" + (retry ? "?dmretry=1" : "")).complete(Uint8Array.of(120).buffer);
        await waitFor(() => scripts.length === 1);
        const error = { type: "error", filename: scripts[0].src, error: new SyntaxError("invalid engine script") };
        context.window.dispatchEvent(error);
        await assert.rejects(completion, reason => reason === error);
        assert.strictEqual(context.GameStartup._session.state, "failed");
        assert.strictEqual(manifest.aborted, true);
        assert.strictEqual(wasm.aborted, true);
        assert.strictEqual(scripts[0].removed, true);
        context.window.dispatchEvent(error);
        assert.strictEqual(calls.errors.length, 1, "the engine failure must be reported only once");
        assert.strictEqual(calls.consoleErrors.filter(value => value === error).length, 1);
        assert.strictEqual(calls.main, 0);
    }
}

// Engine startup overlaps a pending manifest, retains early progress, and still waits for archive data before main.
async function testLoaderStartupOverlapsManifest() {
    const { context, requests, scripts, request, runScript, runtime, restore, waitFor, calls } = setupEngineStartup();
    const completion = context.EngineLoader.load("canvas", "game");
    assert.strictEqual(requests.length, 3, "manifest, WASM, and script must start together");
    request("game_wasm.js").complete(Uint8Array.of(120).buffer);
    await waitFor(() => scripts.length === 1);
    runScript();
    restore();
    request("game.wasm").complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
    await runtime.promise;
    assert.strictEqual(calls.main, 0);
    assert.strictEqual(context.ProgressUpdater.current, 9);
    assert.strictEqual(context.ProgressUpdater.calculateProgress(), 0);
    request("split/archive_files.json").complete(JSON.stringify({ total_size: 1, content: [archiveFile("a")] }));
    await waitFor(() => requests.length === 4);
    assert.strictEqual(context.ProgressUpdater.current, 9, "manifest arrival must not reset downloaded engine bytes");
    assert(context.ProgressUpdater.calculateProgress() > 0);
    assert.strictEqual(calls.main, 0);
    request("split/a0").complete(Uint8Array.of(7).buffer);
    await completion;
    assert.strictEqual(calls.main, 1);
    assert.strictEqual(context.ProgressUpdater.calculateProgress(), 100);
    assert.deepStrictEqual(calls.errors, []);
}

// Either engine download can fail before the manifest arrives; both peers must be cancelled with one error.
async function testLoaderStartupEarlyEngineFailure() {
    for (const failed of ["game_wasm.js", "game.wasm"]) {
        const { context, requests, request, calls } = setupEngineStartup();
        const completion = context.EngineLoader.load("canvas", "game");
        request(failed).fail();
        await assert.rejects(completion);
        assert(requests.filter(xhr => xhr.url !== failed).every(xhr => xhr.aborted));
        assert.strictEqual(calls.errors.length, 1);
        assert.strictEqual(calls.main, 0);
    }
}

// WASM can finish downloading before the script, but instantiation must wait for its updated imports.
async function testLoaderStartupEarlyWasm() {
    const { context, requests, request, scripts, runScript, restore, waitFor, calls } = setupEngineStartup();
    const imports = { env: {} };
    let instantiated = 0;
    context.CUSTOM_PARAMETERS.update_imports = value => { value.env.updated = true; };
    context.WebAssembly.instantiate = async (bytes, value) => {
        ++instantiated;
        assert.strictEqual(value, imports);
        assert.strictEqual(value.env.updated, true);
        return { instance: {}, module: {} };
    };
    const verifications = observeAsyncMethod(context.FileLoader, "verifyHash");
    const completion = context.EngineLoader.load("canvas", "game");
    request("split/archive_files.json").complete('{"total_size":0,"content":[]}');
    request("game.wasm").complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
    await verifications.started;
    await verifications[0];
    assert.strictEqual(instantiated, 0);
    assert.strictEqual(scripts.length, 0);
    request("game_wasm.js").complete(Uint8Array.of(120).buffer);
    await waitFor(() => scripts.length === 1);
    runScript(imports);
    restore();
    await completion;
    assert.strictEqual(instantiated, 1);
    assert.strictEqual(requests.filter(xhr => xhr.url === "game.wasm").length, 1);
    assert.strictEqual(calls.main, 1);
}

// Early streaming fetches must reach the streaming compiler before the body finishes, with Emscripten's imports.
async function testLoaderStartupEarlyStreamingWasm() {
    const { context, request, requests, scripts, runScript, restore, waitFor, calls } = setupEngineStartup();
    Object.assign(context, { Response, ReadableStream, TransformStream, AbortController });
    context.EngineLoader.stream_wasm = true;
    const compiling = deferred(), firstChunk = deferred();
    const imports = { env: {} };
    let body, fetched = 0;
    context.fetch = async url => {
        ++fetched;
        assert.strictEqual(url, "game.wasm");
        return new Response(new ReadableStream({ start(controller) { body = controller; } }),
            { headers: { "Content-Type": "application/wasm" } });
    };
    context.WebAssembly = {
        instantiateStreaming(response, value) {
            assert.strictEqual(value, imports);
            compiling.resolve();
            return WebAssembly.instantiateStreaming(response, value);
        }
    };
    context.ProgressUpdater.addListener(() => {
        if (context.ProgressUpdater.current === 5) firstChunk.resolve();
    });
    const completion = context.EngineLoader.load("canvas", "game");
    assert.strictEqual(fetched, 1, "WASM fetch must start before the script download completes");
    request("split/archive_files.json").complete('{"total_size":0,"content":[]}');
    request("game_wasm.js").complete(Uint8Array.of(120).buffer);
    await waitFor(() => scripts.length === 1);
    runScript(imports);
    restore();
    await compiling.promise;
    body.enqueue(Uint8Array.of(0, 97, 115, 109));
    await firstChunk.promise;
    assert.strictEqual(calls.main, 0);
    body.enqueue(Uint8Array.of(1, 0, 0, 0));
    body.close();
    await completion;
    assert.strictEqual(requests.length, 2, "streaming must not fall back to an XHR");
    assert(context.Module.instance instanceof WebAssembly.Instance);
    assert.strictEqual(calls.main, 1);
}

// Script failure must abort an early response stream even when Emscripten has not supplied imports yet.
async function testLoaderStartupEarlyStreamCancellation() {
    const { context, request, requests, calls } = setupEngineStartup();
    Object.assign(context, { WebAssembly, Response, ReadableStream, TransformStream, AbortController });
    context.EngineLoader.stream_wasm = true;
    let aborted = false;
    context.fetch = async (url, { signal }) => new Response(new ReadableStream({
        start(controller) {
            controller.enqueue(Uint8Array.of(0, 97, 115, 109));
            signal.addEventListener("abort", () => {
                aborted = true;
                controller.error(new Error("aborted"));
            });
        }
    }), { headers: { "Content-Type": "application/wasm" } });
    const fetches = observeAsyncMethod(context, "fetch");
    const completion = context.EngineLoader.load("canvas", "game");
    await fetches[0];
    request("game_wasm.js").fail();
    await assert.rejects(completion);
    assert.strictEqual(aborted, true);
    assert.strictEqual(requests.length, 2, "cancelled streaming must not start an XHR fallback");
    assert.strictEqual(calls.errors.length, 1);
    assert.strictEqual(calls.main, 0);
}

// Startup must wait for the engine, archive, and restored storage in any order and invoke main exactly once.
async function testLoaderStartupReadiness() {
    const orders = [["engine", "archive", "storage"], ["engine", "storage", "archive"],
                    ["archive", "engine", "storage"], ["archive", "storage", "engine"],
                    ["storage", "engine", "archive"], ["storage", "archive", "engine"]];
    for (const order of orders) {
        const { context, calls, scripts, files, request, runScript, runtime, restore, waitFor } = setupEngineStartup();
        let initialized = 0;
        const onRuntimeInitialized = context.Module.onRuntimeInitialized;
        context.Module.onRuntimeInitialized = function() { ++initialized; onRuntimeInitialized(); };
        const completion = context.EngineLoader.load("canvas", "game");
        request("split/archive_files.json").complete(JSON.stringify({ total_size: 1, content: [archiveFile("a")] }));
        await waitFor(() => context.GameArchiveLoader._files.length === 1 && scripts.length === 0 && calls.maxPending === 3);
        request("game_wasm.js").complete(Uint8Array.of(120).buffer);
        await waitFor(() => scripts.length === 1);
        runScript();
        for (const [index, step] of order.entries()) {
            if (step === "engine") {
                request("game.wasm").complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
                await runtime.promise;
            } else if (step === "archive") {
                request("split/a0").complete(Uint8Array.of(7).buffer);
                await waitFor(() => calls.completed === 1);
            } else {
                restore();
            }
            if (index < 2) {
                assert.strictEqual(calls.main, 0, "startup order: " + order);
                assert.strictEqual(files.size, 0, "files must stay staged until startup is ready");
            }
        }
        await completion;
        context.GameStartup._session.runtime.resolve();
        context.GameStartup._session.storage.resolve();
        context.Module.postRun[0]();
        assert.strictEqual(calls.main, 1);
        assert.strictEqual(calls.success, 1);
        assert.strictEqual(initialized, 1, "custom runtime hooks must be preserved");
        assert.deepStrictEqual(files.get("a"), [7]);
        assert.deepStrictEqual(calls.errors, []);
    }
}

// Engine URLs, verification metadata, and locateFile must retain the variant selected when startup began.
async function testLoaderEngineSelection() {
    for (const pthread of [false, true]) {
        const { context, requests, scripts, request, runScript, restore, waitFor, calls } = setupEngineStartup();
        context.Module.isWASMPthreadSupported = pthread;
        context.EngineLoader.wasmjs_pthread_size = 1;
        context.EngineLoader.wasmjs_pthread_sha1 = "";
        context.EngineLoader.wasm_pthread_size = 8;
        context.EngineLoader.wasm_pthread_sha1 = "";
        const completion = context.EngineLoader.load("canvas", "game");
        context.Module.isWASMPthreadSupported = !pthread;
        context.EngineLoader.wasmjs_size = context.EngineLoader.wasmjs_pthread_size = 99;
        context.EngineLoader.wasm_size = context.EngineLoader.wasm_pthread_size = 99;
        context.EngineLoader.wasmjs_sha1 = context.EngineLoader.wasmjs_pthread_sha1 = "changed";
        context.EngineLoader.wasm_sha1 = context.EngineLoader.wasm_pthread_sha1 = "changed";
        const name = "game" + (pthread ? "_pthread" : "");
        assert.strictEqual(context.Module.locateFile("dmengine.wasm", "/assets/"), "/assets/" + name + ".wasm");
        request("split/archive_files.json").complete('{"total_size":0,"content":[]}');
        assert.strictEqual(requests.length, 3);
        request(name + "_wasm.js").complete(Uint8Array.of(120).buffer);
        await waitFor(() => scripts.length === 1);
        runScript();
        restore();
        request(name + ".wasm").complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
        await completion;
        assert.strictEqual(calls.main, 1);
        assert.deepStrictEqual(calls.errors, []);
    }
}

// DMFS downloads wait for the filesystem, while main also waits for persistent storage restoration.
async function testLoaderStartupDMFS() {
    const { context, requests, scripts, request, runScript, runtime, restore, waitFor, calls } = setupEngineStartup();
    context.Module.isDMFSSupported = true;
    context.DMFS = function() {};
    let closed = 0, written = 0;
    Object.assign(context.FS, {
        chdir() {}, stat() { throw new Error("not cached"); },
        open() { assert.strictEqual(context.Module._filesystemReady, true); return {}; },
        write() { ++written; }, close() { ++closed; }, unlink() {}
    });
    const completion = context.EngineLoader.load("canvas", "game");
    request("split/archive_files.json").complete(JSON.stringify({ total_size: 1, content: [archiveFile("a")] }));
    assert.strictEqual(requests.length, 3);
    request("game_wasm.js").complete(Uint8Array.of(120).buffer);
    await waitFor(() => scripts.length === 1);
    runScript();
    assert.strictEqual(requests.length, 3, "only the manifest, script, and WASM may be requested before FS initialization");
    request("game.wasm").complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
    await runtime.promise;
    await waitFor(() => requests.length === 4);
    request("split/a0").complete(Uint8Array.of(7).buffer);
    await waitFor(() => calls.completed === 1);
    assert.strictEqual(calls.main, 0);
    restore();
    await completion;
    assert.strictEqual(calls.main, 1);
    assert.strictEqual(written, 1);
    assert.strictEqual(closed, 1);
    assert.deepStrictEqual(calls.errors, []);
}

// Storage failures must not block startup (#7603/#8496), including disabled IndexedDB and exhausted restore retries.
async function testLoaderStartupStorageFallback() {
    for (const phase of ["mkdir", "mount", "restore", "disabled"]) {
        const { context, requests, scripts, request, runScript, waitFor, calls } = setupEngineStartup();
        context.console.info = () => {};
        let syncs = 0;
        context.FS.syncfs = (populate, callback) => { ++syncs; callback(new Error("storage unavailable")); };
        if (phase === "mkdir" || phase === "mount") context.FS[phase] = () => { throw new Error("storage unavailable"); };
        if (phase === "disabled") {
            context.Module.persistentStorage = false;
            context.CUSTOM_PARAMETERS.persistent_storage = true; // Legacy templates must not re-enable unavailable storage.
        }
        const completion = context.EngineLoader.load("canvas", "game");
        request("split/archive_files.json").complete('{"total_size":0,"content":[]}');
        assert.strictEqual(requests.length, 3);
        request("game_wasm.js").complete(Uint8Array.of(120).buffer);
        await waitFor(() => scripts.length === 1);
        runScript();
        request("game.wasm").complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
        await completion;
        assert.strictEqual(calls.main, 1);
        assert.deepStrictEqual(calls.errors, []);
        assert.strictEqual(syncs, phase === "restore" ? 3 : 0);
        if (phase !== "restore") {
            assert.strictEqual(context.Module.persistentStorage, false);
            context.Module.persistentSync();
            assert.strictEqual(syncs, 0);
        }
    }
}

// Both successful restoration and exhausted startup retries reset the budget for later saves (#9828).
function testLoaderInitialSyncRetryBudget() {
    for (const restoreSucceeds of [false, true]) {
        const context = loadLoaderEnvironment();
        context.console = { info() {}, warn() {} };
        context.Module.persistentStorage = true;
        let restores = 0, saves = 0, ready = 0;
        context.FS = { syncfs(populate, callback) {
            if (populate) {
                ++restores;
                context.Module.persistentSync(); // Closing restored files must not start a save during restoration.
                callback(restoreSucceeds && restores === 3 ? null : new Error("restore failed"));
            } else {
                ++saves;
                callback(null);
            }
        } };
        context.Module.preSync(() => ++ready);
        assert.strictEqual(restores, 3);
        assert.strictEqual(ready, 1);
        assert.strictEqual(saves, 0);
        assert.strictEqual(context.Module._syncTries, 0);
        context.Module.persistentSync();
        assert.strictEqual(saves, 1, "restoration must leave a fresh save retry budget");
    }
}

// Successful saves reset retries (#9828); queued writes coalesce, and exhausting retries must not block future saves.
function testLoaderPersistentSyncRetryBudget() {
    const context = loadLoaderEnvironment();
    context.console = { info() {}, warn() {} };
    context.Module.persistentStorage = true;
    context.Module._syncInitial = true;
    const callbacks = [];
    context.FS = { syncfs(populate, callback) {
        assert.strictEqual(populate, false);
        callbacks.push(callback);
    } };
    function save() {
        const count = callbacks.length;
        context.Module.persistentSync();
        assert.strictEqual(callbacks.length, count + 1, "a new save must reach the filesystem");
        return callbacks[count];
    }
    const failure = new Error("save failed");
    for (let cycle = 0; cycle < 2; ++cycle) {
        for (const error of [failure, failure, null]) save()(error);
    }
    const pending = save();
    const beforeQueued = callbacks.length;
    context.Module.persistentSync();
    context.Module.persistentSync();
    assert.strictEqual(callbacks.length, beforeQueued, "saves must not overlap");
    pending(null);
    assert.strictEqual(callbacks.length, beforeQueued + 1, "queued writes need only one additional sync");
    callbacks[beforeQueued](null);

    for (let attempt = 0; attempt < 3; ++attempt) save()(failure);
    const beforeExhaustion = callbacks.length;
    context.Module.persistentSync();
    assert.strictEqual(callbacks.length, beforeExhaustion, "an exhausted budget must stop retrying");
    save()(null);
    assert.strictEqual(context.Module._syncInProgress, false);
}

// Every engine failure must reject startup once, cancel archive peers, and prevent later readiness events from starting main.
async function testLoaderStartupFailure() {
    for (const failure of ["script-hash", "wasm-hash", "script-execution", "instantiation", "transport"]) {
        const { context, calls, requests, scripts, request, runScript, waitFor } = setupEngineStartup();
        if (failure === "script-hash") context.EngineLoader.wasmjs_sha1 = "00";
        if (failure === "wasm-hash") context.EngineLoader.wasm_sha1 = "00";
        if (failure === "instantiation") context.WebAssembly.instantiate = async () => { throw new Error("instantiation failed"); };
        const completion = context.EngineLoader.load("canvas", "game");
        request("split/archive_files.json").complete(JSON.stringify({ total_size: 2, content: [archiveFile("a", 2)] }));
        await waitFor(() => requests.length === 5);
        if (failure === "transport") {
            request("game_wasm.js").fail();
        } else {
            request("game_wasm.js").complete(Uint8Array.of(120).buffer);
            if (failure !== "script-hash") {
                await waitFor(() => scripts.length === 1);
                if (failure === "script-execution") scripts[0].onerror({});
                else {
                    runScript();
                    request("game.wasm").complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
                }
            }
        }
        await assert.rejects(completion);
        assert.strictEqual(calls.errors.length, 1, failure);
        assert.strictEqual(calls.consoleErrors.length, 1, failure);
        assert(requests.filter(xhr => xhr.url.startsWith("split/a")).every(xhr => !xhr.pending));
        const session = context.GameStartup._session;
        session.runtime.resolve();
        session.storage.resolve();
        session.filesystem.resolve();
        assert.strictEqual(calls.main, 0);
        assert.strictEqual(context.Module._filesToPreload.length, 0);
    }
}

// Callback-only templates must receive startup failures without an unhandled promise rejection (#11476).
async function testLoaderHandledStartupFailure() {
    const unhandled = [];
    const onUnhandled = error => unhandled.push(error);
    process.on("unhandledRejection", onUnhandled);
    try {
        for (const failure of ["wasm-hash", "instantiation", "archive-hash"]) {
            const { context, requests, calls, waitFor } = setupArchiveDownloads();
            context.window.crypto = crypto.webcrypto;
            context.WebAssembly = { instantiate: async () => { throw new Error("instantiation failed"); } };
            context.EngineLoader.wasm_size = 8;
            context.EngineLoader.wasm_sha1 = failure === "wasm-hash" ? "00" : "";
            if (failure !== "archive-hash") {
                context.EngineLoader.loadWasmAsync = (engine, session) =>
                    context.EngineLoader.loadAndInstantiateWasmAsync(engine.wasm, {}, session.scope);
            }
            context.GameArchiveLoader.MAX_VERIFICATION_RETRIES = 0;
            // A custom HTML template can use only start_error, without awaiting the returned promise.
            context.GameArchiveLoader.loadArchiveDescription("/archive_files.json");
            const file = archiveFile("a");
            if (failure === "archive-hash") file.sha1 = "00";
            requests[0].complete(JSON.stringify({ total_size: 1, content: [file] }));
            if (failure === "archive-hash") {
                await waitFor(() => requests.length === 2);
                requests[1].complete(Uint8Array.of(7).buffer);
            } else {
                requests[1].complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
            }
            await waitFor(() => calls.errors.length === 1);
            assert.strictEqual(context.GameStartup._session.state, "failed");
        }
        // Node emits unhandledRejection after promise jobs, before the next event-loop turn.
        await new Promise(resolve => setImmediate(resolve));
        assert.deepStrictEqual(unhandled, []);
    } finally {
        process.removeListener("unhandledRejection", onUnhandled);
    }
}

// Preserve archive error callbacks while engine and unsupported-platform failures use start_error.
async function testLoaderStartupErrorCallbacks() {
    for (const failure of ["description", "archive", "engine", "unsupported"]) {
        const { context, calls, requests, request, waitFor } = setupEngineStartup();
        const downloadErrors = [];
        context.CUSTOM_PARAMETERS.can_not_download_file_callback = error => downloadErrors.push(error);
        context.GameArchiveLoader.MAX_VERIFICATION_RETRIES = 0;
        if (failure === "unsupported") context.Module.isWASMSupported = false;
        const completion = context.EngineLoader.load("canvas", "game");
        if (failure === "unsupported") assert.strictEqual(requests.length, 0);
        else request("split/archive_files.json").complete(failure === "description" ? "invalid json" :
            JSON.stringify({ total_size: 1, content: [archiveFile("a")] }));
        if (failure === "engine" || failure === "archive") {
            await waitFor(() => requests.length === 4);
            if (failure === "engine") request("game_wasm.js").fail();
            else request("split/a0").complete(new ArrayBuffer(0));
        }
        await assert.rejects(completion);
        const archiveError = failure === "description" || failure === "archive";
        assert.strictEqual(downloadErrors.length, archiveError ? 1 : 0, failure);
        assert.strictEqual(calls.errors.length, archiveError ? 0 : 1, failure);
        assert.strictEqual(calls.main, 0);
    }
}

// Fatal main failures must reject startup and log the original error even with a missing or empty start_error callback.
async function testLoaderFatalMainReporting() {
    const defaultStartError = loadLoaderEnvironment().CUSTOM_PARAMETERS.start_error;
    for (const callback of ["default", "missing", "custom"]) {
        for (const failure of ["trap", "exit", "abort"]) {
            const { context, calls, scripts, request, runScript, restore, waitFor } = setupEngineStartup();
            if (callback === "default") context.CUSTOM_PARAMETERS.start_error = defaultStartError;
            if (callback === "missing") delete context.CUSTOM_PARAMETERS.start_error;
            const trap = new WebAssembly.RuntimeError("memory access out of bounds");
            context.Module.callMain = () => {
                ++calls.main;
                if (failure === "trap") throw trap;
                if (failure === "abort") context.Module.onAbort("native assertion failed");
                return failure === "exit" ? 1 : 0;
            };
            const completion = context.EngineLoader.load("canvas", "game");
            request("split/archive_files.json").complete('{"total_size":0,"content":[]}');
            request("game_wasm.js").complete(Uint8Array.of(120).buffer);
            await waitFor(() => scripts.length === 1);
            runScript();
            restore();
            request("game.wasm").complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
            const message = failure === "trap" ? /memory access out of bounds/ :
                failure === "abort" ? /native assertion failed/ : /Engine exited with code 1/;
            await assert.rejects(completion, message);
            assert.strictEqual(context.GameStartup._session.state, "failed", failure);
            assert.strictEqual(calls.main, 1);
            assert.strictEqual(calls.consoleErrors.length, 1, failure);
            assert.match(calls.consoleErrors[0].message, message);
            if (failure === "trap") assert.strictEqual(calls.consoleErrors[0], trap, "preserve the exception and its stack");
            assert.strictEqual(calls.errors.length, callback === "custom" ? 1 : 0);
        }
    }
}

// Emscripten aborts must report once before or after startup, including when a window error follows.
async function testLoaderEngineAbort() {
    for (const state of ["loading", "started"]) {
        const { context, calls, requests, scripts, request, runScript, restore, waitFor } = setupEngineStartup();
        const completion = context.EngineLoader.load("canvas", "game");
        const session = context.GameStartup._session;
        if (state === "started") {
            request("split/archive_files.json").complete('{"total_size":0,"content":[]}');
            request("game_wasm.js").complete(Uint8Array.of(120).buffer);
            await waitFor(() => scripts.length === 1);
            runScript();
            restore();
            request("game.wasm").complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
            await completion;
            context.Module.printErr("ERROR:SCRIPT: recoverable script error");
            assert.strictEqual(session.state, "started", "stderr output alone is not a fatal engine failure");
            assert.strictEqual(calls.consoleErrors.length, 1);
            calls.consoleErrors.length = 0;
        }
        assert.deepStrictEqual(calls.consoleErrors, []);
        assert.deepStrictEqual(calls.errors, []);
        context.Module.onAbort("out of memory");
        if (state === "loading") {
            await assert.rejects(completion, /out of memory/);
            assert(requests.every(xhr => xhr.aborted));
        }
        assert.strictEqual(session.state, "failed");
        context.Module.ccall = () => {};
        context.window.dispatchEvent({ type: "error", error: new Error("same engine failure") });
        assert.strictEqual(calls.errors.length, 1, "the abort and its subsequent exception are one failure");
        assert.strictEqual(calls.consoleErrors.length, 1);
    }
}

// A throwing start_error callback must not prevent Emscripten from completing its abort operation.
async function testLoaderAbortCallbackFailure() {
    const { context, calls } = setupEngineStartup();
    const callbackError = new Error("custom error callback failed");
    context.CUSTOM_PARAMETERS.start_error = () => { throw callbackError; };
    const completion = context.EngineLoader.load("canvas", "game");
    assert.doesNotThrow(() => context.Module.onAbort("native assertion failed"));
    await assert.rejects(completion, /native assertion failed/);
    assert.strictEqual(calls.consoleErrors.length, 2);
    assert.strictEqual(calls.consoleErrors[1], callbackError);
}

// The normal Emscripten main-loop unwind (zero/undefined result) must still finish startup without an error.
async function testLoaderMainSuccessStatus() {
    for (const status of [0, undefined]) {
        const { context, calls, scripts, request, runScript, restore, waitFor } = setupEngineStartup();
        context.Module.callMain = () => { ++calls.main; return status; };
        const completion = context.EngineLoader.load("canvas", "game");
        request("split/archive_files.json").complete('{"total_size":0,"content":[]}');
        request("game_wasm.js").complete(Uint8Array.of(120).buffer);
        await waitFor(() => scripts.length === 1);
        runScript();
        restore();
        request("game.wasm").complete(Uint8Array.of(0, 97, 115, 109, 1, 0, 0, 0).buffer);
        await completion;
        assert.strictEqual(context.GameStartup._session.state, "started");
        assert.strictEqual(calls.main, 1);
        assert.deepStrictEqual(calls.errors, []);
        assert.deepStrictEqual(calls.consoleErrors, []);
    }
}

// Repeated or recursive startup callbacks must enter main only once (#9841).
function testLoaderMainReentrancy() {
    const context = loadLoaderEnvironment();
    context.console = { warn() {} };
    let started = 0, main = 0, removed = 0;
    context.ProgressView.removeProgress = () => ++removed;
    context.CUSTOM_PARAMETERS.start_success = () => {
        ++started;
        assert.strictEqual(started, 1, "start_success must not run recursively");
        context.Module._callMain();
    };
    context.Module.callMain = () => { ++main; };
    context.Module._callMain();
    context.Module._callMain();
    assert.strictEqual(started, 1);
    assert.strictEqual(main, 1);
    assert.strictEqual(removed, 1);
}

// Preloading transfers buffer ownership and tolerates recursive FS callbacks without loading files twice (bb45b611d0).
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
            assert(preloadedFiles <= archives.length, "recursive preload must not create a file twice");
            context.Module.preloadAll();
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
                       testLoaderResizeCanvas, testLoaderProgressView,
                       testLoaderWebGLDetection, testLoaderErrorHandlerReadiness, testLoaderContextMenu,
                       testLoaderWebGLSkipsWebGPUProbe, testLoaderWebGPUProbeGatesStartup, testLoaderWebGPUCancelledStartup,
                       testLoaderArchiveConcurrency, testLoaderArchiveRetryReleasesSlot, testLoaderArchiveQueuedCancellation,
                       testLoaderArchiveBufferOwnership, testLoaderMainReentrancy, testLoaderArchiveProgressWithoutLength,
                       testLoaderArchiveVerificationCompletion, testLoaderArchiveRetrySequence,
                       testLoaderArchiveRetryStateOwnership, testLoaderArchiveTransportRetry,
                       testLoaderArchiveProgressCancellation,
                       testLoaderArchiveVerificationRetryLimit, testLoaderArchiveCancellation,
                       testLoaderArchiveStaleVerification, testLoaderArchiveStaleDescription,
                       testLoaderArchiveStaleCachedFile, testLoaderArchiveDMFSVerificationConcurrency,
                       testLoaderArchiveDMFSVerificationQueueCancellation, testLoaderArchiveDMFSVerificationQueueRetry,
                       testLoaderArchiveDMFSRetry, testLoaderArchiveUnreadableData,
                       testLoaderArchiveDMFSCloseFailure, testLoaderArchiveDMFSErrorCleanup,
                       testLoaderArchiveDMFSVerificationCancellation, testLoaderArchiveTerminalStates,
                       testLoaderTransportRetryLifecycle, testLoaderTransportAdapters, testLoaderArchiveCacheResults,
                       testLoaderArchiveDescriptionHash,
                       testLoaderEngineHashFailure, testLoaderScriptBytes, testLoaderEngineSizeRetry,
                       testLoaderStreamingWasm, testLoaderStreamingCancellation, testLoaderStreamingProgressWithoutLength,
                       testLoaderStreamingProgressErrors,
                       testLoaderArchiveWriteFailure, testLoaderArchiveReplacementDropsFiles,
                       testLoaderArchiveCompletionPromise, testLoaderFetchCancellation, testLoaderFetchExceptions,
                       testLoaderFetchProgressErrors, testLoaderStartupProgressErrors,
                       testLoaderArchiveDiscardedStorage, testLoaderStartupReadiness, testLoaderStartupFailure,
                       testLoaderUnrelatedPageError, testLoaderEngineScriptError,
                       testLoaderStartupOverlapsManifest, testLoaderStartupEarlyEngineFailure,
                       testLoaderStartupEarlyWasm, testLoaderStartupEarlyStreamingWasm, testLoaderStartupEarlyStreamCancellation,
                       testLoaderStartupErrorCallbacks, testLoaderHandledStartupFailure,
                       testLoaderFatalMainReporting, testLoaderEngineAbort,
                       testLoaderAbortCallbackFailure, testLoaderMainSuccessStatus,
                       testLoaderEngineSelection, testLoaderStartupDMFS, testLoaderStartupStorageFallback,
                       testLoaderInitialSyncRetryBudget, testLoaderPersistentSyncRetryBudget,
                       testLoaderPixelRatioBounds, testLoaderPixelRatioScaleModes,
                       testLoaderPixelRatioChanges, testLoaderFullscreenPixelRatio,
                       testTouchMouseSources, testMixedTouchAndMouseSources]) {
        // Pending promises alone do not keep Node alive. Treat a missing completion
        // signal as a failure rather than silently exiting before the suite finishes.
        const unfinished = () => {
            console.error(test.name + " did not complete");
            process.exitCode = 1;
        };
        process.once("beforeExit", unfinished);
        try {
            await test();
            process.stdout.write(test.name + " passed\n");
        } catch (error) {
            console.error(test.name, error);
            process.exitCode = 1;
        } finally {
            process.removeListener("beforeExit", unfinished);
        }
    }
}

runTests();
