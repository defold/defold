package com.dynamo.cr.scene.resource;


import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.physics.proto.Physics.ConvexShape;
import com.dynamo.physics.proto.Physics.ConvexShape.Builder;
import com.google.protobuf.TextFormat;

public class ConvexShapeLoader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
            throws IOException, CreateException {

        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = ConvexShape.newBuilder();
        TextFormat.merge(reader, builder);
        return new ConvexShapeResource(name, builder.build());
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
            throws IOException, CreateException {
        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = ConvexShape.newBuilder();
        TextFormat.merge(reader, builder);
        ((ConvexShapeResource)resource).setConvexShape(builder.build());
    }
}
