// Validate a completed replay before accepting any performance measurements.
const assert = require('node:assert/strict');
const crypto = require('node:crypto');

function canonical(value) {
    if (Array.isArray(value)) return '[' + value.map(canonical).join(',') + ']';
    if (value && typeof value === 'object') return '{' + Object.keys(value).sort().map(k => JSON.stringify(k) + ':' + canonical(value[k])).join(',') + '}';
    return JSON.stringify(value);
}

function validateReplay(result, expected, diagnostic = false) {
    assert.equal(result.kind, 'defold-gameplay-replay');
    assert.equal(result.schema_version, 1);
    assert.equal(result.valid, true);
    assert.equal(result.cleanup_verified, true, 'Replay cleanup was not verified');
    assert.equal(!!result.diagnostic_timing,diagnostic,'Replay diagnostic mode differs');
    assert(!result.diagnostic_pixels, 'Readback replay is not performance evidence');
    for (const key of ['id', 'ticks', 'events']) assert.equal(result.replay[key], expected[key], 'Unexpected replay ' + key);
    assert.equal(result.frame_interval.samples, expected.measure_ticks);
    assert.equal(result.frame_interval.overflow_samples, 0);
    assert(result.render_script_calls > 0);
    assert(Number.isFinite(result.elapsed_seconds) && result.elapsed_seconds > 0);
    assert(result.replay.checkpoints.length > 1);
    assert.equal(result.replay.checkpoints.at(-1).tick, expected.ticks);
    assert.equal(result.replay.checkpoints.at(-1).phase, 'after_last_frame');
    return crypto.createHash('sha256').update(canonical({
        specification: result.replay.specification, ticks: result.replay.ticks,
        events: result.replay.events, checkpoints: result.replay.checkpoints,
    })).digest('hex');
}

module.exports = {validateReplay};
