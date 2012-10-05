package com.dynamo.cr.sceneed.ui;

import javax.annotation.PreDestroy;
import javax.inject.Inject;

import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.IRenderViewProvider;
import com.dynamo.cr.sceneed.core.RenderContext;

public class BackgroundRenderViewProvider implements IRenderViewProvider {

    private IRenderView renderView;
    private BackgroundNode backgroundNode;

    @Inject
    public BackgroundRenderViewProvider(IRenderView renderView) {
        this.renderView = renderView;
        renderView.addRenderProvider(this);
        this.backgroundNode = new BackgroundNode();
    }

    @PreDestroy
    public void dispose() {
        renderView.removeRenderProvider(this);
    }

    @Override
    public void setup(RenderContext renderContext) {
        renderView.setupNode(renderContext, backgroundNode);
    }

}
