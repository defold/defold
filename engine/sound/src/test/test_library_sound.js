const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

let nowMs = 0;
let wallClockOffsetMs = 0;
let nextTimerId = 1;
let timers = new Map();
let warnings = [];
let infos = [];
let deviceChangeHandler = null;

function installEnvironment() {
    global.Date.now = () => nowMs + wallClockOffsetMs;
    Object.defineProperty(global, "performance", {
        configurable: true,
        value: { now: () => nowMs }
    });
    global.setTimeout = (callback, delay) => {
        const id = nextTimerId++;
        timers.set(id, { callback, deadline: nowMs + delay });
        return id;
    };
    global.clearTimeout = (id) => timers.delete(id);
    Object.defineProperty(global, "navigator", {
        configurable: true,
        value: {
            mediaDevices: {
                addEventListener: (name, callback) => {
                    assert.strictEqual(name, "devicechange");
                    deviceChangeHandler = callback;
                },
                removeEventListener: (name, callback) => {
                    assert.strictEqual(name, "devicechange");
                    if (deviceChangeHandler === callback) {
                        deviceChangeHandler = null;
                    }
                }
            }
        }
    });
    global.console = {
        warn: (message) => warnings.push(message),
        info: (message) => infos.push(message),
        log: () => {}
    };
    global.HEAPF32 = new Float32Array(32768);
    global.autoAddDeps = () => {};
    global.addToLibrary = () => {};

    const libraryPath = path.join(__dirname, "..", "js", "library_sound.js");
    const source = fs.readFileSync(libraryPath, "utf8");
    vm.runInThisContext(source + "\nglobal.LibrarySoundDevice = LibrarySoundDevice;");
    global.DefoldSoundDevice = global.LibrarySoundDevice.$DefoldSoundDevice;
}

function resetEnvironment() {
    if (global._dmJSDeviceShared) {
        const ids = Object.keys(global._dmJSDeviceShared.devices);
        for (const id of ids) {
            global.LibrarySoundDevice.dmDeviceJSClose(Number(id));
        }
    }
    delete global.AudioContext;
    delete global.webkitAudioContext;
    delete global._dmJSDeviceShared;
    global.top = global;
    // Recovery tests model an activated page; autoplay tests override this state.
    global.navigator.userActivation = { hasBeenActive: true, isActive: false };
    nowMs = 0;
    wallClockOffsetMs = 0;
    nextTimerId = 1;
    timers = new Map();
    warnings = [];
    infos = [];
    deviceChangeHandler = null;
}

function advanceTime(milliseconds) {
    const target = nowMs + milliseconds;
    while (true) {
        let nextId = null;
        let nextDeadline = Infinity;
        for (const [id, timer] of timers) {
            if (timer.deadline < nextDeadline) {
                nextId = id;
                nextDeadline = timer.deadline;
            }
        }
        if (nextId === null || nextDeadline > target) {
            break;
        }
        nowMs = nextDeadline;
        const timer = timers.get(nextId);
        timers.delete(nextId);
        timer.callback();
    }
    nowMs = target;
}

function resolvedResult() {
    return {
        then: (onResolved) => {
            if (onResolved) {
                onResolved();
            }
            return resolvedResult();
        },
        catch: () => resolvedResult()
    };
}

function deferredResult() {
    let onResolved = null;
    let onRejected = null;
    return {
        promise: {
            then: (resolved, rejected) => {
                onResolved = resolved;
                onRejected = rejected;
            }
        },
        resolve: () => onResolved(),
        reject: () => onRejected()
    };
}

