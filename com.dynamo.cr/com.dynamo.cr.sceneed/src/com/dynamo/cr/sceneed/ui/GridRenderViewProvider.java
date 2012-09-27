package com.dynamo.cr.sceneed.ui;

import java.util.List;

import javax.annotation.PreDestroy;
import javax.inject.Inject;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.IRenderViewProvider;
import com.dynamo.cr.sceneed.core.Node;
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

    @Override
    public void onNodeHit(List<Node> nodes, MouseEvent event, MouseEventType mouseEventType) {
    }

    @Override
    public boolean hasFocus(List<Node> nodes) {
        return false;
    }

}
