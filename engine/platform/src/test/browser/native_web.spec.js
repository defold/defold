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

const { test: base, expect } = require("@playwright/test");
const { readFile } = require("node:fs/promises");
const { createServer } = require("node:http");
const path = require("node:path");

const test = base.extend({
    testUrl: [async ({}, use) => {
        if (!process.env.DEFOLD_BROWSER_TEST_JS)
            throw new Error("Run the CMake target run_test_native_web_browser to select the compiled test.");
        const binary = path.resolve(process.env.DEFOLD_BROWSER_TEST_JS);
        const routes = new Map([
            ["/", [path.join(__dirname, "index.html"), "text/html"]],
            ["/test_native_web_browser.js", [binary, "text/javascript"]],
            ["/test_native_web_browser.wasm", [binary.replace(/\.js$/, ".wasm"), "application/wasm"]]
        ]);
        const server = createServer(async (request, response) => {
            const route = routes.get(request.url);
            if (!route) {
                response.writeHead(404).end();
                return;
            }
            try {
                const body = await readFile(route[0]);
                response.writeHead(200, { "Content-Type": route[1] }).end(body);
            } catch (error) {
                response.writeHead(500).end(String(error));
            }
        });
        await new Promise((resolve, reject) => {
            server.once("error", reject);
            server.listen(0, "127.0.0.1", resolve);
        });
        try {
            await use(`http://127.0.0.1:${server.address().port}/`);
        } finally {
            await new Promise(resolve => server.close(resolve));
        }
    }, { scope: "worker" }],
    page: async ({ page, testUrl }, use, testInfo) => {
        const messages = [];
        const errors = [];
        page.on("console", message => {
            messages.push(`[${message.type()}] ${message.text()}`);
            if (message.type() === "error") errors.push(message.text());
        });
        page.on("pageerror", error => errors.push(error.stack || String(error)));
        page.on("crash", () => errors.push("Browser page crashed"));
        page.on("requestfailed", request => errors.push(`${request.url()}: ${request.failure().errorText}`));
        try {
            await page.goto(testUrl);
            await page.waitForFunction(() => Module.ready);
            await use(page);
        } finally {
            await testInfo.attach("browser.log", {
                body: [...messages, ...errors].join("\n"), contentType: "text/plain"
            });
            expect(errors, "Browser errors").toEqual([]);
        }
    }
});

async function state(page) {
    return page.evaluate(() => {
        Module._TestSnapshot();
        return Module.testState;
    });
}

async function openWindow(page) {
    expect(await page.evaluate(() => Module._TestOpen())).toBe(0);
    await page.evaluate(() => Module._TestFrame());
    expect(await page.evaluate(() => Module._TestRender())).toBe(1);
}

async function browserStateChange(page, eventName, action) {
    await page.evaluate(eventName => {
        Module.browserStateChanged = new Promise(resolve => {
            // Register before the action; backend capture listeners run before this listener.
            document.addEventListener(eventName, () => resolve(), { once: true });
        });
    }, eventName);
    await action();
    await page.evaluate(() => Module.browserStateChanged);
}

// Verify recreation preserves input and rendering, while only a close request emits the engine's close callback.
test("rendering, resize and input survive window recreation", async ({ page }) => {
    const canvas = page.locator("#canvas");
    await openWindow(page);
    expect(await state(page)).toMatchObject({ opened: 1, width: 640, height: 480, scale: 2 });
    await canvas.focus();
    const initial = await state(page);
    await page.keyboard.down("a");
    expect(await state(page)).toMatchObject({ key: 1, characters: initial.characters + 1, lastChar: 97 });
    const box = await canvas.boundingBox();
    await page.mouse.move(box.x + 64, box.y + 48);
    await page.mouse.down();
    expect(await state(page)).toMatchObject({ button: 1, x: 128, y: 96 });

    // Moving focus must release held keys and mouse buttons even when their keyup/mouseup is missed.
    await page.locator("#outside").focus();
    expect(await state(page)).toMatchObject({ key: 0, button: 0, focused: 0, focuses: initial.focuses + 1 });
    await page.keyboard.up("a");
    await page.mouse.up();
    await page.evaluate(() => {
        Module.canvas.width = 800;
        Module.canvas.height = 600;
        Module._TestFrame();
    });
    expect(await state(page)).toMatchObject({ width: 800, height: 600, resizes: initial.resizes + 1 });
    await page.evaluate(() => Module._TestFrame());
    expect((await state(page)).resizes).toBe(initial.resizes + 1);
    expect(await page.evaluate(() => Module._TestRender())).toBe(1);

    await page.evaluate(() => Module._TestClose());
    const closed = await state(page);
    expect(closed).toMatchObject({ opened: 0, closes: 0 });
    await canvas.focus();
    await page.keyboard.press("a");
    expect(await state(page)).toEqual(closed);

    await openWindow(page);
    await page.locator("#outside").focus();
    const reopened = await state(page);
    await canvas.focus();
    await page.keyboard.press("a");
    expect(await state(page)).toMatchObject({
        opened: 1, key: 0, button: 0, focused: 1,
        focuses: reopened.focuses + 1, characters: reopened.characters + 1
    });
    expect(await page.evaluate(() => Module._TestRequestClose())).toBe(1);
    expect((await state(page)).closes).toBe(1);
    await page.evaluate(() => Module._TestClose());
    expect((await state(page)).closes).toBe(1);
});

