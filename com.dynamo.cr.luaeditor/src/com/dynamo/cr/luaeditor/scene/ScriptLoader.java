package com.dynamo.cr.luaeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.go.core.ComponentLoader;
import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.Node;

public class ScriptLoader extends ComponentLoader {
    @Override
    public Node load(ILoaderContext context, String type, InputStream contents)
            throws IOException, CoreException {
        return createNode(type);
    }

    @Override
    public Node createNode(String type) throws IOException,
    CoreException {
        return new ScriptNode(type);
    }
}