function makeAudioContext(options = {}) {
    const contexts = [];
    class FakeAudioContext {
        constructor() {
            if (options.throwOnConstruct) {
                throw new Error("AudioContext unavailable");
            }
            this.sampleRate = options.sampleRate || 44100;
            this.state = options.initialState || "running";
            this.outputLatency = 0;
            this.baseLatency = 0;
            this.destination = {};
            this.onstatechange = null;
            this.startedBuffers = 0;
            this.resumeCalls = 0;
            this.sources = [];
            this.closed = false;
            this.createdAt = nowMs;
            this.frozenTime = null;
            contexts.push(this);
        }
        get currentTime() {
            return this.frozenTime !== null ? this.frozenTime : (nowMs - this.createdAt) / 1000;
        }
        createBuffer(channelCount, frameCount) {
            if (options.throwOnQueue) {
                throw new Error("Audio queue failed");
            }
            assert.strictEqual(channelCount, 2);
            return {
                copyToChannel: (input) => assert.strictEqual(input.length, frameCount)
            };
        }
        createBufferSource() {
            const context = this;
            const source = {
                buffer: null,
                onended: null,
                stopped: false,
                disconnected: false,
                connect: () => {},
                disconnect: () => source.disconnected = true,
                start: () => context.startedBuffers++,
                stop: () => source.stopped = true
            };
            context.sources.push(source);
            return source;
        }
        resume() {
            this.resumeCalls++;
            if (options.throwOnResume) {
                throw new Error("Audio resume failed");
            }
            if (options.resumeResult) {
                return options.resumeResult;
            }
            this.state = "running";
            if (this.onstatechange) {
                this.onstatechange();
            }
            return resolvedResult();
        }
        close() {
            this.closed = true;
            this.state = "closed";
            return resolvedResult();
        }
    }
    FakeAudioContext.contexts = contexts;
    return FakeAudioContext;
}

function testSilentQueueAdvances() {
    resetEnvironment();
    const id = global.LibrarySoundDevice.dmDeviceJSOpen(4);
    assert.strictEqual(id, 0);
    assert.strictEqual(global.LibrarySoundDevice.dmGetDeviceSampleRate(id), 48000);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 1);

    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 4800);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 3);
    advanceTime(100);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 4);
    assert.strictEqual(warnings.length, 1);
    assert.strictEqual(timers.size, 1);
}

function testSilentQueueRestartsAfterIdle() {
    resetEnvironment();
    const id = global.LibrarySoundDevice.dmDeviceJSOpen(4);

    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 4800);
    advanceTime(100);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 4);

    global.LibrarySoundDevice.dmDeviceJSPlaybackIdle(id);
    advanceTime(9900);
    global.LibrarySoundDevice.dmDeviceJSPlaybackStarted(id);
    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 4800);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 3);
    advanceTime(100);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 4);
}

function testConstructorFailureStaysSilent() {
    resetEnvironment();
    global.AudioContext = makeAudioContext({ throwOnConstruct: true });
    const id = global.LibrarySoundDevice.dmDeviceJSOpen(4);
    assert.strictEqual(global.LibrarySoundDevice.dmGetDeviceSampleRate(id), 48000);
    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 4800);
    advanceTime(2000);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 4);
    assert.strictEqual(warnings.length, 1);
}

function testPeriodicRecoveryKeepsDeviceRate() {
    resetEnvironment();
    const id = global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const FakeAudioContext = makeAudioContext({ sampleRate: 44100 });
    global.AudioContext = FakeAudioContext;

    advanceTime(2000);
    assert.strictEqual(FakeAudioContext.contexts.length, 1);
    assert.strictEqual(global.LibrarySoundDevice.dmGetDeviceSampleRate(id), 48000);
    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 480);
    assert.strictEqual(FakeAudioContext.contexts[0].startedBuffers, 1);
    const bufferedTo = global._dmJSDeviceShared.devices[id].bufferedTo;
    global.DefoldSoundDevice.TryResumeAudio();
    assert.strictEqual(global._dmJSDeviceShared.devices[id].bufferedTo, bufferedTo);
    assert.strictEqual(infos.length, 1);
}

function testInteractionAndDeviceChangeRecoverImmediately() {
    resetEnvironment();
    global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const SuspendedAudioContext = makeAudioContext({ initialState: "suspended" });
    global.AudioContext = SuspendedAudioContext;
    global.DefoldSoundDevice.TryResumeAudio();
    assert.strictEqual(SuspendedAudioContext.contexts[0].state, "running");

    resetEnvironment();
    global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const RunningAudioContext = makeAudioContext();
    global.AudioContext = RunningAudioContext;
    assert.notStrictEqual(deviceChangeHandler, null);
    deviceChangeHandler();
    assert.strictEqual(RunningAudioContext.contexts.length, 1);
    assert.strictEqual(RunningAudioContext.contexts[0].state, "running");
}

