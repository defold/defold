// Focused lifecycle and pixel checks for the broad web component ownership handoff.
const { chromium } = require('playwright');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const { PNG } = require('pngjs');

// Pixel identity alone could pass with every new component invisible. Verify
// each geometry path actually drew its expected color after buffer replacement.
function checkGeometry(png, motion = false) {
    const image = PNG.sync.read(png);
    const probes = [
        ['world mesh', 52, [255, 255, 0]], ['local mesh', 132, [0, 255, 255]],
        ['static model', 196, [0, 0, 255]], ['CPU skinning', 276, [255, 0, 255]],
        ['GPU skinning', 356, [255, 0, 0]],
        ['instanced A', 436, [0, 255, 0]], ['instanced B', 516, [0, 255, 0]],
    ];
    assert.equal(image.width, 640);
    assert.equal(image.height, 360);
    for (const [label, x, rgb] of probes) {
        const animated = /skinning|instanced/.test(label);
        const offset = ((image.height - 1 - 266) * image.width + x + (motion && animated ? 12 : 0)) * 4;
        assert.deepEqual(Array.from(image.data.subarray(offset, offset + 3)), rgb, label + ' missing or stale');
        if (motion && animated) {
            const previous = ((image.height - 1 - 266) * image.width + x) * 4;
            assert.deepEqual(Array.from(image.data.subarray(previous, previous + 3)), [0, 0, 0], label + ' stayed in its bind pose');
        }
    }
}

