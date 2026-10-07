const {test}=require('node:test');
const assert=require('node:assert/strict');
const {parsePower,parseThermals,parseIdleCpu}=require('./android_device.cjs');
// External power must be detected without assuming a USB cable reports USB charging.
test('Android power accepts AC, USB or wireless and converts battery temperature',()=>{
    for(const kind of ['AC','USB','Wireless'])assert.deepEqual(parsePower(`${kind} powered: true\nlevel: 62\ntemperature: 277`),{powered:true,level:62,temperatureC:27.7});
    assert.equal(parsePower('AC powered: false\nlevel: 62\ntemperature: 277').powered,false);
    assert.throws(()=>parsePower(''),/Missing battery/);
});
// The thermal service caches stale temperatures; cooldown must use current HAL values.
test('Android thermal parsing uses current HAL temperatures and fails closed on missing data',()=>{
    const t=parseThermals('Thermal Status: 0\nCached temperatures:\nTemperature{mValue=49, mType=3, mName=skin, mStatus=0}\nCurrent temperatures from HAL:\nTemperature{mValue=31.2, mType=3, mName=skin, mStatus=0}\nCurrent cooling devices from HAL:\n');
    assert.equal(t.status,0);assert.equal(t.temperatures.length,1);assert.equal(t.temperatures[0].celsius,31.2);
    assert.throws(()=>parseThermals('Thermal Status: 0'),/Missing thermal/);
});

// A cool device may still be busy with background jobs; use the latest top sample.
test('Android idle CPU parsing rejects missing data and uses the latest interval',()=>{
    assert.deepEqual(parseIdleCpu('800%cpu 100%user 700%idle\n800%cpu 10%user 790%idle'),{totalPercent:800,idlePercent:790,idleFraction:0.9875});
    assert.equal(parseIdleCpu('800%cpu 40%user 93%nice 30%sys 630%idle').idleFraction,0.7875);
    assert.throws(()=>parseIdleCpu(''),/Missing Android CPU/);
});

// Dimming must be undone before Chrome starts and when readiness checks fail.
test('Android cooldown restores display settings on success and failure',async()=>{
    const fs=require('node:fs'),os=require('node:os'),path=require('node:path');
    const {createAndroid}=require('./android_device.cjs');
    const root=fs.mkdtempSync(path.join(os.tmpdir(),'android-benchmark-test-'));
    const statePath=path.join(root,'state.json'),fakeAdb=path.join(root,'adb.cjs');
    fs.writeFileSync(fakeAdb,`#!/usr/bin/env node
const fs=require('node:fs');const file=${JSON.stringify(statePath)};
const state=JSON.parse(fs.readFileSync(file));const a=process.argv.slice(4);let out='';
if(a.shift()==='shell'){
 if(a[0]==='getprop')out='test-device';
 if(a[0]==='settings'&&a[1]==='get')out=state[a[3]]??'0';
 if(a[0]==='settings'&&a[1]==='put')state[a[3]]=a[4];
 if(a[0]==='dumpsys'&&a[1]==='battery')out='AC powered: true\\nlevel: 62\\ntemperature: 300';
 if(a[0]==='dumpsys'&&a[1]==='thermalservice')out=state.failThermal?'unavailable':'Thermal Status: 0\\nCurrent temperatures from HAL:\\nTemperature{mValue=31, mType=3, mName=skin, mStatus=0}\\nCurrent cooling devices from HAL:';
 if(a[0]==='top')out='800%cpu 10%user 790%idle';
 if(a[0]==='am'&&a[1]==='start'){
   if(state.screen_brightness!=='66'||state.screen_brightness_mode!=='1')throw Error('Chrome started with dimmed display');
 }
}
fs.writeFileSync(file,JSON.stringify(state));process.stdout.write(String(out));
`,{mode:0o755});
    const saved={ADB:process.env.ADB,ADB_SERIAL:process.env.ADB_SERIAL};
    const listeners=process.listenerCount('SIGINT');
    try{
        process.env.ADB=fakeAdb;process.env.ADB_SERIAL='test-device';
        const browser={contexts:()=>[{newPage:async()=>({})}]};
        const chromium={connectOverCDP:async()=>browser};
        for(const failThermal of [false,true]){
            fs.writeFileSync(statePath,JSON.stringify({screen_brightness:'66',screen_brightness_mode:'1',failThermal}));
            const device=createAndroid();
            if(failThermal)await assert.rejects(device.launch(chromium),/Missing thermal telemetry/);
            else assert.equal((await device.launch(chromium)).browser,browser);
            const state=JSON.parse(fs.readFileSync(statePath));
            assert.equal(state.screen_brightness,'66');assert.equal(state.screen_brightness_mode,'1');
            assert.equal(process.listenerCount('SIGINT'),listeners);
        }
    }finally{
        for(const [key,value] of Object.entries(saved))if(value===undefined)delete process.env[key];else process.env[key]=value;
        fs.rmSync(root,{recursive:true,force:true});
    }
});
