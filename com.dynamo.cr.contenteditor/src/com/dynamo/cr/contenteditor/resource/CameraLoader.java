package com.dynamo.cr.contenteditor.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.camera.proto.Camera;
import com.dynamo.camera.proto.Camera.CameraDesc.Builder;
import com.dynamo.cr.contenteditor.scene.LoaderException;
import com.google.protobuf.TextFormat;

public class CameraLoader implements IResourceLoader {

    @Override
    public Object load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceLoaderFactory factory)
            throws IOException, LoaderException {

        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = Camera.CameraDesc.newBuilder();
        TextFormat.merge(reader, builder);
        return new CameraResource(builder.build());
    }

}
