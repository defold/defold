const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const source = fs.readFileSync(path.join(__dirname, "../js/library_profile.js"), "utf8");

function createProfiler(enableTimeline) {
    const names = new Map([[10, "Frame"], [20, "Update"], [30, "Draw"]]);
    const decodes = [];
    const events = [];
    let time = 0;
    const context = {
        performance: {
            now: () => ++time,
            beginMeasure: name => events.push(["begin", name]),
            endMeasure: name => events.push(["end", name])
        },
        UTF8ToString(pointer) {
            decodes.push(pointer);
            return names.get(pointer);
        },
        autoAddDeps() {},
        addToLibrary(library) {
            context.DefoldProfiler = library.$DefoldProfiler;
            context.library = library;
        }
    };
    vm.runInNewContext(source, context);
    const lib = context.library;
    assert.ok(lib.dmProfileJSInit(enableTimeline));
    return { lib, names, decodes, events };
}

// Repeated siblings aggregate calls and reused records start each frame with an empty tree and fresh timings.
function testNestedScopesAcrossFrames() {
    const { lib, decodes } = createProfiler(false);
    const root = lib.dmProfileJSBeginMark(0, 10, 1, 2);
    const child = lib.dmProfileJSBeginMark(0, 20, 3, 4);
    assert.equal(lib.dmProfileJSEndMark(0), false);
    assert.equal(lib.dmProfileJSBeginMark(0, 20, 3, 4), child);
    lib.dmProfileJSEndMark(0);
    assert.equal(lib.dmProfileJSEndMark(0), true);
    assert.equal(lib.dmProfileJSFirstChild(0, 0), root);
    assert.equal(lib.dmProfileJSFirstChild(0, root), child);
    assert.equal(lib.dmProfileJSGetCallCount(0, child), 2);
    assert.equal(lib.dmProfileJSGetStart(0, root), 1);
    assert.equal(lib.dmProfileJSGetDuration(0, root), 5);
    assert.equal(lib.dmProfileJSGetSelfDuration(0, root), 3);

    lib.dmProfileJSReset(0);
    assert.equal(lib.dmProfileJSFirstChild(0, 0), -1);
    assert.throws(() => lib.dmProfileJSGetName(0, child));
    assert.throws(() => lib.dmProfileJSGetName(0, child + 1));
    const nextRoot = lib.dmProfileJSBeginMark(0, 10, 1, 2);
    assert.ok(nextRoot > child);
    assert.equal(lib.dmProfileJSFirstChild(0, nextRoot), -1);
    assert.equal(lib.dmProfileJSGetDuration(0, nextRoot), 0);
    const nextChild = lib.dmProfileJSBeginMark(0, 20, 3, 4);
    assert.equal(lib.dmProfileJSGetName(0, nextChild), 20);
    assert.equal(lib.dmProfileJSGetCallCount(0, nextChild), 1);
    lib.dmProfileJSEndMark(0);
    lib.dmProfileJSEndMark(0);
    assert.deepEqual(decodes, [10, 20]);
}

// Two names sharing a scratch buffer stay distinct with or without a hash, even if just the high bits change.
function testMutableScopeNames() {
    const { lib, names, events } = createProfiler(true);
    lib.dmProfileJSBeginMark(0, 10, 1, 2);
    const first = lib.dmProfileJSBeginMark(0, 20, 0, 0);
    lib.dmProfileJSEndMark(0);
    names.set(20, "Physics");
    const second = lib.dmProfileJSBeginMark(0, 20, 0, 0);
    lib.dmProfileJSEndMark(0);
    assert.equal(lib.dmProfileJSNextSibling(0, first), second);
    assert.equal(lib.dmProfileJSBeginMark(0, 20, 0, 0), second);
    lib.dmProfileJSEndMark(0);
    assert.equal(lib.dmProfileJSBeginMark(0, 20, 5, 6), second);
    lib.dmProfileJSEndMark(0);
    assert.equal(lib.dmProfileJSGetCallCount(0, second), 3);
    names.set(20, "Render");
    lib.dmProfileJSBeginMark(0, 20, 5, 7);
    lib.dmProfileJSReset(0);
    assert.deepEqual(events.slice(-3), [["begin", "Render"], ["end", "Render"], ["end", "Frame"]]);

    lib.dmProfileJSBeginMark(0, 10, 1, 2);
    lib.dmProfileJSBeginMark(0, 20, 5, 7);
    lib.dmProfileJSReset(0);
    names.set(20, "Input");
    lib.dmProfileJSBeginMark(0, 10, 1, 2);
    lib.dmProfileJSBeginMark(0, 20, 5, 8);
    assert.deepEqual(events.at(-1), ["begin", "Input"]);
}

// Resetting one thread, including after a smaller frame, must leave the other thread's open scopes intact.
function testPerThreadScopeLifetime() {
    const { lib } = createProfiler(false);
    lib.dmProfileJSBeginMark(1, 10, 1, 2);
    lib.dmProfileJSBeginMark(1, 20, 3, 4);
    const other = lib.dmProfileJSBeginMark(2, 30, 5, 6);
    lib.dmProfileJSReset(1);
    const single = lib.dmProfileJSBeginMark(1, 10, 1, 2);
    assert.equal(lib.dmProfileJSFirstChild(1, single), -1);
    lib.dmProfileJSReset(1);
    assert.equal(lib.dmProfileJSFirstChild(2, 0), other);
    assert.equal(lib.dmProfileJSEndMark(2), true);
    assert.equal(lib.dmProfileJSGetDuration(2, other), 2);
}

testNestedScopesAcrossFrames();
testMutableScopeNames();
testPerThreadScopeLifetime();
console.log("Web profiler scope tests passed");
