package com.dynamo.cr.luaeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.ISceneView.Context;
import com.dynamo.cr.sceneed.go.ComponentPresenter;

public class ScriptPresenter extends ComponentPresenter {
    @Override
    public Node onLoad(Context context, String type, InputStream contents)
            throws IOException, CoreException {
        return onCreateNode(context, type);
    }

    @Override
    public Node onCreateNode(Context context, String type) throws IOException,
    CoreException {
        return new ScriptNode(type);
    }
}
