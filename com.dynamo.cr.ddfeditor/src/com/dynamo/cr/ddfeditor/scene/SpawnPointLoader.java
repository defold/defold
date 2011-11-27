package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.gamesystem.proto.GameSystem.SpawnPointDesc;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class SpawnPointLoader implements INodeLoader<SpawnPointNode> {

    @Override
    public SpawnPointNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        SpawnPointDesc.Builder builder = SpawnPointDesc.newBuilder();
        TextFormat.merge(reader, builder);
        SpawnPointDesc desc = builder.build();
        SpawnPointNode spawnPoint = new SpawnPointNode();
        spawnPoint.setPrototype(desc.getPrototype());
        return spawnPoint;
    }

    @Override
    public Message buildMessage(ILoaderContext context, SpawnPointNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        SpawnPointDesc.Builder builder = SpawnPointDesc.newBuilder();
        builder.setPrototype(node.getPrototype());
        return builder.build();
    }

}
