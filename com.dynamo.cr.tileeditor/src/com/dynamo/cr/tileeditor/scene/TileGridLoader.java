package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.google.protobuf.Message;

public class TileGridLoader implements INodeLoader<TileGridNode> {

    @Override
    public TileGridNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        return new TileGridNode();
    }

    @Override
    public Message buildMessage(ILoaderContext context, TileGridNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return null;
    }
}
