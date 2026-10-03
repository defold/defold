// Visible-browser comparison; each case runs in an isolated Chrome process.
const {chromium}=require('playwright');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {execFileSync}=require('node:child_process');
const out=process.argv[2];if(!out)throw Error('Usage: node run.cjs NEW_OUTPUT_DIRECTORY [case-name]');
const cases=[
 {name:'bunny10k',scene:'bunny',count:10000},
 {name:'bunny20k',scene:'bunny',count:20000},
 {name:'bunny30k',scene:'bunny',count:30000},
 {name:'render50k',count:50000},
 {name:'simulation',count:1000,simulation_iterations:1000000},
 {name:'balanced',count:10000,simulation_iterations:100000,moving_fraction:1},
 {name:'fill',count:10000,sprite_size:64,passes:4}
].filter(c=>(!process.argv[3]||c.name===process.argv[3])&&(!process.env.CASES||process.env.CASES.split(',').includes(c.name)));
const policies={threaded:[0,0],scheduled:[1,0],cached_raf:[0,1],cached_completion:[1,1],cached_retry:[2,1]};
const variants=process.env.VARIANTS_FILE?JSON.parse(fs.readFileSync(process.env.VARIANTS_FILE,'utf8')):null;
const modes=variants?Object.keys(variants):process.env.MODES?process.env.MODES.split(','):['direct','inline','threaded','scheduled'];
assert(cases.length>0);assert(modes.length>0&&new Set(modes).size===modes.length);
assert(modes.every(m=>['direct','inline',...Object.keys(policies)].includes(variants?variants[m].mode:m)));
let manifest={started:new Date().toISOString(),cases,modes,repeats:Number(process.env.REPEATS||3),seconds:Number(process.env.SECONDS||15),warmup:Number(process.env.WARMUP||5),metrics:process.env.METRICS!=='0',headless:process.env.HEADLESS==='1',runs:[]};
if(process.env.MEMORY_PROBE==='1')Object.assign(manifest,{memoryProbe:true,stackKb:Number(process.env.STACK_KB||5120),stackMeasure:process.env.STACK_MEASURE==='1'});
if(variants)manifest.variants=variants;
if(process.env.RESUME==='1'){
 const saved=JSON.parse(fs.readFileSync(path.join(out,'manifest.json'),'utf8'));
 for(const k of ['cases','modes','repeats','seconds','warmup','metrics','headless'])assert.deepEqual(saved[k],manifest[k],`Resume configuration differs: ${k}`);
 for(const k of ['memoryProbe','stackKb','stackMeasure'])assert.equal(saved[k],manifest[k],`Resume configuration differs: ${k}`);
 assert.deepEqual(saved.variants,manifest.variants,'Resume variants differ');
 manifest=saved;
}else fs.mkdirSync(out,{recursive:false});
function power(){const p=execFileSync('pmset',['-g','batt'],{encoding:'utf8'});assert(p.includes('AC Power'));return p;}
async function mem(cdp,page){
 const processes=(await cdp.send('SystemInfo.getProcessInfo')).processInfo;
 const rss=execFileSync('/bin/ps',['-o','pid=,rss=','-p',processes.map(p=>p.id).join(',')],{encoding:'utf8'}).trim().split('\n').map(l=>l.trim().split(/\s+/).map(Number));
 const state=await page.evaluate(probe=>{if(probe)Module._dmEngineSampleWebMemory();return {now:performance.timeOrigin+performance.now(),hidden:document.hidden,focused:document.hasFocus(),canvas:[Module.canvas.width,Module.canvas.height],dpr:devicePixelRatio,heap:Module.HEAPU8.buffer.byteLength,allocator:Module.webAllocator};},manifest.memoryProbe);
 assert(!state.hidden,JSON.stringify(state));assert(state.focused,JSON.stringify(state));return {...state,processes,rss:rss.reduce((n,p)=>n+p[1],0)*1024};
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
 const browser=await chromium.launch({headless:manifest.headless,executablePath:process.env.CHROME_EXECUTABLE||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',args:manifest.headless?['--enable-gpu','--use-angle=metal']:['--window-size=1400,1000']});
 try{
 const cdp=await browser.newBrowserCDPSession();result.browser=browser.version();result.gpu=(await cdp.send('SystemInfo.getInfo')).gpu.devices;
 assert(result.gpu.some(g=>g.deviceString.includes('Apple M1 Pro')));assert(!result.gpu.some(g=>/SwiftShader/.test(g.deviceString)));
 const page=await browser.newPage({viewport:manifest.headless?{width:1400,height:900}:null});
 const pageCdp=await page.context().newCDPSession(page);
 let measured=false,ended=false;
 page.on('pageerror',e=>result.errors.push(String(e)));
 page.on('console',m=>{const s=m.text();result.log.push(s);if(s.startsWith('WEB_PHASE measure'))measured=true;if(s.startsWith('WEB_RESULT ')){result.result=JSON.parse(s.slice(11));ended=true;}if(/WEB_ERROR|FATAL:/.test(s))result.errors.push(s);});
 const q=new URLSearchParams({...c,mode:engineMode,seconds:manifest.seconds,warmup:manifest.warmup,metrics:manifest.metrics?1:0});
 if(manifest.memoryProbe){q.set('stack_kb',stackKb);q.set('stack_measure',manifest.stackMeasure?1:0);}
 await page.goto((variant.url||'http://127.0.0.1:8766/')+'?'+q);
 await pageCdp.send('Emulation.setFocusEmulationEnabled',{enabled:false});
 result.focusEmulation=false;
 await page.bringToFront();
 const deadline=Date.now()+120000;
 while(!measured&&!ended){assert(!await page.evaluate(()=>!!Module.webCpuTrace),'Engine exited before measurement');assert(Date.now()<deadline,'No measurement start');assert.equal(result.errors.length,0,result.errors.join('\n'));await page.waitForTimeout(250);}
 while(!ended){result.memory.push(await mem(cdp,page));await page.waitForTimeout(2000);assert(Date.now()<deadline,'No measurement end');assert.equal(result.errors.length,0,result.errors.join('\n'));}
 await page.waitForFunction(()=>Module.webPocResult!==undefined||typeof EXITSTATUS==='number'||window.exitCode!==undefined,null,{timeout:20000});
 result.exit=await page.evaluate(()=>Module.webPocResult??window.exitCode??EXITSTATUS);assert.equal(result.exit,0);
 const evidence=await page.evaluate(()=>({trace:Module.webCpuTrace,schedule:Module.webSchedule,stack:Module.webStack}));
 if(manifest.memoryProbe){
  result.stack=evidence.stack;
  result.snapshots=result.log.filter(s=>s.startsWith('WEB_MEMORY ')).map(s=>JSON.parse(s.slice(11)));
  result.timing=JSON.parse(result.log.find(s=>s.startsWith('WEB_TIMING ')).slice(11));
  assert(result.snapshots.length>0);assert(result.memory.every(m=>m.allocator&&m.allocator.allocated>0));
  assert.equal(result.timing.overflow_samples,0);
  if(policies[engineMode]){assert.equal(result.stack.reserved,stackKb*1024);assert.equal(result.stack.measured,manifest.stackMeasure);if(manifest.stackMeasure)assert(result.stack.touched>0&&result.stack.touched<result.stack.reserved);}
 }
 if(manifest.metrics){assert(evidence.trace,'Missing CPU trace');assert(evidence.trace.startsWith('# dropped=0;'));fs.writeFileSync(path.join(out,key+'.csv'),evidence.trace);}
 result.schedule=evidence.schedule;if(result.schedule)assert.equal(result.schedule.dropped,0);
 assert(result.result.valid);assert.equal(result.errors.length,0);assert(result.memory.length>0);assert.equal(new Set(result.memory.map(m=>JSON.stringify(m.canvas))).size,1);
 if(policies[engineMode]){const [schedule,cache]=policies[engineMode];assert(result.log.some(s=>s.includes('component-web-threaded')));assert(result.log.some(s=>s.includes(`WEB_POC_OPTIONS schedule=${schedule} cache_window=${cache}`)));assert(result.log.some(s=>s.includes('WEB_POC_SCHEDULE completion_dispatch='+(schedule===1?1:0)+' metrics='+(manifest.metrics?1:0))));}
 result.powerAfter=power();result.valid=true;
 }catch(e){result.failure=String(e);throw e}
 finally{fs.writeFileSync(path.join(out,key+'.json'),JSON.stringify(result,null,2));await browser.close();}
 manifest.runs.push(key);fs.writeFileSync(path.join(out,'manifest.json'),JSON.stringify(manifest,null,2));
 console.log(`${manifest.runs.length}/${cases.length*modes.length*manifest.repeats} ${key}: ${result.result.update_intervals_per_second.toFixed(2)} updates/s`);
 }
}
})().catch(e=>{console.error(e);process.exitCode=1});
