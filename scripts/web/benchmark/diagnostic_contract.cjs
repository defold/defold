// Diagnostic records use one monotonic clock across browser main and pthread.
const assert=require('node:assert/strict');
const fields=['id','browser_tick','source','dispatch_begin','dispatch_end','wake','events_end','update_end','graphics_calls','owner_queue','owner_execute','owner_return','max_owner_queue'];
function validateDiagnostics(d) {
    assert(d,'Missing update diagnostics');
    assert.equal(d.schema,1);assert.equal(d.dropped,0,'Diagnostic buffer overflow');
    assert.deepEqual(d.fields,fields);assert(d.capacityBytes>0);assert(d.rows.length>0);
    let previous;
    for(const r of d.rows) {
        assert.equal(r.length,fields.length);assert(r.every(Number.isFinite));
        assert.equal(r[0],previous?previous[0]+1:1,'Nonsequential update ID');
        assert(Number.isInteger(r[1])&&r[1]>=0&&[0,1,2].includes(r[2]));
        // Absolute epoch doubles can differ by one ULP across main/worker clocks.
        // Allow at most one microsecond, preserving raw timestamps in the result.
        for(let i=4;i<=7;i++)assert(r[i]+0.001>=r[i-1],'Timestamp order');
        if(previous)assert(r[3]+0.001>=previous[7],'Concurrent producer updates');
        assert(Number.isInteger(r[8])&&r[8]>=0);
        assert(r.slice(9).every(n=>n>=-0.001));assert(r[12]<=r[9]+0.001);
        assert(r[9]+r[10]+r[11]<=r[7]-r[5]+0.01,'Graphics calls exceed worker time');
        previous=r;
    }
    return d.rows.map(r=>Object.fromEntries(fields.map((f,i)=>[f,r[i]])));
}
module.exports={validateDiagnostics,fields};
