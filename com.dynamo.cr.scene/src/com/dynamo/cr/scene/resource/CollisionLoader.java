package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.SubProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc.Builder;
import com.google.protobuf.TextFormat;

public class CollisionLoader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
            throws IOException, CreateException, CoreException {

        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = CollisionObjectDesc.newBuilder();
        TextFormat.merge(reader, builder);
        CollisionObjectDesc desc = builder.build();
        ConvexShapeResource convexShape = (ConvexShapeResource)factory.load(new SubProgressMonitor(monitor, 1), desc.getCollisionShape());
        return new CollisionResource(name, desc, convexShape);
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
            throws IOException, CreateException {
        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = CollisionObjectDesc.newBuilder();
        TextFormat.merge(reader, builder);
        ((CollisionResource)resource).setCollisionDesc(builder.build());
    }
}