// Verifies startup stays silent without premature resume calls, then a timer can use sticky activation.
function testAutoplayWaitsForActivation() {
    resetEnvironment();
    global.navigator.userActivation.hasBeenActive = false;
    const FakeAudioContext = makeAudioContext({ initialState: "suspended" });
    global.AudioContext = FakeAudioContext;
    const id = global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const context = FakeAudioContext.contexts[0];

    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 4410);
    advanceTime(6000);
    deviceChangeHandler();
    for (const type of ["touchstart", "touchcancel", "keydown"]) {
        global.DefoldSoundDevice.TryResumeAudio({ type, isTrusted: true, key: "Escape", keyCode: 27 });
    }
    assert.strictEqual(context.resumeCalls, 0);
    assert.strictEqual(context.state, "suspended");
    assert.strictEqual(global._dmJSDeviceShared.resumePending, false);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 4);
    assert.strictEqual(warnings.length, 0);

    // Activation can occur elsewhere on the page and outlive its transient activation window.
    global.navigator.userActivation.hasBeenActive = true;
    advanceTime(2000);
    assert.strictEqual(context.resumeCalls, 1);
    assert.strictEqual(context.state, "running");
    assert.strictEqual(timers.size, 0);
    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 4410);
    assert.strictEqual(context.startedBuffers, 1);
}

// Verifies older browsers accept trusted input, including modified game keys, and remember it for recovery.
function testAutoplayWithoutActivationAPI() {
    for (const event of [
        { type: "mousedown" },
        { type: "touchend" },
        { type: "keydown", key: " ", keyCode: 32 },
        { type: "keydown", key: "ArrowRight", keyCode: 39, ctrlKey: true },
        { type: "keydown", key: "z", keyCode: 90, altKey: true },
        { type: "keydown", key: "ArrowUp", keyCode: 38, metaKey: true },
        { type: "pointerdown", pointerType: "mouse" },
        { type: "pointerup", pointerType: "touch" }
    ]) {
        resetEnvironment();
        delete global.navigator.userActivation;
        const FakeAudioContext = makeAudioContext({ initialState: "suspended" });
        global.AudioContext = FakeAudioContext;
        global.LibrarySoundDevice.dmDeviceJSOpen(4);
        const context = FakeAudioContext.contexts[0];

        advanceTime(2000);
        deviceChangeHandler();
        for (const ignored of [
            { type: "mousedown", isTrusted: false },
            { type: "touchstart", isTrusted: true },
            { type: "touchcancel", isTrusted: true },
            { type: "keydown", isTrusted: true, key: "Escape", keyCode: 27 },
            { type: "pointerdown", isTrusted: true, pointerType: "touch" }
        ]) {
            global.DefoldSoundDevice.TryResumeAudio(ignored);
        }
        assert.strictEqual(context.resumeCalls, 0);

        global.DefoldSoundDevice.TryResumeAudio({ ...event, isTrusted: true });
        assert.strictEqual(context.state, "running");
        assert.strictEqual(context.resumeCalls, 1);
        context.state = "suspended";
        context.onstatechange();
        advanceTime(2000);
        assert.strictEqual(context.state, "running");
        assert.strictEqual(context.resumeCalls, 2);
    }
}

// Verifies activation survives a failed constructor so an unavailable device can recover without another gesture.
function testActivationSurvivesContextFailure() {
    resetEnvironment();
    delete global.navigator.userActivation;
    global.AudioContext = makeAudioContext({ throwOnConstruct: true });
    global.LibrarySoundDevice.dmDeviceJSOpen(4);
    global.DefoldSoundDevice.TryResumeAudio({ type: "touchend", isTrusted: true });

    const RecoveredAudioContext = makeAudioContext({ initialState: "suspended" });
    global.AudioContext = RecoveredAudioContext;
    advanceTime(2000);
    assert.strictEqual(RecoveredAudioContext.contexts[0].state, "running");
    assert.strictEqual(RecoveredAudioContext.contexts[0].resumeCalls, 1);
}

