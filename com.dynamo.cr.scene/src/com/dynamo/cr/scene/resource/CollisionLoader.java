package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.LoaderException;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc.Builder;
import com.google.protobuf.TextFormat;

public class CollisionLoader implements IResourceLoader {

    @Override
    public Object load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceLoaderFactory factory)
            throws IOException, LoaderException {

        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = CollisionObjectDesc.newBuilder();
        TextFormat.merge(reader, builder);
        return new CollisionResource(builder.build());
    }

}
