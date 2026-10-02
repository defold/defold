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

// Verify real DOM input and rendering reach C++, and teardown prevents stale or duplicate callbacks after recreation.
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
    expect(closed).toMatchObject({ opened: 0, closes: 1 });
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
    await page.evaluate(() => Module._TestClose());
    expect((await state(page)).closes).toBe(2);
});

// Verify termination/reinitialization resynchronizes actual fullscreen and pointer lock, including changes while stopped.
test("fullscreen and pointer lock recover across teardown", async ({ page }) => {
    await openWindow(page);
    await page.locator("#fullscreen").click();
    await page.waitForFunction(() => document.fullscreenElement === Module.fullScreenContainer);
    await page.locator("#lock").click();
    await page.waitForFunction(() => document.pointerLockElement === Module.canvas);
    expect(await state(page)).toMatchObject({ fullscreen: true, locked: 1 });

    await page.evaluate(() => Module._TestClose());
    await openWindow(page);
    expect(await state(page)).toMatchObject({ fullscreen: true, locked: 1 });
    const fullscreenSize = await page.evaluate(() => ({
        width: Math.floor(innerWidth * devicePixelRatio), height: Math.floor(innerHeight * devicePixelRatio)
    }));
    expect(await state(page)).toMatchObject(fullscreenSize);
    await page.evaluate(() => Module._TestSetCursorVisible(1));
    await expect.poll(async () => (await state(page)).locked).toBe(0);
    await page.evaluate(() => document.exitFullscreen());
    await expect.poll(async () => (await state(page)).fullscreen).toBe(false);

    await page.locator("#fullscreen").click();
    await page.waitForFunction(() => document.fullscreenElement === Module.fullScreenContainer);
    await page.locator("#lock").click();
    await page.waitForFunction(() => document.pointerLockElement === Module.canvas);
    await page.evaluate(() => Module._TestClose());
    await page.evaluate(() => document.exitPointerLock());
    await page.evaluate(() => document.exitFullscreen());
    await page.waitForFunction(() => !document.fullscreenElement && !document.pointerLockElement);
    await page.evaluate(() => {
        Module.canvas.width = 640;
        Module.canvas.height = 480;
    });
    await openWindow(page);
    expect(await state(page)).toMatchObject({ fullscreen: false, locked: 0, width: 640, height: 480 });
    await page.evaluate(() => Module._TestClose());
});