// Verifies permitted autoplay still recovers on timers and device changes without requiring a new gesture.
function testPermittedAutoplayRecoversWithoutActivation() {
    for (const recover of [() => advanceTime(2000), () => deviceChangeHandler()]) {
        resetEnvironment();
        global.navigator.userActivation.hasBeenActive = false;
        const options = {};
        const FakeAudioContext = makeAudioContext(options);
        global.AudioContext = FakeAudioContext;
        global.LibrarySoundDevice.dmDeviceJSOpen(4);
        const context = FakeAudioContext.contexts[0];

        context.state = "suspended";
        context.onstatechange();
        recover();
        assert.strictEqual(context.state, "running");
        assert.strictEqual(context.resumeCalls, 1);

        // A replacement context may need an explicit resume even though the original could autoplay.
        context.state = "closed";
        context.onstatechange();
        options.initialState = "suspended";
        recover();
        assert.strictEqual(FakeAudioContext.contexts[1].state, "running");
        assert.strictEqual(FakeAudioContext.contexts[1].resumeCalls, 1);
    }
}

// Verifies an iframe can use delegated autoplay without local activation, including after a pending resume.
function testIframeAutoplayWithoutLocalActivation() {
    for (const pending of [false, true]) {
        resetEnvironment();
        global.navigator.userActivation.hasBeenActive = false;
        // Cross-origin ancestors cannot be inspected; only compare the window reference.
        global.top = new Proxy({}, { get: () => { throw new Error("Cross-origin access"); } });
        const deferred = deferredResult();
        const options = { initialState: "suspended" };
        if (pending) {
            options.resumeResult = deferred.promise;
        }
        const FakeAudioContext = makeAudioContext(options);
        global.AudioContext = FakeAudioContext;
        global.LibrarySoundDevice.dmDeviceJSOpen(4);
        const context = FakeAudioContext.contexts[0];

        advanceTime(2000);
        assert.strictEqual(context.resumeCalls, 1);
        if (pending) {
            assert.strictEqual(global._dmJSDeviceShared.resumeAllowed, false);
            advanceTime(4000);
            assert.strictEqual(context.resumeCalls, 1);
            // A host may request a resume after gaining autoplay permission in the parent.
            delete options.resumeResult;
            global.DefoldSoundDevice.TryResumeAudio();
            assert.strictEqual(context.resumeCalls, 2);
            deferred.resolve();
        }
        assert.strictEqual(context.state, "running");
        assert.strictEqual(global.navigator.userActivation.hasBeenActive, false);
        assert.strictEqual(global._dmJSDeviceShared.resumeAllowed, true);
        assert.strictEqual(global._dmJSDeviceShared.resumePending, false);
        assert.strictEqual(timers.size, 0);
    }
}

// Verifies blocked iframe autoplay stays pending until activation instead of retrying on unrelated input.
function testBlockedIframeAutoplayWaitsForGesture() {
    resetEnvironment();
    global.navigator.userActivation.hasBeenActive = false;
    global.top = new Proxy({}, { get() { assert.fail("Inspected the host window"); } });
    const blockedResume = deferredResult();
    const options = { initialState: "suspended", resumeResult: blockedResume.promise };
    const FakeAudioContext = makeAudioContext(options);
    global.AudioContext = FakeAudioContext;
    global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const context = FakeAudioContext.contexts[0];

    advanceTime(6000);
    assert.strictEqual(context.resumeCalls, 1);
    assert.strictEqual(global._dmJSDeviceShared.resumePending, true);
    for (const event of [
        { type: "touchstart", isTrusted: true },
        { type: "touchcancel", isTrusted: true },
        { type: "keydown", key: "Escape", keyCode: 27, isTrusted: true },
        { type: "mousedown", isTrusted: false }
    ]) {
        global.DefoldSoundDevice.TryResumeAudio(event);
    }
    assert.strictEqual(context.resumeCalls, 1);
    assert.strictEqual(context.state, "suspended");

    global.navigator.userActivation.hasBeenActive = true;
    delete options.resumeResult;
    global.DefoldSoundDevice.TryResumeAudio({ type: "touchend", isTrusted: true });
    blockedResume.resolve();
    assert.strictEqual(context.resumeCalls, 2);
    assert.strictEqual(context.state, "running");
    assert.strictEqual(global._dmJSDeviceShared.resumePending, false);
    assert.strictEqual(timers.size, 0);
}

