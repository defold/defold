package com.dynamo.cr.sceneed.ui;

import javax.annotation.PreDestroy;
import javax.inject.Inject;

import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.IRenderViewProvider;
import com.dynamo.cr.sceneed.core.RenderContext;

public class GridRenderViewProvider implements IRenderViewProvider {

    private IRenderView renderView;
    private GridNode gridNode;

    @Inject
    public GridRenderViewProvider(IRenderView renderView) {
        this.renderView = renderView;
        renderView.addRenderProvider(this);
        this.gridNode = new GridNode(renderView.getGrid());
    }

    @PreDestroy
    public void dispose() {
        renderView.removeRenderProvider(this);
    }

    @Override
    public void setup(RenderContext renderContext) {
        renderView.setupNode(renderContext, gridNode);
    }

}
