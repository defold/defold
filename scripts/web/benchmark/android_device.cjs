// Android Chrome transport for the visible web benchmark runner.
const {execFileSync}=require('node:child_process');
const assert=require('node:assert/strict');

function parseThermals(text) {
    const status=Number(text.match(/Thermal Status:\s*(\d+)/)?.[1]);
    const current=text.split('Current temperatures from HAL:')[1]?.split('Current cooling devices')[0]||'';
    const temperatures=[...current.matchAll(/mValue=([\d.]+), mType=(\d+), mName=([^,}]+), mStatus=(\d+)/g)]
        .map(m=>({celsius:Number(m[1]),type:Number(m[2]),name:m[3],status:Number(m[4])}));
    assert(Number.isInteger(status)&&temperatures.length,'Missing thermal telemetry');
    return {status,temperatures};
}
function parsePower(text) {
    const field=name=>text.match(new RegExp('^\\s*'+name+':\\s*(.+)$','m'))?.[1];
    const powered=['AC powered','USB powered','Wireless powered'].some(k=>field(k)==='true');
    const temperatureC=Number(field('temperature'))/10, level=Number(field('level'));
    assert(Number.isFinite(temperatureC)&&Number.isFinite(level),'Missing battery telemetry');
    return {powered,temperatureC,level};
}
function parseIdleCpu(text) {
    const headers=[...text.matchAll(/(\d+)%cpu[^\n]*?([\d.]+)%idle/g)];
    assert(headers.length,'Missing Android CPU telemetry');
    const last=headers.at(-1),totalPercent=Number(last[1]),idlePercent=Number(last[2]);
    assert(totalPercent>0&&idlePercent>=0&&idlePercent<=totalPercent);
    return {totalPercent,idlePercent,idleFraction:idlePercent/totalPercent};
}
function createAndroid() {
    const executable=process.env.ADB||'adb',serial=process.env.ADB_SERIAL;
    assert(serial,'ADB_SERIAL must select one device explicitly');
    const port=Number(process.env.ANDROID_CDP_PORT||9223);
    assert(Number.isInteger(port)&&port>1024&&port<65536);
    const adb=(...args)=>execFileSync(executable,['-s',serial,...args],{encoding:'utf8',timeout:30000}).trim();
    const shell=(...args)=>adb('shell',...args);
    const power=()=>{const p=parsePower(shell('dumpsys','battery'));assert(p.powered,'Android lost external power');assert.equal(shell('settings','get','global','low_power'),'0','Android Battery Saver is enabled');return p;};
    const thermal=()=>parseThermals(shell('dumpsys','thermalservice'));
    const idleCpu=()=>parseIdleCpu(shell('top','-b','-n','2','-d','1','-m','1'));
    const environment={platform:'android',device:shell('getprop','ro.product.model'),os:shell('getprop','ro.build.version.release'),arch:shell('getprop','ro.product.cpu.abi')};
    async function coolDown() {
        const brightness=shell('settings','get','system','screen_brightness');
        const automatic=shell('settings','get','system','screen_brightness_mode');
        assert(/^\d+$/.test(brightness)&&/^[01]$/.test(automatic),'Cannot preserve Android brightness settings');
        let restored=false;
        const restore=()=>{if(!restored){shell('settings','put','system','screen_brightness',brightness);shell('settings','put','system','screen_brightness_mode',automatic);restored=true;}};
        const interrupt=()=>{restore();process.exit(130);};
        process.once('SIGINT',interrupt);process.once('SIGTERM',interrupt);
        try {
        shell('settings','put','system','screen_brightness_mode','0');
        shell('settings','put','system','screen_brightness','0');
        const deadline=Date.now()+15*60*1000;
        while(true){
            power();const t=thermal();const skin=t.temperatures.filter(x=>x.type===3);
            if(t.status<=1&&skin.length){
                const cpu=idleCpu();
                if(cpu.idleFraction>=0.9)return {...t,idleCpu:cpu};
                console.log('Waiting for Android background CPU activity: '+(100*(1-cpu.idleFraction)).toFixed(1)+'% of total capacity busy');
            }
            assert(Date.now()<deadline,'Device did not reach thermal status <=1 / CPU idle >=90%');
            console.log('Waiting for Android cooldown: status='+t.status+', skin='+skin.map(x=>x.celsius.toFixed(1)).join(',')+' C');
            await new Promise(r=>setTimeout(r,30000));
        }
        } finally {
            restore();process.removeListener('SIGINT',interrupt);process.removeListener('SIGTERM',interrupt);
        }
    }
    async function launch(chromium) {
        shell('am','force-stop','com.android.chrome');
        const thermalBefore=await coolDown();
        shell('am','start','-W','-a','android.intent.action.VIEW','-d','about:blank','-p','com.android.chrome');
        adb('forward','tcp:'+port,'localabstract:chrome_devtools_remote');
        let browser;
        for(let i=0;i<30&&!browser;i++){
            try{browser=await chromium.connectOverCDP('http://127.0.0.1:'+port,{timeout:3000});}
            catch(e){if(i===29)throw e;await new Promise(r=>setTimeout(r,500));}
        }
        const context=browser.contexts()[0];
        const page=await context.newPage();
        return {browser,page,thermalBefore};
    }
    async function close(browser,page) {
        if(page)await page.close().catch(()=>{});
        if(browser)await browser.close().catch(()=>{});
        shell('am','force-stop','com.android.chrome');
    }
    return {environment,power,thermal,idleCpu,launch,close,adb};
}
module.exports={parsePower,parseThermals,parseIdleCpu,createAndroid};
