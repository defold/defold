package com.dynamo.cr.sceneed.ui;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneGrid;

@SuppressWarnings("serial")
public class GridNode extends Node {

    private SceneGrid grid;

    public GridNode(SceneGrid grid) {
        this.grid = grid;
    }

    public SceneGrid getGrid() {
        return this.grid;
    }

}