// Verifies an input gesture retries a pending resume and stale callbacks cannot override the newer attempt.
function testGestureRetriesPendingResume() {
    resetEnvironment();
    const firstResume = deferredResult();
    const options = { initialState: "suspended", resumeResult: firstResume.promise };
    const FakeAudioContext = makeAudioContext(options);
    global.AudioContext = FakeAudioContext;
    global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const context = FakeAudioContext.contexts[0];
    advanceTime(2000);
    assert.strictEqual(context.resumeCalls, 1);
    assert.strictEqual(global._dmJSDeviceShared.resumePending, true);

    const secondResume = deferredResult();
    options.resumeResult = secondResume.promise;
    global.DefoldSoundDevice.TryResumeAudio({ type: "keydown", isTrusted: true, key: " ", keyCode: 32 });
    assert.strictEqual(context.resumeCalls, 2);
    firstResume.reject();
    assert.strictEqual(global._dmJSDeviceShared.resumePending, true);
    context.state = "running";
    context.onstatechange();
    secondResume.resolve();
    assert.strictEqual(global._dmJSDeviceShared.resumePending, false);
    assert.strictEqual(timers.size, 0);
}

function testQueueFailureFallsBackAndRecovers() {
    resetEnvironment();
    const BrokenAudioContext = makeAudioContext({ throwOnQueue: true });
    global.AudioContext = BrokenAudioContext;
    const id = global.LibrarySoundDevice.dmDeviceJSOpen(4);
    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 4800);
    assert.strictEqual(global._dmJSDeviceShared.audioCtx, undefined);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 3);

    const RecoveredAudioContext = makeAudioContext();
    global.AudioContext = RecoveredAudioContext;
    advanceTime(2000);
    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 4800);
    assert.strictEqual(RecoveredAudioContext.contexts[0].startedBuffers, 1);
}

function testSuspendCancelsQueuedSourcesBeforeRecovery() {
    resetEnvironment();
    const FakeAudioContext = makeAudioContext();
    global.AudioContext = FakeAudioContext;
    const id = global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const context = FakeAudioContext.contexts[0];

    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 4800);
    const staleSource = context.sources[0];
    assert.strictEqual(staleSource.stopped, false);

    context.state = "suspended";
    context.onstatechange();
    assert.strictEqual(staleSource.stopped, true);
    assert.strictEqual(staleSource.disconnected, true);
    assert.strictEqual(global._dmJSDeviceShared.devices[id].activeSources.length, 0);

    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 4800);
    context.state = "running";
    context.onstatechange();
    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 4800);
    assert.strictEqual(context.sources.length, 2);
    assert.strictEqual(context.sources[1].stopped, false);
    assert.strictEqual(global._dmJSDeviceShared.devices[id].activeSources.length, 1);
}

function testResumeFailureAndClosedContextFallBack() {
    resetEnvironment();
    const ResumeFailureContext = makeAudioContext({ initialState: "suspended", throwOnResume: true });
    global.AudioContext = ResumeFailureContext;
    global.LibrarySoundDevice.dmDeviceJSOpen(4);
    global.DefoldSoundDevice.TryResumeAudio();
    assert.strictEqual(global._dmJSDeviceShared.audioCtx, undefined);
    assert.strictEqual(warnings.length, 1);

    resetEnvironment();
    const ClosingAudioContext = makeAudioContext();
    global.AudioContext = ClosingAudioContext;
    global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const context = ClosingAudioContext.contexts[0];
    const stateChangeHandler = context.onstatechange;
    context.state = "closed";
    stateChangeHandler();
    assert.strictEqual(global._dmJSDeviceShared.audioCtx, undefined);
    assert.strictEqual(warnings.length, 1);
}

