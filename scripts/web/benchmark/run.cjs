// Visible-browser comparison; each case runs in an isolated Chrome process.
const {chromium}=require('playwright');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const crypto=require('node:crypto'),os=require('node:os');
const {execFileSync}=require('node:child_process');
const {validateReplay}=require('./replay_contract.cjs');
const {validateDiagnostics}=require('./diagnostic_contract.cjs');
const {validateTiming}=require('./timing_contract.cjs');
const android=process.env.ADB_SERIAL?require('./android_device.cjs').createAndroid():null;
const out=process.argv[2];if(!out)throw Error('Usage: node run.cjs NEW_OUTPUT_DIRECTORY [case-name]');
const cases=(process.env.CASES_FILE?JSON.parse(fs.readFileSync(process.env.CASES_FILE,'utf8')):[
 {name:'bunny10k',scene:'bunny',count:10000},
 {name:'bunny20k',scene:'bunny',count:20000},
 {name:'bunny30k',scene:'bunny',count:30000},
 {name:'render50k',count:50000},
 {name:'simulation',count:1000,simulation_iterations:1000000},
 {name:'balanced',count:10000,simulation_iterations:100000,moving_fraction:1},
 {name:'fill',count:10000,sprite_size:64,passes:4},
 {name:'geometry200',scene:'geometry',count:200},
 {name:'geometry1000',scene:'geometry',count:1000}
]).filter(c=>(c.scene!=='geometry'||process.argv[3]||process.env.CASES||process.env.CASES_FILE)&&(!process.argv[3]||c.name===process.argv[3])&&(!process.env.CASES||process.env.CASES.split(',').includes(c.name)));
const policies={threaded:[0,0],scheduled:[1,0],cached_raf:[0,1],cached_completion:[1,1],cached_retry:[2,1]};
const variants=process.env.VARIANTS_FILE?JSON.parse(fs.readFileSync(process.env.VARIANTS_FILE,'utf8')):null;
const modes=variants?Object.keys(variants):process.env.MODES?process.env.MODES.split(','):['direct','inline','threaded','scheduled'];
assert(cases.length>0);assert(cases.every(c=>/^[a-zA-Z0-9_-]+$/.test(c.name)));assert(new Set(cases.map(c=>c.name)).size===cases.length);assert(modes.length>0&&new Set(modes).size===modes.length);
assert(modes.every(m=>['direct','inline','serialized','barrier','overlap','overlap_completion','overlap_paced','overlap_ready','barrier_completion',...Object.keys(policies)].includes(variants?variants[m].mode:m)));
let manifest={started:new Date().toISOString(),cases,modes,repeats:Number(process.env.REPEATS||3),seconds:Number(process.env.SECONDS||15),warmup:Number(process.env.WARMUP||5),metrics:process.env.METRICS!=='0',headless:process.env.HEADLESS==='1',runs:[]};
assert(Number.isInteger(manifest.repeats)&&manifest.repeats>0);
assert(Number.isFinite(manifest.seconds)&&manifest.seconds>0&&Number.isFinite(manifest.warmup)&&manifest.warmup>=0);
if(process.env.MEMORY_PROBE==='1')Object.assign(manifest,{memoryProbe:true,stackKb:Number(process.env.STACK_KB||5120),stackMeasure:process.env.STACK_MEASURE==='1'});
manifest.audioActive=process.env.AUDIO_ACTIVE==='1';
manifest.diagnostics=process.env.DIAGNOSTICS==='1';
manifest.memoryDiagnostic=process.env.MEMORY_DIAGNOSTIC==='1';
manifest.memorySamples=process.env.MEMORY_SAMPLES!=='0';
manifest.statusReports=process.env.STATUS_REPORTS!=='0';
manifest.allowTimingOverflow=process.env.ALLOW_TIMING_OVERFLOW==='1';
assert(!manifest.allowTimingOverflow||cases.every(c=>c.scene==='geometry'),'Timing overflow reporting is restricted to explicit geometry stress runs');
assert(!manifest.memoryDiagnostic||cases.every(c=>c.scene==='geometry'),'GC diagnostic currently requires geometry cases');
if(variants)manifest.variants=variants;
manifest.environment=android?android.environment:{platform:process.platform,arch:process.arch,os:os.release(),device:process.env.DEVICE_ID||os.hostname()};
if(android){assert(!manifest.headless,'Android measurements require visible Chrome');manifest.android={thermalStartMaxStatus:1,cpuIdleMinimum:0.9,thermalProtocol:'sustained-external-power',rssAvailable:false};}
function bundleHashes(){
 const hashDir=dir=>Object.fromEntries(fs.readdirSync(dir).filter(n=>n==='index.html'||n.startsWith('dmengine_release.')).sort().map(n=>[n,crypto.createHash('sha256').update(fs.readFileSync(path.join(dir,n))).digest('hex')]));
 if(variants&&Object.values(variants).some(v=>v.bundleDir)){
  assert(Object.values(variants).every(v=>v.bundleDir),'Every variant needs a frozen bundle directory');
  return Object.fromEntries(Object.entries(variants).map(([name,v])=>[name,hashDir(v.bundleDir)]));
 }
 return process.env.BUNDLE_DIR?hashDir(process.env.BUNDLE_DIR):undefined;
}
manifest.bundle=bundleHashes();
const sourceNames=['run.cjs','replay_contract.cjs','diagnostic_contract.cjs','index.html','timing_contract.cjs',...(android?['android_device.cjs']:[])];
manifest.runnerSources=Object.fromEntries(sourceNames.map(n=>[n,crypto.createHash('sha256').update(fs.readFileSync(path.join(__dirname,n))).digest('hex')]));
if(process.env.GATES_FILE)manifest.gates=JSON.parse(fs.readFileSync(process.env.GATES_FILE,'utf8'));
if(process.env.RESUME==='1'){
 const saved=JSON.parse(fs.readFileSync(path.join(out,'manifest.json'),'utf8'));
 for(const k of ['cases','modes','repeats','seconds','warmup','metrics','headless'])assert.deepEqual(saved[k],manifest[k],`Resume configuration differs: ${k}`);
 for(const k of ['memoryProbe','stackKb','stackMeasure','memoryDiagnostic','memorySamples','diagnostics','audioActive','allowTimingOverflow','statusReports'])assert.equal(saved[k],manifest[k],`Resume configuration differs: ${k}`);
 assert.deepEqual(saved.variants,manifest.variants,'Resume variants differ');
 assert.deepEqual(saved.bundle,manifest.bundle,'Resume bundle differs');
 assert.deepEqual(saved.runnerSources,manifest.runnerSources,'Resume runner sources differ');
 assert.deepEqual(saved.gates,manifest.gates,'Resume gates differ');
 assert.deepEqual(saved.environment,manifest.environment,'Resume device differs');
 manifest=saved;
}else fs.mkdirSync(out,{recursive:false});
// Preserve the manifest even if the very first run is rejected, so it can be
// resumed without dropping the failed attempt or changing its configuration.
fs.writeFileSync(path.join(out,'manifest.json'),JSON.stringify(manifest,null,2));
fs.mkdirSync(path.join(out,'sources'),{recursive:true});
for(const n of sourceNames)fs.copyFileSync(path.join(__dirname,n),path.join(out,'sources',n));
function power(){if(android)return android.power();const p=execFileSync('pmset',['-g','batt'],{encoding:'utf8'});assert(p.includes('AC Power'));return p;}
async function mem(cdp,page){
 const processes=android?[]:(await cdp.send('SystemInfo.getProcessInfo')).processInfo;
 const rss=android?[]:execFileSync('/bin/ps',['-o','pid=,rss=','-p',processes.map(p=>p.id).join(',')],{encoding:'utf8'}).trim().split('\n').map(l=>l.trim().split(/\s+/).map(Number));
 const state=await page.evaluate(probe=>{if(probe)Module._dmEngineSampleWebMemory();return {now:performance.timeOrigin+performance.now(),hidden:document.hidden,focused:document.hasFocus(),canvas:[Module.canvas.width,Module.canvas.height],dpr:devicePixelRatio,audioState:globalThis._dmJSDeviceShared?.audioCtx?.state,heap:Module.HEAPU8.buffer.byteLength,allocator:Module.webAllocator};},manifest.memoryProbe);
 assert(!state.hidden,JSON.stringify(state));assert(state.focused,JSON.stringify(state));return {...state,processes,rss:android?null:rss.reduce((n,p)=>n+p[1],0)*1024};
}
(async()=>{
for(let repeat=0;repeat<manifest.repeats;repeat++)for(const c of cases){
 const order=modes.slice(repeat%modes.length).concat(modes.slice(0,repeat%modes.length));
 for(const mode of order){
 const variant=variants?variants[mode]:{mode},engineMode=variant.mode,stackKb=variant.stackKb??manifest.stackKb;
 const key=`${c.name}-${mode}-${repeat+1}`, result={key,case:c,mode,repeat:repeat+1,power:power(),memory:[],log:[],errors:[]};
 if(manifest.runs.includes(key))continue;
 const savedFile=path.join(out,key+'.json');
 if(fs.existsSync(savedFile)){
  const previous=JSON.parse(fs.readFileSync(savedFile,'utf8'));
  assert(!previous.valid,'Valid unregistered run exists; reconcile the manifest before resuming');
  const failed=path.join(out,'failed');fs.mkdirSync(failed,{recursive:true});
  let attempt=1;while(fs.existsSync(path.join(failed,`${key}-attempt-${attempt}.json`)))attempt++;
  fs.renameSync(savedFile,path.join(failed,`${key}-attempt-${attempt}.json`));
  if(fs.existsSync(path.join(out,key+'.csv')))fs.renameSync(path.join(out,key+'.csv'),path.join(failed,`${key}-attempt-${attempt}.csv`));
 }
 console.log(`Starting ${key}`);
 const mobile=android?await android.launch(chromium):null;
 const browser=mobile?mobile.browser:await chromium.launch({headless:manifest.headless,executablePath:process.env.CHROME_EXECUTABLE||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',args:[...(manifest.headless?['--enable-gpu','--use-angle=metal']:['--window-size=1400,1000']),...(manifest.audioActive?['--autoplay-policy=no-user-gesture-required','--mute-audio']:[])]});
 try{
 const cdp=await browser.newBrowserCDPSession();result.browser=browser.version();result.gpu=(await cdp.send('SystemInfo.getInfo')).gpu.devices;
 if(process.env.GPU_MATCH)assert(result.gpu.some(g=>g.deviceString.includes(process.env.GPU_MATCH)));assert(result.gpu.length>0);assert(!result.gpu.some(g=>/SwiftShader|llvmpipe|Software Rasterizer/i.test(g.deviceString)));
 const page=mobile?mobile.page:await browser.newPage({viewport:manifest.headless?{width:1400,height:900}:null});
 if(android){result.thermalBefore=mobile.thermalBefore;result.thermal=[];}
 const pageCdp=await page.context().newCDPSession(page);
 let measured=false,ended=false,phase="startup";
 page.on('pageerror',e=>result.errors.push(String(e)));
 page.on('console',m=>{const s=m.text();result.log.push(s);if(s.startsWith('WEB_PHASE '))phase=s.slice(10);if(s.startsWith('WEB_PHASE measure'))measured=true;if(s.startsWith('WEB_RESULT ')){result.result=JSON.parse(s.slice(11));ended=true;}if(/WEB_ERROR|FATAL:|ERROR:|GL_INVALID_|GL_OUT_OF_MEMORY/.test(s))result.errors.push(s);});
 const q=new URLSearchParams({...c,config:JSON.stringify(c.config||{}),mode:engineMode,seconds:manifest.seconds,warmup:manifest.warmup,metrics:manifest.metrics?1:0});
 if(manifest.memoryProbe){q.set('stack_kb',stackKb);q.set('stack_measure',manifest.stackMeasure?1:0);}
 q.set('diagnostics',manifest.diagnostics?1:0);
 q.set('owned_models',variant.ownedModels?1:0);
 q.set('deferred_sprites',variant.deferredSprites?1:0);
 q.set('memory_diagnostic',manifest.memoryDiagnostic?1:0);
 q.set('memory_samples',manifest.memorySamples?1:0);
 q.set('status_reports',manifest.statusReports?1:0);
 q.set('ready_budget_ms',variant.readyBudgetMs??0);
 await page.goto((variant.url||c.url||'http://127.0.0.1:8766/')+'?'+q);
 await pageCdp.send('Emulation.setFocusEmulationEnabled',{enabled:false});
 result.focusEmulation=false;
 if(!manifest.headless&&!android){
  const processes=(await cdp.send('SystemInfo.getProcessInfo')).processInfo;
  const pid=processes.find(p=>p.type==='browser').id;
  assert(Number.isSafeInteger(pid));
  // Target this isolated process, not another user Chrome profile with the same
  // bundle ID. CDP bringToFront alone does not activate its macOS application.
  execFileSync('/usr/bin/osascript',['-e','tell application "Google Chrome" to activate'],{timeout:10000});
  result.activation=execFileSync('/usr/bin/osascript',['-l','JavaScript','-e',`ObjC.import('AppKit'); const app=$.NSRunningApplication.runningApplicationWithProcessIdentifier(${pid}); app.activateWithOptions(2); JSON.stringify({pid:${pid},active:Boolean(app.active),frontmost:$.NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier});`],{timeout:10000,encoding:'utf8'}).trim();
 }
 await page.bringToFront();
 await page.waitForFunction(()=>document.hasFocus(),null,{timeout:3000});
 const deadline=Date.now()+(c.timeout_seconds||manifest.warmup+manifest.seconds+120)*1000;
 while(!measured&&!ended){assert(!await page.evaluate(()=>!!Module.webCpuTrace),'Engine exited before measurement');assert(Date.now()<deadline,'No measurement start');assert.equal(result.errors.length,0,result.errors.join('\n'));await page.waitForTimeout(250);}
 while(!ended){if(android&&result.memory.length%5===0)result.thermal.push({time:Date.now(),...android.thermal()});result.memory.push({...await mem(cdp,page),phase});if(result.memory.length%15===0)console.log(`${key}: ${result.memory.length*2}s sampled`);await page.waitForTimeout(2000);assert(Date.now()<deadline,'No measurement end');assert.equal(result.errors.length,0,result.errors.join('\n'));}
 await page.waitForFunction(()=>Module.webPocResult!==undefined||typeof EXITSTATUS==='number'||window.exitCode!==undefined,null,{timeout:20000});
 result.exit=await page.evaluate(()=>Module.webPocResult??window.exitCode??EXITSTATUS);assert.equal(result.exit,0);
 const evidence=await page.evaluate(()=>({trace:Module.webCpuTrace,schedule:Module.webSchedule,stack:Module.webStack,diagnostics:Module.webUpdateDiagnostics,renderAdmission:Module.webRenderAdmission}));
 if(manifest.memoryProbe){
  result.stack=evidence.stack;
  result.snapshots=c.scene==='replay'?result.result.memory.samples.map(s=>s.snapshot).filter(Boolean):result.log.filter(s=>s.startsWith('WEB_MEMORY ')).map(s=>JSON.parse(s.slice(11)));
  const finalSnapshot=result.log.find(s=>s.startsWith('WEB_MEMORY_FINAL '));
  if(finalSnapshot)result.finalSnapshot=JSON.parse(finalSnapshot.slice(17));
  result.timing=c.scene==='replay'?result.result.frame_interval:JSON.parse(result.log.find(s=>s.startsWith('WEB_TIMING ')).slice(11));
  assert(!manifest.memorySamples||result.snapshots.length>0);assert(result.memory.every(m=>m.allocator&&m.allocator.allocated>0));
  result.timingOverflowAllowed=manifest.allowTimingOverflow;
  validateTiming(result.timing,{allowOverflow:manifest.allowTimingOverflow});
  if(policies[engineMode]){assert.equal(result.stack.reserved,stackKb*1024);assert.equal(result.stack.measured,manifest.stackMeasure);if(manifest.stackMeasure)assert(result.stack.touched>0&&result.stack.touched<result.stack.reserved);}
 }
 if(manifest.metrics){assert(evidence.trace,'Missing CPU trace');assert(evidence.trace.startsWith('# dropped=0;'));fs.writeFileSync(path.join(out,key+'.csv'),evidence.trace);}
 result.memoryDiagnostic=manifest.memoryDiagnostic;
 result.gc=result.log.filter(s=>s.startsWith('WEB_MEMORY_GC ')).map(s=>JSON.parse(s.slice(14)));
 if(manifest.memoryDiagnostic){
  assert.deepEqual(result.gc.map(s=>s.phase),['start','end']);
  assert(result.gc.every(s=>[s.allocated_before,s.allocated_after,s.lua_before,s.lua_after].every(n=>Number.isFinite(n)&&n>0)));
 }else assert.equal(result.gc.length,0);
 result.diagnostics=evidence.diagnostics;
 if(manifest.diagnostics&&engineMode!=='direct'&&engineMode!=='inline')validateDiagnostics(result.diagnostics);
 else assert(!result.diagnostics,'Unexpected diagnostic instrumentation');
 result.renderAdmission=evidence.renderAdmission;
 if(engineMode==='overlap_ready'){assert(result.renderAdmission);assert(result.renderAdmission.retired<=result.renderAdmission.browserTicks+1);}
 // Config GetFloat rounds fractional budgets to float32; formatted banners are
 // not an exact numeric contract (printf and JS can round ties differently).
 if(variant.readyBudgetMs){assert.equal(result.renderAdmission.budgetMs,Math.fround(variant.readyBudgetMs));assert(result.log.some(s=>s.includes('WEB_POC_READY_BUDGET milliseconds=')));}
 if(!manifest.statusReports&&c.scene==='bunny')assert(!result.log.some(s=>s.startsWith('BUNNYMARK count=')),'Status reporting was not disabled by the content bundle');
 result.schedule=evidence.schedule;if(result.schedule)assert.equal(result.schedule.dropped,0);
 if(android&&manifest.audioActive&&c.scene==='replay')assert(result.memory.filter(m=>m.phase==='measure').every(m=>m.audioState==='running'),'Android replay audio was not running');
 assert(result.result.valid);assert.equal(result.errors.length,0);assert(result.memory.length>0);assert.equal(new Set(result.memory.map(m=>JSON.stringify(m.canvas))).size,1);
 if(policies[engineMode]){const [schedule,cache]=policies[engineMode];assert(result.log.some(s=>s.includes('component-web-threaded')));assert(result.log.some(s=>s.includes(`WEB_POC_OPTIONS schedule=${schedule} cache_window=${cache}`)));assert(result.log.some(s=>s.includes('WEB_POC_SCHEDULE completion_dispatch='+(schedule===1?1:0)+' metrics='+(manifest.metrics?1:0))));}
 if(['serialized','barrier','overlap','overlap_completion','overlap_paced','overlap_ready','barrier_completion'].includes(engineMode)){
  assert(result.log.some(s=>s.includes(engineMode==='serialized'?'WEB_POC_ACTIVE component-web-serialized':'WEB_POC_ACTIVE component-web-snapshots')));
  assert(result.log.some(s=>s.includes('WEB_COMPONENT_SPRITES deferred_geometry='+(variant.deferredSprites?1:0)))||engineMode==='serialized');
  assert(result.log.some(s=>s.includes('WEB_COMPONENT_BUFFERS owned_models='+(variant.ownedModels?1:0)))||engineMode==='serialized');
  assert(result.log.some(s=>s.includes(`WEB_POC_OPTIONS schedule=${engineMode==='overlap_ready'?4:engineMode==='overlap_paced'?3:engineMode.endsWith('_completion')?1:0} cache_window=1`)));
  if(engineMode!=='serialized')assert(result.log.some(s=>s.includes('WEB_COMPONENT_PREPARATION overlap='+(engineMode.startsWith('overlap')?1:0))));
  if(manifest.memoryProbe){
   assert.equal(result.stack.reserved,stackKb*1024);
   assert.equal(result.stack.measured,manifest.stackMeasure);
   if(manifest.stackMeasure)assert(result.stack.touched>0&&result.stack.touched<result.stack.reserved);
   if(engineMode.startsWith('barrier'))assert(result.snapshots.every(s=>s.thread_preparations_during_render===0));
  }
 }
 if(c.scene==='replay'){
  result.replaySignature=validateReplay(result.result,c.expected,manifest.diagnostics);
  manifest.replaySignatures ||= {};
  if(manifest.replaySignatures[c.name])assert.equal(result.replaySignature,manifest.replaySignatures[c.name],'Gameplay diverged from the reference replay');
  else manifest.replaySignatures[c.name]=result.replaySignature;
 }
 assert.deepEqual(bundleHashes(),manifest.bundle,'Input bundle changed during measurement');
 result.powerAfter=power();if(android){result.thermalAfter=android.thermal();result.cpuAfter=android.idleCpu();}result.valid=true;
 }catch(e){result.failure=String(e);throw e}
 finally{fs.writeFileSync(path.join(out,key+'.json'),JSON.stringify(result,null,2));if(android)await android.close(browser,mobile.page);else await browser.close();}
 manifest.runs.push(key);fs.writeFileSync(path.join(out,'manifest.json'),JSON.stringify(manifest,null,2));
 console.log(`${manifest.runs.length}/${cases.length*modes.length*manifest.repeats} ${key}: ${result.result.update_intervals_per_second.toFixed(2)} updates/s`);
 }
}
})().catch(e=>{console.error(e);process.exitCode=1});
