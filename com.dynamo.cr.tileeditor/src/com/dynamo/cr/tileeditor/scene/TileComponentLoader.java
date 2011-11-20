package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.go.ComponentLoader;
import com.dynamo.cr.sceneed.go.TileGridNode;

public class TileComponentLoader extends ComponentLoader {
    @Override
    public Node createNode(String type) throws IOException,
    CoreException {
        if (type.equals("tilegrid")) {
            return new TileGridNode(type);
        }
        return super.createNode(type);
    }
}
