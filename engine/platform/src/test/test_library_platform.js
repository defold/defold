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

function loadLoaderEnvironment(hasWebGPU = false, verifySize = true) {
    const loaderPath = path.resolve(__dirname,
        "../../../../com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/bundle/resources/web/dmloader.js");
    const flags = { "html5.verify_downloaded_file_size": verifySize, DEFOLD_HAS_WASM_ENGINE: true,
                    DEFOLD_HAS_WEBGPU: hasWebGPU };
    const loader = fs.readFileSync(loaderPath, "utf8")
        .replace(/\{\{![\s\S]*?\}\}/g, "")
        .replace(/\{\{([#^])([^}]+)\}\}([\s\S]*?)\{\{\/\2\}\}/g,
            (_, kind, name, content) => (kind === "#" ? !!flags[name] : !flags[name]) ? content : "")
        .replace(/\{\{[^}]+\}\}/g, "0");
    const context = loadEnvironment();
    context.setTimeout = setTimeout;
    context.clearTimeout = clearTimeout;
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

// Verify limited and unlimited XHR downloads reconstruct every file when pieces arrive out of order.
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
        await loader.downloadContent();
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
        assert.strictEqual(calls.maxPending, limit === undefined ? 8 : limit);
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
    const method = target[name].bind(target);
    target[name] = (...args) => {
        const result = method(...args);
        results.push(result);
        return result;
    };
    return results;
}

function setupArchiveDownloads(files = [archiveFile("a")], verifySize = true) {
    const context = loadLoaderEnvironment(false, verifySize);
    const loader = context.GameArchiveLoader;
    const timers = new Map();
    const requests = [];
    const waiters = new Set();
    const calls = { loaded: [], errors: [], completed: 0, engines: 0, maxPending: 0 };
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
    context.console = { log() {}, warn() {} };
    context.setTimeout = callback => {
        const id = nextTimer++;
        timers.set(id, callback);
        notify();
        return id;
    };
    context.clearTimeout = id => timers.delete(id);
    context.XMLHttpRequest = class {
        static DONE = 4;
        open(method, url) { this.url = url; }
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
        fail() {
            assert.strictEqual(this.pending, true, "only an active request can fail");
            this.pending = false;
            this.onerror({});
        }
    };
    context.FileLoader.options.retryCount = 2;
    context.CUSTOM_PARAMETERS.start_error = error => { calls.errors.push(String(error)); notify(); };
    context.Module.isWASMSupported = true;
    context.Module._preloadAndCallMain = () => {};
    context.EngineLoader.arc_sha1 = "";
    context.EngineLoader.loadWasmAsync = () => ++calls.engines;
    loader._files = files;
    loader.addFileLoadedListener(file => { calls.loaded.push(file.name); notify(); });
    loader.addFileLoadedListener(context.Module.onArchiveFileLoaded);
    loader.addFileDownloadErrorListener(error => { calls.errors.push(String(error)); notify(); });
    loader.addArchiveLoadedListener(() => { ++calls.completed; notify(); });
    loader.addArchiveLoadedListener(context.Module.onArchiveLoaded);
    function runRetry() {
        assert.strictEqual(timers.size, 1);
        const [id, callback] = timers.entries().next().value;
        timers.delete(id);
        callback();
    }
    return { context, loader, timers, requests, calls, runRetry, waitFor };
}

// A file must pass async verification before the next file starts or the archive is marked complete.
async function testLoaderArchiveVerificationCompletion() {
    for (const verifySize of [true, false]) {
        const file = archiveFile("a", 2);
        file.sha1 = "00";
        const { loader, requests, calls, context, waitFor } = setupArchiveDownloads([file, archiveFile("b")], verifySize);
        const digest = deferred();
        context.window.crypto = { subtle: { digest: () => digest.promise } };
        await loader.downloadContent();
        assert.strictEqual(requests.length, 2);
        requests[0].complete(Uint8Array.of(1).buffer);
        requests[1].complete(Uint8Array.of(2).buffer);
        assert.deepStrictEqual(calls.loaded, []);
        assert.strictEqual(calls.completed, 0);
        assert.strictEqual(requests.length, 2);
        digest.resolve(Uint8Array.of(0).buffer);
        await waitFor(() => calls.loaded.length === 1);
        assert.deepStrictEqual(calls.loaded, ["a"]);
        assert.strictEqual(requests.length, 3);
        requests[2].complete(Uint8Array.of(3).buffer);
        await waitFor(() => calls.completed === 1);
        assert.deepStrictEqual(calls.loaded, ["a", "b"]);
        assert.strictEqual(calls.completed, 1);
        assert.strictEqual(loader.isCompleted, true);
        assert.strictEqual(loader._files.length, 0, "production completion listener clears the files");
        assert.deepStrictEqual(calls.errors, []);
    }
}

// A verification retry must finish the failed file before starting the next file in the archive.
async function testLoaderArchiveRetrySequence() {
    const { loader, requests, calls, timers, runRetry, context, waitFor } = setupArchiveDownloads([archiveFile("a"), archiveFile("b")]);
    await loader.downloadContent();
    requests[0].complete(new ArrayBuffer(0));
    await waitFor(() => timers.size === 1);
    assert.strictEqual(context.ProgressUpdater.current, 0);
    assert.deepStrictEqual(calls.loaded, []);
    runRetry();
    assert.strictEqual(requests[1].url, "split/a0?dmretry=3");
    requests[1].complete(Uint8Array.of(1).buffer);
    await waitFor(() => calls.loaded.length === 1);
    assert.deepStrictEqual(calls.loaded, ["a"]);
    assert.strictEqual(requests[2].url, "split/b0");
    requests[2].complete(Uint8Array.of(2).buffer);
    await waitFor(() => calls.completed === 1);
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
        await loader.downloadContent();
        requests.slice().forEach(xhr => xhr.complete(Uint8Array.of(9).buffer));
        await waitFor(() => timers.size === 1);
        assert.strictEqual(context.ProgressUpdater.current, 0);
        runRetry();
        requests.slice(3).forEach(xhr => xhr.complete(Uint8Array.of(8).buffer));
        await waitFor(() => timers.size === 1);
        runRetry();
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
    await loader.downloadContent();
    requests[0].progress(10);
    requests[0].fail();
    assert.strictEqual(context.ProgressUpdater.current, 0);
    requests[1].complete(Uint8Array.of(2).buffer);
    runRetry();
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
        const { context, loader, requests, calls, timers } = setupArchiveDownloads();
        const cancelled = deferred();
        context.ProgressUpdater.addListener(progress => {
            if (progress === 0) {
                loader.cleanUp();
                cancelled.resolve();
            }
        });
        await loader.downloadContent();
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
    await loader.downloadContent();
    for (let attempt = 0; attempt <= loader.MAX_VERIFICATION_RETRIES; ++attempt) {
        requests[attempt].complete(new ArrayBuffer(0));
        await waitFor(() => timers.size === 1 || calls.errors.length === 1);
        if (attempt < loader.MAX_VERIFICATION_RETRIES) runRetry();
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
        await loader.downloadContent();
        if (phase === "transport-retry") requests[0].fail();
        if (phase === "verification-retry") requests[0].complete(new ArrayBuffer(0));
        if (phase !== "downloading") await waitFor(() => timers.size === 1);
        loader.cleanUp();
        if (phase === "downloading") assert.strictEqual(requests[0].aborted, true);
        assert.strictEqual(timers.size, 0);
        loader.loadArchiveDescription("/new.json");
        requests[1].complete(JSON.stringify({ total_size: 1, content: [archiveFile("new")] }));
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
        await loader.downloadContent();
        requests[0].complete(Uint8Array.of(1).buffer);
        loader.loadArchiveDescription("/new.json");
        requests[1].complete(JSON.stringify({ total_size: 1, content: [archiveFile("new")] }));
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

// An obsolete manifest hash result must not replace a newer archive or start another engine download.
async function testLoaderArchiveStaleDescription() {
    const { context, loader, requests, calls, waitFor } = setupArchiveDownloads([]);
    const digest = deferred();
    context.EngineLoader.arc_sha1 = "00";
    context.TextEncoder = TextEncoder;
    context.window.crypto = { subtle: { digest: () => digest.promise } };
    const descriptions = observeAsyncMethod(loader, "onReceiveDescription");
    loader.loadArchiveDescription("/old.json");
    requests[0].complete(JSON.stringify({ total_size: 1, content: [archiveFile("old")] }));
    loader.loadArchiveDescription("/new.json");
    context.EngineLoader.arc_sha1 = "";
    requests[1].complete(JSON.stringify({ total_size: 1, content: [archiveFile("new")] }));
    digest.resolve(Uint8Array.of(0).buffer);
    await descriptions[0];
    assert.strictEqual(calls.engines, 1);
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
            stat(name) { if (name === "/data/old") return { size: 1 }; throw new Error("not found"); },
            open(name, mode) { const stream = { name, mode, closed: false }; streams.push(stream); return stream; },
            mmap() { return Uint8Array.of(0); },
            close(stream) { assert.strictEqual(stream.closed, false); stream.closed = true; },
            write(stream) { assert.strictEqual(stream.closed, false); }
        };
        const oldDownload = loader.downloadContent();
        loader.cleanUp();
        assert.strictEqual(streams[0].closed, true);
        loader._files = [archiveFile("new")];
        await loader.downloadContent();
        digest.resolve(Uint8Array.of(0).buffer);
        await oldDownload;
        assert.deepStrictEqual(streams.map(stream => [stream.name, stream.mode]), [["/data/old", "r"], ["/data/new", "w+"]]);
        assert.deepStrictEqual(calls.loaded, []);
        requests[0].complete(Uint8Array.of(1).buffer);
        await waitFor(() => calls.completed === 1);
        assert.deepStrictEqual(calls.loaded, ["new"]);
        assert(streams.every(stream => stream.closed));
        assert.deepStrictEqual(calls.errors, []);
    }
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
        await loader.downloadContent();
        requests[0].complete(Uint8Array.of(9, 9, 9).buffer);
        requests[1].complete(Uint8Array.of(9).buffer);
        await waitFor(() => timers.size === 1);
        assert.strictEqual(streams[0].closed, true);
        assert.strictEqual(persisted.length, 3);
        runRetry();
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

// A DMFS close failure after verification must retry with a fresh stream and report persistent failures instead of stalling.
async function testLoaderArchiveDMFSCloseFailure() {
    for (const alwaysFails of [false, true]) {
        const { context, loader, requests, calls, timers, runRetry, waitFor } = setupArchiveDownloads();
        const streams = [];
        const closeError = new Error("stream close failed");
        context.Module.isDMFSSupported = true;
        context.DMSYS = { GetUserPersistentDataRoot: () => "/data" };
        context.FS = {
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
        await loader.downloadContent();
        for (let attempt = 0; attempt <= loader.MAX_VERIFICATION_RETRIES; ++attempt) {
            requests[attempt].complete(Uint8Array.of(1).buffer);
            await waitFor(() => timers.size === 1 || calls.completed === 1 || calls.errors.length === 1);
            if (timers.size === 0) break;
            assert.deepStrictEqual(calls.loaded, []);
            assert.deepStrictEqual(calls.errors, []);
            assert.strictEqual(context.ProgressUpdater.current, 0);
            runRetry();
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
    const { context, loader, requests, calls, timers, runRetry } = setupArchiveDownloads([archiveFile("a", 3)]);
    let closeCalls = 0;
    context.Module.isDMFSSupported = true;
    context.DMSYS = { GetUserPersistentDataRoot: () => "/data" };
    context.FileLoader.options.retryCount = 1;
    context.FS = {
        stat() { throw new Error("not found"); },
        open() { return {}; },
        close() { ++closeCalls; throw new Error("stream close failed"); }
    };
    await loader.downloadContent();
    requests[0].fail();
    runRetry();
    requests[1].fail();
    assert.strictEqual(timers.size, 1, "a peer has a pending transport retry");
    assert.doesNotThrow(() => requests[3].fail());
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
            stat() { throw new Error("not found"); },
            open() { const stream = { closed: false }; streams.push(stream); return stream; },
            mmap() { return Uint8Array.of(1); },
            write(stream) { assert.strictEqual(stream.closed, false); },
            close(stream) { assert.strictEqual(stream.closed, false); stream.closed = true; }
        };
        const verifications = observeAsyncMethod(loader, "verifyFile");
        await loader.downloadContent();
        requests[0].complete(Uint8Array.of(1).buffer);
        loader.loadArchiveDescription("/new.json");
        assert.strictEqual(streams[0].closed, true);
        requests[1].complete(JSON.stringify({ total_size: 1, content: [archiveFile("a")] }));
        context.Module.postRun[0]();
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

// A terminal piece error cancels its peers; malformed manifests fail before downloads while an empty archive completes.
async function testLoaderArchiveTerminalStates() {
    const { context, loader, requests, calls } = setupArchiveDownloads([archiveFile("a", 2)]);
    context.FileLoader.options.retryCount = 0;
    await loader.downloadContent();
    requests[0].fail();
    assert.strictEqual(requests[1].aborted, true);
    assert.strictEqual(calls.errors.length, 1);
    assert.strictEqual(calls.completed, 0);

    for (const description of [{}, { total_size: 1, content: [{ name: "a", size: 1 }] }]) {
        const invalid = setupArchiveDownloads();
        invalid.loader.loadArchiveDescription("/invalid.json");
        invalid.requests[0].complete(JSON.stringify(description));
        assert.strictEqual(invalid.calls.errors.length, 1);
        assert.strictEqual(invalid.calls.engines, 0);
        assert.strictEqual(invalid.requests.length, 1);
        assert.strictEqual(invalid.calls.completed, 0);
    }
    const empty = setupArchiveDownloads();
    empty.loader.loadArchiveDescription("/empty.json");
    empty.requests[0].complete('{"total_size":0,"content":[]}');
    await empty.waitFor(() => empty.calls.completed === 1);
    assert.deepStrictEqual(empty.calls.errors, []);
}

// XHR retries obey their budget, release completed requests, and cancel active requests or pending retries.
function testLoaderTransportRetryLifecycle() {
    for (const failure of ["network", "status", "verification"]) {
        const { context, requests, timers, calls, runRetry } = setupArchiveDownloads();
        let loaded = 0, progress = 0;
        const request = context.FileLoader.load("/piece", "arraybuffer", delta => progress += delta,
            error => calls.errors.push(error), () => ++loaded, delta => progress -= delta,
            () => failure === "verification" ? "bad response" : undefined);
        for (let attempt = 0; attempt < 3; ++attempt) {
            const xhr = requests[attempt];
            if (attempt === 0 && failure === "network") xhr.progress(2);
            if (failure === "network") xhr.fail();
            else xhr.complete(new ArrayBuffer(0), failure === "status" ? 503 : 200);
            if (attempt < 2) runRetry();
        }
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
        assert.strictEqual(requests.length, 1);
        assert.strictEqual(timers.size, 0);
        assert.strictEqual(loaded, 0);
        assert.strictEqual(errors, 0);
        if (!retryPending) assert.strictEqual(requests[0].aborted, true);
    }
    const { context, requests, timers, calls, runRetry } = setupArchiveDownloads();
    let loaded = 0;
    const request = context.FileLoader.load("/piece", "arraybuffer", () => {},
        error => calls.errors.push(error), () => ++loaded, () => {});
    requests[0].fail();
    runRetry();
    requests[1].complete(Uint8Array.of(1).buffer);
    request.cancel();
    assert.strictEqual(loaded, 1, "a successful retry must complete only once");
    assert.strictEqual(requests.length, 2);
    assert.strictEqual(timers.size, 0);
    assert.deepStrictEqual(calls.errors, []);
    assert.strictEqual(requests[1].aborted, undefined, "completed requests must not be aborted");
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
                       testLoaderArchiveVerificationCompletion, testLoaderArchiveRetrySequence,
                       testLoaderArchiveRetryStateOwnership, testLoaderArchiveTransportRetry,
                       testLoaderArchiveProgressCancellation,
                       testLoaderArchiveVerificationRetryLimit, testLoaderArchiveCancellation,
                       testLoaderArchiveStaleVerification, testLoaderArchiveStaleDescription,
                       testLoaderArchiveStaleCachedFile, testLoaderArchiveDMFSRetry,
                       testLoaderArchiveDMFSCloseFailure, testLoaderArchiveDMFSErrorCleanup,
                       testLoaderArchiveDMFSVerificationCancellation, testLoaderArchiveTerminalStates,
                       testLoaderTransportRetryLifecycle,
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
