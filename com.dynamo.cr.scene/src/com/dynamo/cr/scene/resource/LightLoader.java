package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.LoaderException;
import com.dynamo.gamesystem.proto.GameSystem.LightDesc;
import com.dynamo.gamesystem.proto.GameSystem.LightDesc.Builder;
import com.google.protobuf.TextFormat;

public class LightLoader implements IResourceLoader {

    @Override
    public Object load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceLoaderFactory factory)
            throws IOException, LoaderException {

        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = LightDesc.newBuilder();
        TextFormat.merge(reader, builder);
        return new LightResource(builder.build());
    }

}
