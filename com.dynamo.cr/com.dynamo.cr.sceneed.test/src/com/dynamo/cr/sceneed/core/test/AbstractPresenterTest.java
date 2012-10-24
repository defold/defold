package com.dynamo.cr.sceneed.core.test;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.junit.Before;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;

public class AbstractPresenterTest {

    private IPresenterContext presenterContext;
    private ILoaderContext loaderContext;
    protected ISceneModel model;

    private int refreshRenderViewCounter;
    private int selectionCounter;
    private int executionCounter;

    public AbstractPresenterTest() {
        super();

        this.refreshRenderViewCounter = 0;
        this.selectionCounter = 0;
        this.executionCounter = 0;
    }

    @Before
    public void setup() {
        this.presenterContext = mock(IPresenterContext.class);
        this.loaderContext = mock(ILoaderContext.class);
        this.model = mock(ISceneModel.class);
    }

    protected IPresenterContext getPresenterContext() {
        return presenterContext;
    }

    protected void setPresenterContext(IPresenterContext presenterContext) {
        this.presenterContext = presenterContext;
    }

    protected ILoaderContext getLoaderContext() {
        return loaderContext;
    }

    protected void setLoaderContext(ILoaderContext loaderContext) {
        this.loaderContext = loaderContext;
    }

    protected ISceneModel getModel() {
        return model;
    }

    protected void select(Node node) {
        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(node));
    }

    protected void select(Node[] nodes) {
        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(nodes));
    }

    protected void verifyRefreshRenderView() {
        ++this.refreshRenderViewCounter;
        verify(this.presenterContext, times(this.refreshRenderViewCounter)).refreshRenderView();
    }

    protected void verifySelection() {
        ++this.selectionCounter;
        verify(this.presenterContext, times(this.selectionCounter)).setSelection(any(IStructuredSelection.class));
    }

    protected void verifyNoSelection() {
        verify(this.presenterContext, times(this.selectionCounter)).setSelection(any(IStructuredSelection.class));
    }

    protected void verifyExecution() {
        ++this.executionCounter;
        verify(this.presenterContext, times(this.executionCounter)).executeOperation(any(IUndoableOperation.class));
    }

    protected void verifyNoExecution() {
        verify(this.presenterContext, times(this.executionCounter)).executeOperation(any(IUndoableOperation.class));
    }

}
