package com.dynamo.cr.go.core.test;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.jface.viewers.ILabelProvider;
import org.eclipse.jface.viewers.StructuredSelection;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.GameObjectPresenter;
import com.dynamo.cr.sceneed.core.test.AbstractPresenterTest;

public class GameObjectPresenterTest extends AbstractPresenterTest {

    GameObjectPresenter presenter;

    @Override
    @Before
    public void setup() {
        super.setup();
        this.presenter = new GameObjectPresenter();
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testAddComponentEmptySelection() {
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection());
        this.presenter.onAddComponent(this.getPresenterContext(), this.getLoaderContext());
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testAddComponentIllegalSelection() {
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(new DummyComponentNode()));
        this.presenter.onAddComponent(this.getPresenterContext(), this.getLoaderContext());
    }

    @Test
    public void testAddComponent() throws Exception {
        GameObjectNode gameObject = new GameObjectNode();
        gameObject.setModel(this.getModel());
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(gameObject));
        IResourceType resource = mock(IResourceType.class);
        when(resource.getFileExtension()).thenReturn("dummy");
        when(this.getPresenterContext().selectFromArray(anyString(), anyString(), any(Object[].class), any(ILabelProvider.class))).thenReturn(resource);
        when(this.getLoaderContext().loadNodeFromTemplate("dummy")).thenReturn(new DummyComponentNode());

        this.presenter.onAddComponent(this.getPresenterContext(), this.getLoaderContext());

        verify(this.getPresenterContext(), times(1)).executeOperation(any(IUndoableOperation.class));
    }

    @Test
    public void testAddComponentFromFile() throws Exception {
        GameObjectNode gameObject = new GameObjectNode();
        gameObject.setModel(this.getModel());
        when(this.getPresenterContext().getSelection()).thenReturn(new StructuredSelection(gameObject));
        when(this.getPresenterContext().selectFile(anyString())).thenReturn("dummy");
        when(this.getLoaderContext().loadNode("dummy")).thenReturn(new DummyComponentNode());

        this.presenter.onAddComponentFromFile(this.getPresenterContext(), this.getLoaderContext());

        verify(this.getPresenterContext(), times(1)).executeOperation(any(IUndoableOperation.class));
    }

}
