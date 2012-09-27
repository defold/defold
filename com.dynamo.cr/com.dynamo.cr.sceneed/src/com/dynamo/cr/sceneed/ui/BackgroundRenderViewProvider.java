package com.dynamo.cr.sceneed.ui;

import java.util.List;

import javax.annotation.PreDestroy;
import javax.inject.Inject;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.IRenderViewProvider;
import com.dynamo.cr.sceneed.core.Node;
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

    @Override
    public void onNodeHit(List<Node> nodes, MouseEvent event, MouseEventType mouseEventType) {
    }

    @Override
    public boolean hasFocus(List<Node> nodes) {
        return false;
    }

}
