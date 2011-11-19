package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.ISceneView.Context;
import com.dynamo.cr.sceneed.go.ComponentPresenter;
import com.dynamo.cr.sceneed.go.TileGridNode;

public class TileComponentPresenter extends ComponentPresenter {
    @Override
    public Node onCreateNode(Context context, String type) throws IOException,
    CoreException {
        if (type.equals("tilegrid")) {
            return new TileGridNode(type);
        }
        return super.onCreateNode(context, type);
    }
}
