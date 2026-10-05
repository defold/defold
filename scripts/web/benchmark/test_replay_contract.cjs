const {test} = require('node:test');
const assert = require('node:assert/strict');
const {validateReplay} = require('./replay_contract.cjs');
const expected = {id:'fixture', ticks:120, events:2, measure_ticks:100};
function fixture() {
    return {kind:'defold-gameplay-replay', schema_version:1, valid:true, cleanup_verified:true,
        elapsed_seconds:2, render_script_calls:100, frame_interval:{samples:100, overflow_samples:0},
        replay:{id:'fixture', ticks:120, events:2, specification:'fixed', checkpoints:[
            {tick:0, state:'initial'}, {tick:120, phase:'after_last_frame', state:'final'}]}};
}
// Guards against accepting different gameplay solely because timing looks valid.
test('a changed checkpoint changes the replay signature', () => {
    const original=fixture(), changed=fixture(); changed.replay.checkpoints[1].state='different';
    assert.notEqual(validateReplay(original,expected),validateReplay(changed,expected));
});
// Missing work, cleanup, and timing overflow must invalidate an evaluation.
test('incomplete replay evidence is rejected', () => {
    for (const mutate of [r=>r.cleanup_verified=false,r=>r.replay.events--,
        r=>r.frame_interval.overflow_samples++,r=>r.replay.checkpoints.pop()]) {
        const r=fixture(); mutate(r); assert.throws(()=>validateReplay(r,expected));
    }
});
// JSON key ordering is not gameplay divergence across browser or tool versions.
test('object key order does not affect replay signature', () => {
    const a=fixture(),b=fixture(); b.replay.checkpoints[1]={state:'final',phase:'after_last_frame',tick:120};
    assert.equal(validateReplay(a,expected),validateReplay(b,expected));
});
// Trace-enabled replays require explicit diagnostic collection and cannot pass as timing controls.
test('instrumented replay must match explicit diagnostic selection',()=>{
    const r=fixture();r.diagnostic_timing=true;
    assert.throws(()=>validateReplay(r,expected),/diagnostic mode differs/);
    assert.equal(validateReplay(r,expected,true),validateReplay(fixture(),expected));
    assert.throws(()=>validateReplay(fixture(),expected,true),/diagnostic mode differs/);
});
