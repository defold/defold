package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.physics.proto.Physics.ConvexShape;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class ConvexShapeLoader implements INodeLoader<ConvexShapeNode> {

    @Override
    public ConvexShapeNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        ConvexShape.Builder builder = ConvexShape.newBuilder();
        TextFormat.merge(reader, builder);
        ConvexShape convexShape = builder.build();
        ConvexShapeNode convexShapeNode = new ConvexShapeNode(convexShape);
        return convexShapeNode;
    }

    @Override
    public Message buildMessage(ILoaderContext context, ConvexShapeNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        return node.getConvexShape();
    }

}
