const {test}=require('node:test');
const assert=require('node:assert/strict');
const {validateDiagnostics,fields}=require('./diagnostic_contract.cjs');
function record(){return {schema:1,dropped:0,capacityBytes:1024,fields,rows:[[1,1,0,10,11,12,13,20,1,1,2,1,1],[2,2,1,21,22,23,24,30,0,0,0,0,0]]};}
// Correlation must use sequential update IDs with nonoverlapping producer work.
test('accepts ordered cross-owner timing and returns named records',()=>{
    const d=validateDiagnostics(record());assert.equal(d[1].id,2);assert.equal(d[0].owner_execute,2);
});
// Truncated or impossible timing data must not enter an attribution report.
test('rejects dropped, reordered, overlapping and overcounted records',()=>{
    for(const mutate of [d=>d.dropped++,d=>d.rows[1][0]=3,d=>d.rows[0][5]=10,d=>d.rows[1][3]=19,d=>d.rows[0][10]=20]){
        const d=record();mutate(d);assert.throws(()=>validateDiagnostics(d));
    }
});

// Different absolute-clock rounding may reverse adjacent cross-owner timestamps by one ULP.
test('accepts sub-microsecond clock rounding but rejects a real reversal',()=>{
    const d=record();d.rows[0][5]=d.rows[0][4]-0.000244140625;
    assert.equal(validateDiagnostics(d).length,2);
    d.rows[0][5]=d.rows[0][4]-0.01;
    assert.throws(()=>validateDiagnostics(d),/Timestamp order/);
});
