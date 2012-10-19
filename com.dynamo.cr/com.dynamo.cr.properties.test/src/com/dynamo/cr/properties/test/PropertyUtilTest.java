package com.dynamo.cr.properties.test;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.UndoContext;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.core.operations.MergeableDelegatingOperationHistory;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.properties.PropertyUtil;

public class PropertyUtilTest {


    private MergeableDelegatingOperationHistory history;
    private UndoContext context;
    private DummyWorld world;
    private DummyClass dummy1, dummy2;
    private PropertyIntrospectorModel<DummyClass, DummyWorld> model1;
    private PropertyIntrospectorModel<DummyClass, DummyWorld> model2;
    private static PropertyIntrospector<DummyClass, DummyWorld> introspector = new PropertyIntrospector<DummyClass, DummyWorld>(DummyClass.class);

    @Before
    public void setUp() {
        context = new UndoContext();
        history = new MergeableDelegatingOperationHistory(new DefaultOperationHistory());

        world = new DummyWorld();
        dummy1 = new DummyClass();
        dummy2 = new DummyClass();
        model1 = new PropertyIntrospectorModel<DummyClass, DummyWorld>(dummy1, world, introspector);
        model2 = new PropertyIntrospectorModel<DummyClass, DummyWorld>(dummy2, world, introspector);
    }

    private void execute(IUndoableOperation op) throws ExecutionException {
        op.addContext(context);
        history.execute(op, null, null);
    }

    @SuppressWarnings("unchecked")
    @Test
    public void test1() throws ExecutionException {

        IUndoableOperation op1 = PropertyUtil.setProperty(new IPropertyModel[] {model1}, "integerValue", 1010);
        execute(op1);

    }

}
