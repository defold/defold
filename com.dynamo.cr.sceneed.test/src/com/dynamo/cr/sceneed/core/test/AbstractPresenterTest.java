package com.dynamo.cr.sceneed.core.test;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.junit.Before;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;

public class AbstractPresenterTest {

    private IPresenterContext presenterContext;
    private ILoaderContext loaderContext;
    private ISceneModel model;

    private int refreshCounter;
    private int selectionCounter;
    private int executionCounter;

    public AbstractPresenterTest() {
        super();

        this.refreshCounter = 0;
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

    protected void setModel(ISceneModel model) {
        this.model = model;
    }

    protected void verifyRefresh() {
        ++this.refreshCounter;
        verify(this.presenterContext, times(this.refreshCounter)).refreshView();
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
