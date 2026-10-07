const test=require('node:test');
const assert=require('node:assert/strict');
const {validateTiming}=require('./timing_contract.cjs');

// Normal collection must retain its strict rejection of histogram overflow.
test('timing overflow is rejected unless explicitly allowed',()=>{
    const timing={samples:45,overflow_samples:4,histogram_limit_ms:1000,max_ms:1122};
    assert.throws(()=>validateTiming(timing),/overflow/);
    validateTiming(timing,{allowOverflow:true});
    assert.equal(timing.p99_ms_upper_bound,undefined);
});

// An explicit stress policy must not turn missing or malformed timing into evidence.
test('stress timing still rejects unexplained missing p99 and malformed data',()=>{
    const base={samples:45,overflow_samples:4,histogram_limit_ms:1000,max_ms:1122};
    for(const change of [{overflow_samples:0},{overflow_samples:46},{samples:0},{max_ms:999},{histogram_limit_ms:0},{p99_ms_upper_bound:1500},{samples:500,overflow_samples:1}]){
        assert.throws(()=>validateTiming({...base,...change},{allowOverflow:true}));
    }
});

// In-range percentiles remain usable when rare tail samples exceed the range.
test('normal percentiles and rare overflow preserve their measured bounds',()=>{
    validateTiming({samples:500,overflow_samples:0,histogram_limit_ms:1000,max_ms:40,p99_ms_upper_bound:39});
    validateTiming({samples:500,overflow_samples:1,histogram_limit_ms:1000,max_ms:1100,p99_ms_upper_bound:900},{allowOverflow:true});
});
