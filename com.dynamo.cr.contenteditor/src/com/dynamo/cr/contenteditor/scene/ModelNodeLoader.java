package com.dynamo.cr.contenteditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.editors.TextureResource;
import com.dynamo.cr.contenteditor.resource.IResourceLoaderFactory;
import com.dynamo.model.proto.Model.ModelDesc;
import com.google.protobuf.TextFormat;

public class ModelNodeLoader implements INodeLoader {

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream, INodeLoaderFactory factory, IResourceLoaderFactory resourceFactory, Node parent)
            throws IOException, LoaderException, CoreException {

        InputStreamReader reader = new InputStreamReader(stream);
        ModelDesc.Builder builder = ModelDesc.newBuilder();
        TextFormat.merge(reader, builder);
        ModelDesc modelDesc = builder.build();

        TextureResource texture = null;
        if (modelDesc.getTexturesCount() > 0) {
            texture = (TextureResource) resourceFactory.load(monitor, modelDesc.getTextures(0));
        }

        MeshNode mesh_node = (MeshNode) factory.load(monitor, scene, modelDesc.getMesh(), null);
        mesh_node.textureResource = texture;
        return new ModelNode(name, scene, mesh_node);
    }

    @Override
    public void save(IProgressMonitor monitor, String name, Node node, OutputStream stream,
            INodeLoaderFactory loaderFactory) throws IOException, LoaderException {
        throw new UnsupportedOperationException();
    }


}
