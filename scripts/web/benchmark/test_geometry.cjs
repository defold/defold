// Verify the timed geometry scene renders every intended material path at scale.
const {chromium} = require('playwright');
const {PNG} = require('pngjs');
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path');
const out = process.argv[2];
assert(out, 'Usage: node test_geometry.cjs NEW_OUTPUT_DIRECTORY');
fs.mkdirSync(out, {recursive:false});
(async () => {
    const browser = await chromium.launch({headless:true, executablePath:process.env.CHROME_EXECUTABLE,
        args:['--enable-unsafe-swiftshader']});
    const results = [];
    try {
        for (const count of [200, 1000]) for (const mode of ['direct', 'overlap', 'overlap-owned']) {
            const page = await browser.newPage({viewport:{width:1400,height:900}});
            const errors = [];
            page.on('pageerror', e => errors.push(String(e)));
            page.on('console', m => {if (/WEB_ERROR|FATAL:|ERROR:|GL_INVALID_|GL_OUT_OF_MEMORY/.test(m.text())) errors.push(m.text());});
            await page.goto(`http://127.0.0.1:8766/?scene=geometry&count=${count}&mode=${mode==='overlap-owned'?'overlap':mode}&owned_models=${mode==='overlap-owned'?1:0}&warmup=3&seconds=60&metrics=0`);
            await page.waitForFunction(() => (window.lines||[]).some(s => s.startsWith('WEB_PHASE measure')), null, {timeout:60000});
            const png = PNG.sync.read(await page.locator('canvas').screenshot({path:path.join(out, `${count}-${mode}.png`)}));
            assert.equal(png.width, 1280); assert.equal(png.height, 720);
            const colors = {'world mesh':[255,255,0], 'local mesh':[0,255,255], static:[0,0,255], cpu:[255,0,255], gpu:[255,0,0], instanced:[0,255,0]};
            const pixels = {};
            for (const [name, rgb] of Object.entries(colors)) {
                let matches = 0;
                for (let i=0; i<png.data.length; i+=4) if (rgb.every((v,j) => png.data[i+j]===v)) matches++;
                assert(matches >= count/2, `${name} missing: ${matches} pixels for ${count} groups`);
                pixels[name] = matches;
            }
            assert.deepEqual(errors, []);
            results.push({count, mode, pixels});
            await page.close();
        }
        fs.writeFileSync(path.join(out,'results.json'), JSON.stringify(results,null,2));
        console.log(JSON.stringify(results,null,2));
    } finally {await browser.close();}
})().catch(e => {console.error(e);process.exitCode=1;});