function testStaleResumeResultDoesNotAffectRecoveredContext() {
    resetEnvironment();
    const staleResume = deferredResult();
    const StaleAudioContext = makeAudioContext({
        initialState: "suspended",
        resumeResult: staleResume.promise
    });
    global.AudioContext = StaleAudioContext;
    global.LibrarySoundDevice.dmDeviceJSOpen(4);
    global.DefoldSoundDevice.TryResumeAudio();

    const staleContext = StaleAudioContext.contexts[0];
    staleContext.state = "closed";
    staleContext.onstatechange();

    const recoveredResume = deferredResult();
    const RecoveredAudioContext = makeAudioContext({
        initialState: "suspended",
        resumeResult: recoveredResume.promise
    });
    global.AudioContext = RecoveredAudioContext;
    global.DefoldSoundDevice.TryResumeAudio();

    const recoveredContext = RecoveredAudioContext.contexts[0];
    assert.strictEqual(global._dmJSDeviceShared.resumePending, true);
    staleResume.reject();
    assert.strictEqual(global._dmJSDeviceShared.resumePending, true);

    advanceTime(2000);
    assert.strictEqual(recoveredContext.resumeCalls, 1);

    recoveredContext.state = "running";
    recoveredContext.onstatechange();
    recoveredResume.resolve();
    assert.strictEqual(global._dmJSDeviceShared.resumePending, false);
}

function testSharedContextAndCleanup() {
    resetEnvironment();
    const FakeAudioContext = makeAudioContext();
    global.AudioContext = FakeAudioContext;
    const first = global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const second = global.LibrarySoundDevice.dmDeviceJSOpen(4);
    assert.strictEqual(FakeAudioContext.contexts.length, 1);

    const context = FakeAudioContext.contexts[0];
    global.LibrarySoundDevice.dmDeviceJSQueue(first, 0, 4800);
    global.LibrarySoundDevice.dmDeviceJSQueue(second, 0, 4800);
    global.LibrarySoundDevice.dmDeviceJSClose(first);
    assert.strictEqual(context.sources[0].stopped, true);
    assert.strictEqual(context.sources[1].stopped, false);
    assert.strictEqual(context.closed, false);
    assert.notStrictEqual(deviceChangeHandler, null);

    global.LibrarySoundDevice.dmDeviceJSClose(second);
    assert.strictEqual(context.sources[1].stopped, true);
    assert.strictEqual(context.closed, true);
    assert.strictEqual(deviceChangeHandler, null);
    assert.strictEqual(global._dmJSDeviceShared, undefined);
    assert.strictEqual(timers.size, 0);
}

// Verifies that a frozen running clock releases every device's queue and recovers, preventing instance exhaustion.
function testStalledRunningClockFallsBackAndRecovers() {
    resetEnvironment();
    const FakeAudioContext = makeAudioContext();
    global.AudioContext = FakeAudioContext;
    const first = global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const second = global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const context = FakeAudioContext.contexts[0];
    context.frozenTime = context.currentTime;

    global.LibrarySoundDevice.dmDeviceJSQueue(first, 0, 441);
    global.LibrarySoundDevice.dmDeviceJSQueue(second, 0, 441);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(first), 0);
    advanceTime(100);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(first), 0);
    assert.strictEqual(context.closed, false);

    // Repeated interaction with a context reporting the same state must not postpone detection.
    global.DefoldSoundDevice.TryResumeAudio();
    advanceTime(101);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(first), 4);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(second), 4);
    assert.strictEqual(global._dmJSDeviceShared.audioCtx, undefined);
    assert.strictEqual(context.closed, true);
    assert.ok(context.sources.every(source => source.stopped && source.disconnected));
    assert.strictEqual(timers.size, 1);
    assert.strictEqual(warnings.length, 1);

    global.LibrarySoundDevice.dmDeviceJSQueue(first, 0, 441);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(first), 3);
    advanceTime(100);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(first), 4);
    advanceTime(1900);
    const recoveredContext = FakeAudioContext.contexts[1];
    assert.strictEqual(global._dmJSDeviceShared.audioCtx, recoveredContext);
    assert.strictEqual(recoveredContext.currentTime, 0);
    global.LibrarySoundDevice.dmDeviceJSQueue(first, 0, 441);
    advanceTime(100);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(first), 4);
    assert.strictEqual(recoveredContext.startedBuffers, 1);
    assert.strictEqual(timers.size, 0);
}