// Synchronous buffer mutations must notify native code even when zero-size pulses restore the original dimensions.
test("canvas mutations invalidate frames before the browser microtask", async ({ page }) => {
    await openWindow(page);
    let previous = await state(page);
    for (const mutation of ["zero", "restore", "pulse", "attribute", "same-value"]) {
        const current = await page.evaluate(mutation => {
            const canvas = Module.canvas;
            if (mutation === "zero") {
                canvas.width = 0;
                canvas.height = 0;
            } else if (mutation === "restore") {
                canvas.width = 640;
                canvas.height = 480;
            } else if (mutation === "pulse") {
                canvas.width = 0;
                canvas.height = 0;
                canvas.width = 640;
                canvas.height = 480;
            } else if (mutation === "attribute") {
                canvas.setAttribute("width", "0");
                canvas.setAttribute("width", "640");
            } else {
                canvas.width = canvas.width;
            }
            Module._TestFrame();
            Module._TestSnapshot();
            return Module.testState;
        }, mutation);
        expect(current).toMatchObject({ resizes: previous.resizes + 1,
            width: mutation === "zero" ? 0 : 640, height: mutation === "zero" ? 0 : 480 });
        await page.evaluate(() => Module._TestFrame());
        expect((await state(page)).resizes).toBe(current.resizes);
        previous = current;
    }
    expect(await page.evaluate(() => Module._TestRender())).toBe(1);
    await page.evaluate(() => Module._TestClose());
});

// Microtask-delivered resize records must survive until polling, while CSS-only changes do not invalidate the buffer.
test("canvas mutation notifications are consumed once", async ({ page }) => {
    await openWindow(page);
    const initial = await state(page);
    await page.evaluate(() => { Module.canvas.width = Module.canvas.width; });
    await page.evaluate(() => Module._TestFrame());
    expect((await state(page)).resizes).toBe(initial.resizes + 1);
    await page.evaluate(() => {
        Module.canvas.style.width = "300px";
        Module.canvas.setAttribute("data-test", "unchanged-buffer");
    });
    await page.evaluate(() => Module._TestFrame());
    expect((await state(page)).resizes).toBe(initial.resizes + 1);
    await page.evaluate(() => Module._TestClose());
});

// A canvas bound to WebGPU must be replaced before WebGL fallback, preserving layout, focus, input, and rendering.
test("WebGPU canvas can fall back to WebGL", async ({ page }) => {
    expect(await page.evaluate(() => Module._TestOpenWebGPU())).toBe(0);
    await page.locator("#canvas").focus();
    expect(await page.evaluate(() => {
        Module.previousCanvas = Module.canvas;
        Module.canvas.setAttribute("data-fallback-test", "preserved");
        Module.canvas.style.borderRadius = "4px";
        Module.fullScreenContainer = Module.canvas;
        // Simulate graphics initialization failing after acquiring the surface.
        const context = Module.canvas.getContext("webgpu");
        return !!context && Module.canvas.getContext("webgl2") === null && Module.canvas.getContext("webgl") === null;
    })).toBe(true);

    await openWindow(page);
    expect(await page.evaluate(() => ({
        replaced: Module.canvas !== Module.previousCanvas && !Module.previousCanvas.isConnected,
        current: document.getElementById("canvas") === Module.canvas,
        fullscreen: Module.fullScreenContainer === Module.canvas,
        focused: document.activeElement === Module.canvas,
        attribute: Module.canvas.getAttribute("data-fallback-test"),
        tabIndex: Module.canvas.tabIndex,
        style: Module.canvas.style.borderRadius
    }))).toEqual({ replaced: true, current: true, fullscreen: true, focused: true,
        attribute: "preserved", tabIndex: 0, style: "4px" });
    const initial = await state(page);
    expect(initial).toMatchObject({ opened: 1, width: 640, height: 480, scale: 2, closes: 0 });
    await page.keyboard.down("a");
    expect(await state(page)).toMatchObject({ key: 1, characters: initial.characters + 1 });
    const box = await page.locator("#canvas").boundingBox();
    await page.mouse.move(box.x + 64, box.y + 48);
    await page.mouse.down();
    expect(await state(page)).toMatchObject({ button: 1, x: 128, y: 96 });
    await page.locator("#outside").focus();
    expect(await state(page)).toMatchObject({ key: 0, button: 0, focused: 0, focuses: initial.focuses + 1 });
    await page.keyboard.up("a");
    await page.mouse.up();
    await page.locator("#canvas").focus();
    expect(await state(page)).toMatchObject({ focused: 1, focuses: initial.focuses + 2 });
    expect(await page.evaluate(() => Module._TestRender())).toBe(1);
    await page.evaluate(() => Module._TestClose());
    expect((await state(page)).closes).toBe(0);
});

