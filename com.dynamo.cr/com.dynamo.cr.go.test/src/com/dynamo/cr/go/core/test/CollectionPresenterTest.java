package com.dynamo.cr.go.core.test;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.jface.viewers.StructuredSelection;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.CollectionPresenter;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.sceneed.core.test.AbstractPresenterTest;

public class CollectionPresenterTest extends AbstractPresenterTest {

    CollectionPresenter presenter;

    @Override
    @Before
    public void setup() {
        super.setup();
        this.presenter = new CollectionPresenter();
    }

    @Test
    public void testAddEmbedGameObject() throws Exception {
        CollectionNode collection = new CollectionNode();
        collection.setModel(this.getModel());
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(collection));

        this.presenter.onAddGameObject(this.getPresenterContext(), this.getLoaderContext());

        verify(this.getPresenterContext(), times(1)).executeOperation(any(IUndoableOperation.class));
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testAddGameObjectFromFileEmptySelection() {
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection());
        this.presenter.onAddGameObjectFromFile(this.getPresenterContext(), this.getLoaderContext());
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testAddGameObjectFromFileIllegalSelection() {
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(new DummyComponentNode()));
        this.presenter.onAddGameObjectFromFile(this.getPresenterContext(), this.getLoaderContext());
    }

    @Test
    public void testAddGameObject() throws Exception {
        CollectionNode collection = new CollectionNode();
        collection.setModel(this.getModel());
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(collection));
        when(this.getPresenterContext().selectFile(anyString(), any(String[].class))).thenReturn("/test.go");
        when(this.getLoaderContext().loadNode("/test.go")).thenReturn(new GameObjectNode());

        this.presenter.onAddGameObject(this.getPresenterContext(), this.getLoaderContext());

        verify(this.getPresenterContext(), times(1)).executeOperation(any(IUndoableOperation.class));
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testAddCollectionEmptySelection() {
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection());
        this.presenter.onAddCollection(this.getPresenterContext(), this.getLoaderContext());
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testAddCollectionIllegalSelection() {
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(new DummyComponentNode()));
        this.presenter.onAddCollection(this.getPresenterContext(), this.getLoaderContext());
    }

    @Test
    public void testAddCollection() throws Exception {
        CollectionNode collection = new CollectionNode();
        collection.setModel(this.getModel());
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(collection));
        when(this.getPresenterContext().selectFile(anyString(), any(String[].class))).thenReturn("/test.collection");
        when(this.getLoaderContext().loadNode("/test.collection")).thenReturn(new CollectionNode());

        this.presenter.onAddCollection(this.getPresenterContext(), this.getLoaderContext());

        verify(this.getPresenterContext(), times(1)).executeOperation(any(IUndoableOperation.class));
    }

}
