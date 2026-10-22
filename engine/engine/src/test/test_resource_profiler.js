const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const html = fs.readFileSync(path.join(__dirname, "../../content/profiler.html"), "utf8");

// Only DOM operations used by the resource table are needed; execute the page's actual JavaScript.
function element(attributes = {}) {
    const classes = new Set((attributes.class || "").split(/\s+/));
    return {
        id: attributes.id, children: [], style: {},
        classList: {
            contains(name) { return classes.has(name); },
            toggle(name, enabled) { enabled ? classes.add(name) : classes.delete(name); }
        },
        getAttribute(name) { return attributes[name]; },
        setAttribute(name, value) { attributes[name] = String(value); },
        appendChild(child) { this.children.push(child); },
        set innerHTML(value) { this.children = []; this.html = value; },
        get innerHTML() { return this.html; }
    };
}

function createPage() {
    const nodes = new Map();
    let button;
    for (const tag of html.matchAll(/<(?:th|td|div|tbody|input)\b([^>]*)>/g)) {
        const attributes = Object.fromEntries(Array.from(tag[1].matchAll(/([\w-]+)="([^"]*)"/g), match => [match[1], match[2]]));
        const node = element(attributes);
        if (attributes.id) nodes.set(attributes.id, node);
        if (attributes.value === "Show load times") button = node;
    }
    const page = vm.createContext({
        console: { log() {} },
        alert(message) { assert.fail(message); },
        XMLHttpRequest: function() {
            this.open = (method, url) => { this.url = url; };
            this.overrideMimeType = () => {};
            this.send = () => {};
        },
        document: {
            getElementById(id) { return nodes.get(id); },
            createElement() { return element(); }
        }
    });
    vm.runInContext(html.match(/<script type="text\/javascript">([\s\S]*?)<\/script>/)[1], page);
    return { page, nodes, button };
}

function string(value) {
    const bytes = Buffer.from(value);
    const length = Buffer.alloc(2);
    length.writeUInt16LE(bytes.length);
    return Buffer.concat([length, bytes]);
}

function record(name, size, timings, extended = true) {
    const fields = Buffer.alloc(12);
    fields.writeUInt32LE(size);
    fields.writeUInt32LE(128, 4);
    fields.writeUInt32LE(2, 8);
    const parts = [string(name), string(".test"), fields];
    if (extended) {
        const extra = Buffer.alloc(64);
        extra.writeUInt32LE(timings ? 1 : 0);
        if (timings) {
            timings.forEach((value, index) => extra.writeBigUInt64LE(BigInt(value), 4 + 8 * index));
            extra.writeUInt32LE(17, 60);
        }
        parts.push(extra);
    }
    return Buffer.concat(parts);
}

function capture(page, type, records) {
    vm.runInContext("captureResources()", page);
    assert.strictEqual(page.requestResources.url, "/resources_data?timings=1");
    page.requestResources.readyState = 4;
    page.requestResources.status = 200;
    page.requestResources.responseText = Buffer.concat([string(type), ...records]).toString("latin1");
    vm.runInContext("requestResources.onreadystatechange()", page);
}

// Old engines' RESS responses must still decode, with no timing measurement invented for their rows.
function testLegacyResponse() {
    const { page, nodes } = createPage();
    capture(page, "RESS", [record("/legacy", 256, null, false), record("/other", 64, null, false)]);
    assert.strictEqual(page.framesResources.length, 2);
    assert.strictEqual(page.resourceItems[0].name, "/legacy");
    assert.strictEqual(page.resourceItems[0].sizeOnDisc, 128);
    assert.strictEqual(page.resourceItems[0].referenceCount, 2);
    assert.strictEqual(page.resourceItems[0].loadTotal, null);
    assert.strictEqual(nodes.get("resource_table_body").children[0].children[5].innerText, "—");
}

// RES2 preserves field order and 64-bit durations; unavailable and measured-zero records cannot shift later rows.
function testExtendedResponse() {
    const { page, nodes } = createPage();
    capture(page, "RES2", [
        record("/unavailable", 512, null),
        record("/zero", 256, [0, 0, 0, 0, 0, 0, 0]),
        record("/slow", 128, [4294967300, 11, 23, 37, 41, 53, 4294967135])
    ]);
    const slow = page.framesResources[2];
    assert.deepStrictEqual(
        [slow.name, slow.loadTotal, slow.loadPreload, slow.loadRead, slow.loadCreate, slow.loadPostCreate, slow.loadQueue, slow.loadWait, slow.postCreateCount],
        ["/slow", 4294967300, 11, 23, 37, 41, 53, 4294967135, 17]);
    const rows = nodes.get("resource_table_body").children;
    assert.strictEqual(rows.length, 3);
    assert.strictEqual(rows[0].children[5].innerText, "—");
    assert.strictEqual(rows[1].children[5].innerText, "0.000");
    assert.strictEqual(rows[2].children[5].innerText, "4294967.300");
    assert.strictEqual(rows[2].children[6].innerText, "0.011");
    assert.strictEqual(rows[2].children[7].innerText, "0.023");
    assert.strictEqual(rows[2].children[8].innerText, "0.037");
    assert.strictEqual(rows[2].children.length, 9);
    assert.match(rows[2].children[5].style.background, /100%/);
}

// Showing timings ranks slow loads first, recapture preserves sorting, and hiding never leaves a hidden sort selected.
function testTimingControls() {
    const { page, nodes, button } = createPage();
    const records = [record("/unknown", 512, null), record("/zero", 256, [0, 0, 0, 0, 0, 0, 0]), record("/slow", 128, [12300, 100, 200, 300, 400, 500, 10800])];
    const order = () => nodes.get("resource_table_body").children.map(row => row.children[0].innerText);
    capture(page, "RES2", records);
    assert.strictEqual(button.getAttribute("aria-expanded"), "false");
    assert(!nodes.get("tab_resources").classList.contains("show-load-times"));
    page.toggleResourceLoadTimes(button);
    assert.strictEqual(button.getAttribute("aria-expanded"), "true");
    assert.strictEqual(button.value, "Hide load times");
    assert(nodes.get("tab_resources").classList.contains("show-load-times"));
    assert.deepStrictEqual(order(), ["/slow", "/zero", "/unknown"]);
    page.setResourceSortPredicate(nodes.get("resources_load_label"));
    assert.deepStrictEqual(order(), ["/zero", "/slow", "/unknown"]);
    capture(page, "RES2", records);
    assert.deepStrictEqual(order(), ["/zero", "/slow", "/unknown"]);
    page.toggleResourceLoadTimes(button);
    assert.strictEqual(button.value, "Show load times");
    assert(!nodes.get("tab_resources").classList.contains("show-load-times"));
    assert.strictEqual(page.sortResourcesSettings.id, "resources_size_label");
    assert.deepStrictEqual(order(), ["/unknown", "/zero", "/slow"]);
    page.toggleResourceLoadTimes(button);
    page.setResourceSortPredicate(nodes.get("resources_resource_label"));
    page.toggleResourceLoadTimes(button);
    assert.strictEqual(page.sortResourcesSettings.id, "resources_resource_label");
}

for (const test of [testLegacyResponse, testExtendedResponse, testTimingControls]) {
    test();
    console.log(test.name + " PASS");
}
