package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.model.proto.Model.ModelDesc;
import com.dynamo.model.proto.Model.ModelDesc.Builder;
import com.google.protobuf.TextFormat;

public class ModelLoader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory) throws IOException,
            CreateException, CoreException {
        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = ModelDesc.newBuilder();
        TextFormat.merge(reader, builder);
        ModelDesc desc = builder.build();
        MeshResource meshResource = (MeshResource)factory.load(monitor, desc.getMesh());
        List<TextureResource> textureResources = new ArrayList<TextureResource>(desc.getTexturesCount());
        for (int i = 0; i < desc.getTexturesCount(); ++i) {
            textureResources.add((TextureResource)factory.load(monitor, desc.getTextures(i)));
        }
        return new ModelResource(name, desc, meshResource, textureResources);
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory) throws IOException,
            CreateException, CoreException {
        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = ModelDesc.newBuilder();
        TextFormat.merge(reader, builder);
        ModelDesc desc = builder.build();
        ModelResource modelResource = (ModelResource)resource;
        modelResource.setModelDesc(desc);
        modelResource.setMeshResource((MeshResource)factory.load(monitor, desc.getMesh()));
        List<TextureResource> textureResources = new ArrayList<TextureResource>(desc.getTexturesCount());
        for (int i = 0; i < desc.getTexturesCount(); ++i) {
            textureResources.add((TextureResource)factory.load(monitor, desc.getTextures(i)));
        }
        modelResource.setTextureResources(textureResources);
    }

}
