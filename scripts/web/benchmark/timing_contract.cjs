// A stress run may exceed the histogram range without invalidating throughput.
const assert=require('node:assert/strict');

function validateTiming(timing,{allowOverflow=false}={}) {
    assert(Number.isInteger(timing.samples)&&timing.samples>0,'Missing timing samples');
    assert(Number.isInteger(timing.overflow_samples)&&timing.overflow_samples>=0&&timing.overflow_samples<=timing.samples,'Invalid overflow count');
    assert(Number.isFinite(timing.histogram_limit_ms)&&timing.histogram_limit_ms>0,'Missing histogram limit');
    if(!allowOverflow)assert.equal(timing.overflow_samples,0,'Timing histogram overflow; p99 may be unavailable');
    if(timing.p99_ms_upper_bound===undefined){
        assert(allowOverflow&&timing.overflow_samples>0,'Missing p99 without recorded overflow');
        assert(timing.overflow_samples>timing.samples-Math.ceil(timing.samples*.99),'Overflow does not explain missing p99');
        assert(timing.max_ms>timing.histogram_limit_ms,'Overflow lacks an out-of-range maximum');
    }else{
        assert(Number.isFinite(timing.p99_ms_upper_bound)&&timing.p99_ms_upper_bound>0&&timing.p99_ms_upper_bound<=timing.histogram_limit_ms,'Invalid p99');
    }
}
module.exports={validateTiming};
