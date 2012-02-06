package com.dynamo.cr.sceneed.ui;

import java.util.List;

import javax.annotation.PreDestroy;

import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.IRenderViewProvider;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.RenderContext;

public class SelectionBoxRenderViewProvider implements IRenderViewProvider {

    private IRenderView renderView;
    private SelectionBoxNode selectionBoxNode;

    public SelectionBoxRenderViewProvider(IRenderView renderView, SelectionBoxNode node) {
        this.renderView = renderView;
        this.selectionBoxNode = node;
        renderView.addRenderProvider(this);
    }

    @PreDestroy
    public void dispose() {
        renderView.removeRenderProvider(this);
    }

    @Override
    public void setup(RenderContext renderContext) {
        renderView.setupNode(renderContext, selectionBoxNode);
    }

    @Override
    public void onNodeHit(List<Node> nodes) {
    }


}