// Verify a loader-selected ratio is reported as display scale and survives fullscreen without duplicate resize events (#8040).
test("fullscreen respects the loader pixel ratio", async ({ page }) => {
    await page.evaluate(() => {
        Module.getPixelRatio = highDPI => highDPI ? Math.min(devicePixelRatio, 1.5) : 1;
        Module.canvas.width = 480;
        Module.canvas.height = 360;
    });
    await openWindow(page);
    expect(await state(page)).toMatchObject({ width: 480, height: 360, scale: 1.5 });
    const box = await page.locator("#canvas").boundingBox();
    await page.mouse.move(box.x + 64, box.y + 48);
    expect(await state(page)).toMatchObject({ x: 96, y: 72 });

    await browserStateChange(page, "fullscreenchange", () => page.locator("#fullscreen").click());
    await page.evaluate(() => Module._TestFrame());
    const expectedSize = await page.evaluate(() => ({
        width: Math.floor(innerWidth * 1.5), height: Math.floor(innerHeight * 1.5)
    }));
    const fullscreen = await state(page);
    expect(fullscreen).toMatchObject({ ...expectedSize, fullscreen: true, scale: 1.5 });
    await page.evaluate(() => Module._TestFrame());
    expect((await state(page)).resizes).toBe(fullscreen.resizes);
    expect(await page.evaluate(() => Module._TestRender())).toBe(1);

    await browserStateChange(page, "fullscreenchange", () => page.evaluate(() => document.exitFullscreen()));
    await page.evaluate(() => Module._TestClose());
});

// Verify fullscreen and pointer lock survive recreation, waiting for change events to guard against stale backend state.
test("fullscreen and pointer lock recover across teardown", async ({ page }) => {
    await openWindow(page);
    await browserStateChange(page, "fullscreenchange", () => page.locator("#fullscreen").click());
    expect(await page.evaluate(() => document.fullscreenElement === Module.fullScreenContainer)).toBe(true);
    await browserStateChange(page, "pointerlockchange", () => page.locator("#lock").click());
    expect(await page.evaluate(() => document.pointerLockElement === Module.canvas)).toBe(true);
    expect(await state(page)).toMatchObject({ fullscreen: true, locked: 1 });

    await page.evaluate(() => Module._TestClose());
    await openWindow(page);
    expect(await state(page)).toMatchObject({ fullscreen: true, locked: 1 });
    const fullscreenSize = await page.evaluate(() => ({
        width: Math.floor(innerWidth * devicePixelRatio), height: Math.floor(innerHeight * devicePixelRatio)
    }));
    expect(await state(page)).toMatchObject(fullscreenSize);
    await browserStateChange(page, "pointerlockchange", () => page.evaluate(() => Module._TestSetCursorVisible(1)));
    expect((await state(page)).locked).toBe(0);
    await browserStateChange(page, "fullscreenchange", () => page.evaluate(() => document.exitFullscreen()));
    expect((await state(page)).fullscreen).toBe(false);

    await browserStateChange(page, "fullscreenchange", () => page.locator("#fullscreen").click());
    expect(await page.evaluate(() => document.fullscreenElement === Module.fullScreenContainer)).toBe(true);
    await browserStateChange(page, "pointerlockchange", () => page.locator("#lock").click());
    expect(await page.evaluate(() => document.pointerLockElement === Module.canvas)).toBe(true);
    await page.evaluate(() => Module._TestClose());
    await browserStateChange(page, "pointerlockchange", () => page.evaluate(() => document.exitPointerLock()));
    await browserStateChange(page, "fullscreenchange", () => page.evaluate(() => document.exitFullscreen()));
    expect(await page.evaluate(() => !document.fullscreenElement && !document.pointerLockElement)).toBe(true);
    await page.evaluate(() => {
        Module.canvas.width = 640;
        Module.canvas.height = 480;
    });
    await openWindow(page);
    expect(await state(page)).toMatchObject({ fullscreen: false, locked: 0, width: 640, height: 480 });
    await page.evaluate(() => Module._TestClose());
});
