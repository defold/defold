// Short correctness checks only. Install playwright or provide it via NODE_PATH.
const { chromium } = require('playwright');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

async function main() {
    const base = process.argv[2] || 'http://127.0.0.1:8765/';
    const output = process.argv[3];
    if (!output) throw new Error('Usage: node test_component_poc.cjs URL NEW_OUTPUT_DIRECTORY');
    fs.mkdirSync(output, {recursive: false});
    const browser = await chromium.launch({
        headless: true,
        executablePath: process.env.CHROME_EXECUTABLE || undefined,
        args: ['--enable-unsafe-swiftshader'],
    });
    const results = [];
    try {
        // Verify window caching and both schedulers preserve pixels, input, pause and context-loss shutdown.
        const testModes = process.env.POC_TEST_MODES ? process.env.POC_TEST_MODES.split(',') : ['direct', 'inline', 'threaded', 'scheduled', 'cached-raf', 'cached-completion', 'cached-retry', 'context-loss', 'scheduled-context-loss', 'cached-raf-context-loss', 'cached-completion-context-loss', 'cached-retry-context-loss'];
        for (const mode of testModes) {
            const context = await browser.newContext({viewport: {width: 800, height: 500}});
            const page = await context.newPage();
            const log = [], errors = [];
            page.on('console', message => log.push(message.text()));
            page.on('pageerror', error => errors.push(error.stack));
            try {
                const threaded = !['direct', 'inline'].includes(mode);
                const schedule = mode.includes('retry') ? 2 : mode.startsWith('scheduled') || mode.includes('completion') ? 1 : 0;
                const cache = mode.startsWith('cached') ? 1 : 0;
                await page.goto(`${base}?mode=${threaded ? 'threaded' : mode}&schedule=${schedule}&cache_window=${cache}&stack_kb=${process.env.STACK_KB||5120}&stack_measure=${process.env.STACK_MEASURE==='1'?1:0}`);
                assert(await page.evaluate(() => crossOriginIsolated));
                if (threaded) {
                    await page.waitForFunction(() => (window.lines || []).some(s => s.includes('WEB_POC_ACTIVE')));
                    assert(log.some(s => s.includes(`WEB_POC_OPTIONS schedule=${schedule} cache_window=${cache}`)));
                }
                // Inject hidden state to exercise admission/hidden-service behavior;
                // this is a control-flow regression, not a browser visibility test.
                if (threaded && process.env.HIDDEN_CHECKS === '1') {
                    await page.evaluate(() => Object.defineProperty(document, 'hidden', {configurable: true, get: () => true}));
                    await page.waitForTimeout(2500);
                    assert(!log.some(s => s.includes('WEB_POC_PASS')), 'Hidden tab continued simulation');
                    if (!mode.includes('context-loss')) {
                        await page.evaluate(() => delete document.hidden);
                    } else {
                        // With rAF explicitly paused, only HiddenService can retire
                        // the accepted frame and complete context-loss shutdown.
                        await page.evaluate(() => MainLoop.pause());
                    }
                }
                if (mode.includes('context-loss')) {
                    await page.waitForFunction(() => (window.lines || []).some(s => s.includes('WEB_POC_ACTIVE')));
                    const lost = await page.evaluate(() => {
                        const gl = document.querySelector('canvas').getContext('webgl2');
                        const extension = gl.getExtension('WEBGL_lose_context');
                        if (!extension) return false;
                        extension.loseContext();
                        return true;
                    });
                    assert(lost, 'Context-loss extension is required for this check');
                    await page.waitForFunction(() => Module.webPocResult !== undefined, null, {timeout: 15000});
                    assert.equal(await page.evaluate(() => Module.webPocResult), 1);
                    assert.deepEqual(errors, []);
                    results.push({mode, stoppedCleanly: true});
                } else {
                    await page.waitForFunction(() => typeof MainLoop !== 'undefined' && MainLoop.func);
                    await page.locator('canvas').focus();
                    await page.keyboard.down('Space');
                    await page.waitForTimeout(100);
                    await page.keyboard.up('Space');
                    if (threaded) {
                        await page.evaluate(() => Module.ccall('dmEngineSetRenderEnabled', null, ['number'], [0]));
                        await page.waitForTimeout(100);
                        await page.evaluate(() => Module.ccall('dmEngineSetRenderEnabled', null, ['number'], [1]));
                        await page.evaluate(() => Module.ccall('dmEngineSetUpdateEnabled', null, ['number'], [0]));
                        await page.waitForTimeout(100);
                        await page.evaluate(() => Module.ccall('dmEngineSetUpdateEnabled', null, ['number'], [1]));
                    }
                    await page.waitForFunction(() => (window.lines || []).some(s => s.includes('WEB_POC_CHECK')), null, {timeout: 15000});
                    // The fixture freezes its visual positions at tick 120.
                    await page.waitForTimeout(150);
                    const pixels = await page.locator('canvas').screenshot();
                    fs.writeFileSync(path.join(output, `${mode}.png`), pixels);
                    await page.waitForFunction(() => (window.lines || []).some(s => s.includes('WEB_POC_PASS')), null, {timeout: 15000});
                    await page.waitForFunction(() => Module.webPocResult !== undefined || typeof EXITSTATUS === 'number', null, {timeout: 15000});
                    const code = await page.evaluate(() => Module.webPocResult ?? EXITSTATUS);
                    assert.equal(code, 0);
                    assert.deepEqual(errors, []);
                    const checkpoint = log.find(s => s.includes('WEB_POC_CHECK'));
                    const pass = log.find(s => s.includes('WEB_POC_PASS'));
                    assert.match(pass, /inputs=[1-9][0-9]* ticks=200/);
                    const stack = threaded ? await page.evaluate(() => Module.webStack) : undefined;
                    if (threaded && process.env.STACK_MEASURE === '1') {
                        assert.equal(stack.reserved, Number(process.env.STACK_KB||5120)*1024);
                        assert(stack.measured && stack.touched > 0 && stack.touched < stack.reserved);
                    }
                    results.push({mode, code, checkpoint, pass, stack, pixelSha256: crypto.createHash('sha256').update(pixels).digest('hex')});
                }
            } finally {
                fs.writeFileSync(path.join(output, `${mode}.log`), log.concat(errors).join('\n') + '\n');
                await context.close();
            }
        }
        const modes = results.filter(r => r.pixelSha256);
        assert.equal(new Set(modes.map(r => r.pixelSha256)).size, 1, 'Direct, inline and threaded pixels differ');
        assert.equal(new Set(modes.map(r => /checksum=(\d+)/.exec(r.checkpoint)[1])).size, 1, 'Simulation checkpoints differ');
        fs.writeFileSync(path.join(output, 'results.json'), JSON.stringify({browser: browser.version(), correctnessOnly: true, results}, null, 2) + '\n');
        console.log(JSON.stringify(results, null, 2));
    } finally {
        await browser.close();
    }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
