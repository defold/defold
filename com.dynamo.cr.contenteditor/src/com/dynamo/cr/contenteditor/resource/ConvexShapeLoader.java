package com.dynamo.cr.contenteditor.resource;


import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.scene.LoaderException;
import com.dynamo.physics.proto.Physics.ConvexShape;
import com.dynamo.physics.proto.Physics.ConvexShape.Builder;
import com.google.protobuf.TextFormat;

public class ConvexShapeLoader implements IResourceLoader {

    @Override
    public Object load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceLoaderFactory factory)
            throws IOException, LoaderException {

        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = ConvexShape.newBuilder();
        TextFormat.merge(reader, builder);
        return new ConvexShapeResource(builder.build());
    }

}
