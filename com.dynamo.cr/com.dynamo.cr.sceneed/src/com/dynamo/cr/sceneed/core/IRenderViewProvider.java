package com.dynamo.cr.sceneed.core;

import java.util.List;

public interface IRenderViewProvider {

    void setup(RenderContext renderContext);
    void onNodeHit(List<Node> nodes);
}
