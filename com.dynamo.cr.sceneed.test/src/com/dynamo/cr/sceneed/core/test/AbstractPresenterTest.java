package com.dynamo.cr.sceneed.core.test;

import static org.mockito.Mockito.mock;

import org.junit.Before;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;

public class AbstractPresenterTest {

    private IPresenterContext presenterContext;
    private ILoaderContext loaderContext;
    private ISceneModel model;

    public AbstractPresenterTest() {
        super();
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

}