async function main() {
    const base = process.argv[2] || 'http://127.0.0.1:8766/';
    const output = process.argv[3];
    if (!output) throw new Error('Usage: node test_component_2d.cjs URL NEW_OUTPUT_DIRECTORY');
    fs.mkdirSync(output, {recursive: false});
    const browser = await chromium.launch({headless: true,
        executablePath: process.env.CHROME_EXECUTABLE || undefined,
        args: ['--enable-unsafe-swiftshader', '--autoplay-policy=no-user-gesture-required']});
    const results = [];
    try {
        // The legacy flag must select the same handoff path; explicit new=0 in
        // direct mode must override the project file's old flag.
        const modes = process.env.POC_TEST_MODES ? process.env.POC_TEST_MODES.split(',') : ['overlap', 'overlap-slow', 'overlap-completion', 'overlap-retry', 'threaded', 'direct', 'inline', 'alias', 'context-loss', 'overlap-context-loss'];
        for (const mode of modes) {
            const overlap = mode.startsWith('overlap');
            const threaded = mode === 'threaded' || mode === 'alias' || overlap;
            const contextLoss = mode.endsWith('context-loss');
            const page = await browser.newPage({viewport: {width: 800, height: 500}});
            await page.addInitScript(slow => {
                window.webGeometryDraws = {maxInstances: 0};
                for (const name of ['drawArraysInstanced', 'drawElementsInstanced']) {
                    const original = WebGL2RenderingContext.prototype[name];
                    WebGL2RenderingContext.prototype[name] = function(...args) {
                        window.webGeometryDraws.maxInstances = Math.max(window.webGeometryDraws.maxInstances, args.at(-1));
                        // A deliberate consumer stall makes the concurrency assertion
                        // deterministic. These are correctness tests, not timings.
                        if (slow) {
                            const until = performance.now() + 2;
                            while (performance.now() < until) {}
                        }
                        return original.apply(this, args);
                    };
                }
            }, mode === 'overlap-slow');
            const log = [], errors = [];
            page.on('console', message => {
                log.push(message.text());
                if (message.text().includes('WEB_2D_ERROR') || /GL_INVALID_|GL_OUT_OF_MEMORY/.test(message.text())) errors.push(message.text());
            });
            page.on('pageerror', error => errors.push(String(error)));
            try {
                await page.goto(`${base}?mode=${contextLoss ? (overlap ? 'overlap' : 'threaded') : mode}&stack_kb=${process.env.STACK_KB||5120}&stack_measure=${process.env.STACK_MEASURE==='1'?1:0}`);
                assert(await page.evaluate(() => crossOriginIsolated));
                if (contextLoss) {
                    await page.waitForFunction(() => (window.lines || []).some(s => s.includes('WEB_POC_ACTIVE')));
                    assert(await page.evaluate(() => {
                        const extension = Module.ctx.getExtension('WEBGL_lose_context');
                        if (!extension) return false;
                        extension.loseContext();
                        return true;
                    }));
                    await page.waitForFunction(() => Module.webPocResult !== undefined);
                    assert.equal(await page.evaluate(() => Module.webPocResult), 1);
                    results.push({mode, stoppedCleanly: true});
                } else {
                    await page.waitForFunction(() => typeof MainLoop !== 'undefined' && MainLoop.func);
                    await page.locator('canvas').focus();
                    await page.keyboard.down('Space');
                    await page.waitForTimeout(120);
                    await page.keyboard.up('Space');
                    if (mode === 'threaded' || overlap) {
                        await page.evaluate(() => Module.ccall('dmEngineSetRenderEnabled', null, ['number'], [0]));
                        await page.waitForTimeout(100);
                        await page.evaluate(() => Module.ccall('dmEngineSetRenderEnabled', null, ['number'], [1]));
                        await page.evaluate(() => Module.ccall('dmEngineSetUpdateEnabled', null, ['number'], [0]));
                        await page.waitForTimeout(100);
                        await page.evaluate(() => Module.ccall('dmEngineSetUpdateEnabled', null, ['number'], [1]));
                    }
                    await page.waitForFunction(() => (window.lines || []).some(s => s.includes('WEB_GEOMETRY_MOTION') || s.includes('WEB_2D_ERROR')));
                    assert.deepEqual(errors, []);
                    await page.evaluate(() => Module.ccall('dmEngineSetUpdateEnabled', null, ['number'], [0]));
                    const motionPng = await page.locator('canvas').screenshot({path: path.join(output, mode + '-motion.png')});
                    checkGeometry(motionPng, true);
                    await page.evaluate(() => Module.ccall('dmEngineSetUpdateEnabled', null, ['number'], [1]));
                    await page.waitForFunction(() => (window.lines || []).some(s => s.includes('WEB_2D_CHECK') || s.includes('WEB_2D_ERROR')));
                    assert.deepEqual(errors, []);
                    const checkpoint = log.find(s => s.includes('WEB_2D_CHECK'));
                    assert(checkpoint.includes(overlap ? 'component-web-snapshots' : threaded ? 'component-web-serialized' : 'mode=existing'));
                    assert(checkpoint.includes('inputs=1 sound_done=1'));
                    assert(log.some(s => s.includes('WEB_GEOMETRY_CHECK animations=4 created=2 deleted=2 buffer_offset=16')));
                    const overlapLine = log.find(s => s.includes('WEB_OVERLAP'));
                    const simulationsDuringRender = Number(/simulations=(\d+)/.exec(overlapLine)[1]);
                    if (mode === 'overlap-slow') assert(simulationsDuringRender > 0, 'Simulation must complete during active consumption');
                    const png = await page.locator('canvas').screenshot({path: path.join(output, mode + '.png')});
                    checkGeometry(png);
                    const geometryDraws = await page.evaluate(() => window.webGeometryDraws);
                    assert(geometryDraws.maxInstances >= 2, 'Animated models must exercise a batched instanced draw');
                    await page.waitForFunction(() => (window.lines || []).some(s => s.includes('WEB_2D_PASS')));
                    if (threaded) {
                        await page.waitForFunction(() => Module.webPocResult !== undefined);
                        assert.equal(await page.evaluate(() => Module.webPocResult), 0);
                    }
                    const stack = threaded ? await page.evaluate(() => Module.webStack) : undefined;
                    if (threaded && process.env.STACK_MEASURE === '1') {
                        assert.equal(stack.reserved, Number(process.env.STACK_KB||5120)*1024);
                        assert(stack.measured && stack.touched > 0 && stack.touched < stack.reserved);
                    }
                    results.push({mode, checkpoint, stack, geometryDraws, simulationsDuringRender, artificialDrawDelayMs: mode === 'overlap-slow' ? 2 : 0, pixelSha256: crypto.createHash('sha256').update(png).digest('hex')});
                }
                assert.deepEqual(errors, []);
            } finally {
                fs.writeFileSync(path.join(output, mode + '.log'), log.concat(errors).join('\n'));
                await page.close();
            }
        }
        assert.equal(new Set(results.filter(r => r.pixelSha256).map(r => r.pixelSha256)).size, 1, 'All modes must render identical frozen checkpoints');
        fs.writeFileSync(path.join(output, 'results.json'), JSON.stringify({browser: browser.version(), correctnessOnly: true, results}, null, 2));
        console.log(JSON.stringify(results, null, 2));
    } finally {
        await browser.close();
    }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
