package com.dynamo.cr.editor.core.test.operations;

import static org.junit.Assert.assertEquals;

import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.commands.operations.UndoContext;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.core.operations.MergeableDelegatingOperationHistory;
import com.dynamo.cr.editor.core.operations.IMergeableOperation.Type;

public class MergableOperationHistoryTest implements IOperationHistoryListener {

    private MergeableDelegatingOperationHistory history;
    private UndoContext context;
    private Map<Integer, Integer> events = new HashMap<Integer, Integer>();

    @Before
    public void setup() throws Exception {
        context = new UndoContext();
        history = new MergeableDelegatingOperationHistory(new DefaultOperationHistory());
        history.addOperationHistoryListener(this);
    }

    @Test
    public void testMergeable() throws Exception {
        DummyOperation op1 = newOp("foo");
        DummyOperation op2 = newOp("bar");

        assertEquals(0, undoLength());
        execute(op1);
        assertEquals(1, undoLength());
        execute(op2);
        assertEquals(1, undoLength());

        assertEquals(1, eventCount(OperationHistoryEvent.DONE));
        assertEquals(1, eventCount(OperationHistoryEvent.OPERATION_CHANGED));
    }

    @Test
    public void testNotMergeable() throws Exception {
        DummyOperation op1 = newOp("foo");
        DummyOperation op2 = newOp(123);

        assertEquals(0, undoLength());
        execute(op1);
        assertEquals(1, undoLength());
        execute(op2);
        assertEquals(2, undoLength());

        assertEquals(2, eventCount(OperationHistoryEvent.DONE));
        assertEquals(0, eventCount(OperationHistoryEvent.OPERATION_CHANGED));
    }

    @Test
    public void testClose() throws Exception {
        DummyOperation op1 = newOp(123);
        DummyOperation op2 = newOp(456);
        DummyOperation op3 = newOp(789);
        op2.setType(Type.CLOSE);

        assertEquals(0, undoLength());
        execute(op1);
        assertEquals(1, undoLength());
        execute(op2);
        assertEquals(1, undoLength());
        execute(op3);
        assertEquals(2, undoLength());

        assertEquals(2, eventCount(OperationHistoryEvent.DONE));
        assertEquals(1, eventCount(OperationHistoryEvent.OPERATION_CHANGED));
    }

    private DummyOperation newOp(Object value) {
        DummyOperation op = new DummyOperation(value);
        op.setType(Type.INTERMEDIATE);
        return op;
    }

    private int eventCount(int t) {
        if (events.containsKey(t))
            return (int) events.get(t);
        else
            return 0;
    }

    private int undoLength() {
        return history.getUndoHistory(context).length;
    }

    private void execute(IUndoableOperation op) throws ExecutionException {
        op.addContext(context);
        history.execute(op, null, null);
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        int count = 0;
        int type = event.getEventType();
        if (events.containsKey(type)) {
            count = events.get(type);
        }
        count++;
        events.put(type, count);
    }

}
