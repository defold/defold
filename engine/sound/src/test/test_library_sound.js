const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

let nowMs = 0;
let nextTimerId = 1;
let timers = new Map();
let warnings = [];
let infos = [];
let deviceChangeHandler = null;

function installEnvironment() {
    global.Date.now = () => nowMs;
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
    nowMs = 0;
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
            contexts.push(this);
        }
        get currentTime() {
            return nowMs / 1000;
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

installEnvironment();
testSilentQueueAdvances();
testSilentQueueRestartsAfterIdle();
testConstructorFailureStaysSilent();
testPeriodicRecoveryKeepsDeviceRate();
testInteractionAndDeviceChangeRecoverImmediately();
testQueueFailureFallsBackAndRecovers();
testSuspendCancelsQueuedSourcesBeforeRecovery();
testResumeFailureAndClosedContextFallBack();
testStaleResumeResultDoesNotAffectRecoveredContext();
testSharedContextAndCleanup();

process.stdout.write("HTML5 sound device tests passed\n");