// Verifies that 100 ms clock quantization keeps playback advancing without unnecessary context recreation.
function testAudioClockWatchdogAllowsCoarseProgress() {
    resetEnvironment();
    const FakeAudioContext = makeAudioContext();
    global.AudioContext = FakeAudioContext;
    const id = global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const context = FakeAudioContext.contexts[0];

    for (let i = 0; i < 60; ++i) {
        context.frozenTime = Math.floor(nowMs / 100) / 10;
        const slots = global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id);
        for (let slot = 0; slot < slots; ++slot) {
            global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 441);
        }
        advanceTime(1000 / 60);
    }

    assert.ok(context.startedBuffers > 4);
    assert.strictEqual(context.closed, false);
    assert.strictEqual(FakeAudioContext.contexts.length, 1);
    assert.strictEqual(warnings.length, 0);
}

// Verifies that idle time and suspension do not count toward the stall timeout on playback restart or resume.
function testAudioClockWatchdogResetsAfterIdleAndResume() {
    resetEnvironment();
    const FakeAudioContext = makeAudioContext({ resumeResult: deferredResult().promise });
    global.AudioContext = FakeAudioContext;
    const id = global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const context = FakeAudioContext.contexts[0];
    context.frozenTime = context.currentTime;
    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 441);

    global.LibrarySoundDevice.dmDeviceJSPlaybackIdle(id);
    advanceTime(10000);
    global.LibrarySoundDevice.dmDeviceJSPlaybackStarted(id);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 0);
    assert.strictEqual(context.closed, false);

    context.state = "suspended";
    context.onstatechange();
    advanceTime(5000);
    context.state = "running";
    context.onstatechange();
    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 441);
    advanceTime(100);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 0);
    assert.strictEqual(context.closed, false);
    advanceTime(101);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 4);
    assert.strictEqual(context.closed, true);
}

// Verifies that system clock adjustments neither trigger false stalls nor stop silent playback from advancing.
function testAudioClockWatchdogIgnoresWallClockChanges() {
    resetEnvironment();
    const FakeAudioContext = makeAudioContext();
    global.AudioContext = FakeAudioContext;
    const id = global.LibrarySoundDevice.dmDeviceJSOpen(4);
    const context = FakeAudioContext.contexts[0];
    context.frozenTime = context.currentTime;
    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 441);

    wallClockOffsetMs = 3600000;
    advanceTime(100);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 0);
    assert.strictEqual(context.closed, false);
    wallClockOffsetMs = -3600000;
    advanceTime(101);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 4);
    assert.strictEqual(context.closed, true);

    global.LibrarySoundDevice.dmDeviceJSQueue(id, 0, 441);
    wallClockOffsetMs = -7200000;
    advanceTime(100);
    assert.strictEqual(global.LibrarySoundDevice.dmDeviceJSFreeBufferSlots(id), 4);
}

installEnvironment();
testSilentQueueAdvances();
testSilentQueueRestartsAfterIdle();
testConstructorFailureStaysSilent();
testPeriodicRecoveryKeepsDeviceRate();
testInteractionAndDeviceChangeRecoverImmediately();
testAutoplayWaitsForActivation();
testAutoplayWithoutActivationAPI();
testActivationSurvivesContextFailure();
testPermittedAutoplayRecoversWithoutActivation();
testIframeAutoplayWithoutLocalActivation();
testBlockedIframeAutoplayWaitsForGesture();
testGestureRetriesPendingResume();
testQueueFailureFallsBackAndRecovers();
testSuspendCancelsQueuedSourcesBeforeRecovery();
testResumeFailureAndClosedContextFallBack();
testStaleResumeResultDoesNotAffectRecoveredContext();
testSharedContextAndCleanup();
testStalledRunningClockFallsBackAndRecovers();
testAudioClockWatchdogAllowsCoarseProgress();
testAudioClockWatchdogResetsAfterIdleAndResume();
testAudioClockWatchdogIgnoresWallClockChanges();

process.stdout.write("HTML5 sound device tests passed\n");
