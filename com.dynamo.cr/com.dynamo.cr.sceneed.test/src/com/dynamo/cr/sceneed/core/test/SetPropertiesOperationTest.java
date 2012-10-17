package com.dynamo.cr.sceneed.core.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import org.eclipse.core.commands.ExecutionException;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.core.operations.IMergeableOperation.Type;
import com.dynamo.cr.properties.BeanPropertyAccessor;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.operations.SetPropertiesOperation;

public class SetPropertiesOperationTest {

    private IPropertyAccessor<Object, ISceneModel> accessor;
    private ISceneModel scene;

    @SuppressWarnings("unchecked")
    @Before
    public void setUp() {
        scene = mock(ISceneModel.class);
        IPropertyAccessor<?, ? extends IPropertyObjectWorld> tmp = new BeanPropertyAccessor();
        accessor = (IPropertyAccessor<Object, ISceneModel>) tmp;
    }

    SetPropertiesOperation<Object, ISceneModel> createOp(DummyNode node, String property, Object oldValue, Object newValue) {
        SetPropertiesOperation<Object, ISceneModel> op = new SetPropertiesOperation<Object, ISceneModel>(node, property, accessor, oldValue, newValue, false, scene);
        op.setType(Type.INTERMEDIATE);
        return op;
    }

    @Test
    public void testMerge() throws ExecutionException {
        DummyNode node = new DummyNode();
        SetPropertiesOperation<Object, ISceneModel> op1 = createOp(node, "dummyProperty", 123, 456);
        SetPropertiesOperation<Object, ISceneModel> op2 = createOp(node, "dummyProperty", 123, 1010);

        op1.execute(null, null);
        assertEquals(456, node.getDummyProperty());

        assertTrue(op1.canMerge(op2));
        op1.mergeWith(op2);
        op1.execute(null, null);
        assertEquals(1010, node.getDummyProperty());
    }

    @Test
    public void testIncompatibleMerge1() throws ExecutionException {
        DummyNode node = new DummyNode();
        SetPropertiesOperation<Object, ISceneModel> op1 = createOp(node, "dummyProperty", 123, 456);
        SetPropertiesOperation<Object, ISceneModel> op2 = createOp(node, "dummyProperty", 123, "foo");
        assertFalse(op1.canMerge(op2));
    }

    @Test
    public void testIncompatibleMerge2() throws ExecutionException {
        DummyNode node1 = new DummyNode();
        DummyNode node2 = new DummyNode();
        SetPropertiesOperation<Object, ISceneModel> op1 = createOp(node1, "dummyProperty", 123, 456);
        SetPropertiesOperation<Object, ISceneModel> op2 = createOp(node2, "dummyProperty", 123, 1010);
        assertFalse(op1.canMerge(op2));
    }

    @Test
    public void testIncompatibleMerge3() throws ExecutionException {
        DummyNode node = new DummyNode();
        SetPropertiesOperation<Object, ISceneModel> op1 = createOp(node, "dummyProperty", 123, 456);
        SetPropertiesOperation<Object, ISceneModel> op2 = createOp(node, "foo", 123, 1010);
        assertFalse(op1.canMerge(op2));
    }


}
