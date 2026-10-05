const {test} = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
function config(search, file='/index.html') {
    const html=fs.readFileSync(__dirname+file,'utf8');
    const context={URLSearchParams,location:{search},crossOriginIsolated:true,
        document:{getElementById:()=>({style:{}})},console};
    vm.runInNewContext(html.match(/<script>([\s\S]*?)<\/script>/)[1],context);
    return Array.from(context.Module.arguments);
}
// Real-project evaluation must preserve sound; synthetic scenes may disable it.
test('replay launch keeps sound capacity and enables the deterministic adapter',()=>{
    const args=config('?scene=replay&mode=overlap_completion&metrics=0');
    assert(!args.includes('--config=sound.max_sound_instances=0'));
    assert(args.includes('--config=replay.enabled=1'));
    assert(args.includes('--config=render.poc_web_components=1'));
    assert(args.includes('--config=render.poc_web_schedule=1'));
});
// The barrier comparison must retain identical scheduling while disabling preparation overlap.
test('completion barrier is distinct from full overlap',()=>{
    const args=config('?mode=barrier_completion');
    assert(args.includes('--config=render.poc_web_prepare_overlap=0'));
    assert(args.includes('--config=render.poc_web_overlap=1'));
    assert(args.includes('--config=render.poc_web_schedule=1'));
});
// A project manifest must not silently override the mode under evaluation.
test('replay config rejects execution-policy overrides',()=>{
    assert.throws(()=>config('?scene=replay&config='+encodeURIComponent(JSON.stringify({'render.poc_threaded':0}))),/Unexpected replay setting/);
});

// Memory diagnostics and ownership must stay independently opt-in for valid A/B runs.
test('memory diagnostics and model ownership default off and can be selected',()=>{
    const defaults=config('?mode=overlap_completion');
    assert(defaults.includes('--config=render.poc_web_owned_model_buffers=0'));
    assert(defaults.includes('--config=benchmark.memory_diagnostic=0'));
    const enabled=config('?mode=overlap_completion&owned_models=1&memory_diagnostic=1&memory_samples=0');
    assert(enabled.includes('--config=render.poc_web_owned_model_buffers=1'));
    assert(enabled.includes('--config=benchmark.memory_diagnostic=1'));
    assert(enabled.includes('--config=benchmark.memory_samples=0'));
});

// Work placement and browser pacing are separate switches, never an automatic scene heuristic.
test('deferred sprite geometry and paced completion can be selected independently',()=>{
    const geometry=config('?mode=overlap_completion&deferred_sprites=1');
    assert(geometry.includes('--config=render.poc_web_schedule=1'));
    assert(geometry.includes('--config=render.poc_web_deferred_sprites=1'));
    const pacing=config('?mode=overlap_paced');
    assert(pacing.includes('--config=render.poc_web_schedule=3'));
    assert(pacing.includes('--config=render.poc_web_deferred_sprites=0'));
    assert(pacing.includes('--config=render.poc_web_components=1'));
    assert(pacing.includes('--config=render.poc_web_overlap=1'));
});

// Detailed timing allocates buffers and must never silently affect acceptance runs.
test('update diagnostics remain opt-in',()=>{
    assert(config('?mode=overlap_completion').includes('--config=render.poc_web_diagnostics=0'));
    assert(config('?mode=overlap_completion&diagnostics=1').includes('--config=render.poc_web_diagnostics=1'));
});

// Readiness consumption is an explicit experiment and must retain component snapshots.
test('ready render scheduling is opt-in',()=>{
    const args=config('?mode=overlap_ready&deferred_sprites=1');
    assert(args.includes('--config=render.poc_web_schedule=4'));
    assert(args.includes('--config=render.poc_web_components=1'));
    assert(args.includes('--config=render.poc_web_overlap=1'));
    assert(config('?mode=overlap_completion').includes('--config=render.poc_web_schedule=1'));
});
