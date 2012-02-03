package com.dynamo.cr.go.core.test;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.jface.viewers.StructuredSelection;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.CollectionPresenter;
import com.dynamo.cr.go.core.GameObjectInstanceNode;
import com.dynamo.cr.go.core.InstanceNode;
import com.dynamo.cr.sceneed.core.test.AbstractPresenterTest;

public class CollectionPresenterTest extends AbstractPresenterTest {

    CollectionPresenter presenter;

    @Override
    @Before
    public void setup() {
        super.setup();
        this.presenter = new CollectionPresenter();
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testAddGameObjectEmptySelection() {
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection());
        this.presenter.onAddGameObject(this.getPresenterContext());
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testAddGameObjectIllegalSelection() {
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(new DummyComponentNode()));
        this.presenter.onAddGameObject(this.getPresenterContext());
    }

    @Test
    public void testAddGameObject() throws Exception {
        CollectionNode collection = new CollectionNode();
        collection.setModel(this.getModel());
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(collection));

        this.presenter.onAddGameObject(this.getPresenterContext());

        verify(this.getPresenterContext(), times(1)).executeOperation(any(IUndoableOperation.class));
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testAddCollectionEmptySelection() {
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection());
        this.presenter.onAddCollection(this.getPresenterContext());
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testAddCollectionIllegalSelection() {
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(new DummyComponentNode()));
        this.presenter.onAddCollection(this.getPresenterContext());
    }

    @Test
    public void testAddCollection() throws Exception {
        CollectionNode collection = new CollectionNode();
        collection.setModel(this.getModel());
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(collection));

        this.presenter.onAddCollection(this.getPresenterContext());

        verify(this.getPresenterContext(), times(1)).executeOperation(any(IUndoableOperation.class));
    }

    @Test
    public void testRemoveInstance() throws Exception {
        CollectionNode collection = new CollectionNode();
        collection.setModel(this.getModel());
        InstanceNode instance = new GameObjectInstanceNode(null);
        collection.addInstance(instance);
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(instance));

        this.presenter.onRemoveInstance(this.getPresenterContext());

        verify(this.getPresenterContext(), times(1)).executeOperation(any(IUndoableOperation.class));
    }
}
