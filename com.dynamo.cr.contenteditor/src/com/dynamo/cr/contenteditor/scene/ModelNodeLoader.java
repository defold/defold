package com.dynamo.cr.contenteditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.editors.TextureResource;
import com.dynamo.cr.contenteditor.resource.IResourceLoaderFactory;
import com.dynamo.ddf.DDF;
import com.dynamo.model.ddf.Model.ModelDesc;

public class ModelNodeLoader implements INodeLoader {

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream, INodeLoaderFactory factory, IResourceLoaderFactory resourceFactory, Node parent)
            throws IOException, LoaderException, CoreException {

        InputStreamReader reader = new InputStreamReader(stream);
        ModelDesc model_desc = DDF.loadTextFormat(reader, ModelDesc.class);

        TextureResource texture = null;
        if (model_desc.m_Textures.size() > 0) {
            texture = (TextureResource) resourceFactory.load(monitor, model_desc.m_Textures.get(0));
        }

        MeshNode mesh_node = (MeshNode) factory.load(monitor, scene, model_desc.m_Mesh, null);
        mesh_node.textureResource = texture;
        return new ModelNode(scene, name, mesh_node);
    }

    @Override
    public void save(IProgressMonitor monitor, String name, Node node, OutputStream stream,
            INodeLoaderFactory loaderFactory) throws IOException, LoaderException {
        throw new UnsupportedOperationException();
    }


}
