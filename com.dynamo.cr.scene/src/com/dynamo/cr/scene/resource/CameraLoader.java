package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.camera.proto.Camera;
import com.dynamo.camera.proto.Camera.CameraDesc.Builder;
import com.dynamo.cr.scene.graph.CreateException;
import com.google.protobuf.TextFormat;

public class CameraLoader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
            throws IOException, CreateException {

        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = Camera.CameraDesc.newBuilder();
        TextFormat.merge(reader, builder);
        return new CameraResource(name, builder.build());
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
            throws IOException, CreateException {
        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = Camera.CameraDesc.newBuilder();
        TextFormat.merge(reader, builder);
        ((CameraResource)resource).setCameraDesc(builder.build());
    }
}
