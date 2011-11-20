package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.go.core.ComponentLoader;
import com.dynamo.cr.sceneed.core.Node;

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
