package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.go.core.ComponentLoader;
import com.dynamo.cr.sceneed.core.Node;

public class Sprite2Loader extends ComponentLoader {
    @Override
    public Node createNode(String type) throws IOException,
    CoreException {
        if (type.equals("sprite2")) {
            return new Sprite2Node();
        }
        return super.createNode(type);
    }
}
