package com.dynamo.cr.contenteditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.ddf.DDF;
import com.dynamo.model.ddf.Model.ModelDesc;

public class ModelLoader implements ILoader {

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream, LoaderFactory factory)
            throws IOException, LoaderException {

        InputStreamReader reader = new InputStreamReader(stream);
        ModelDesc model_desc = DDF.loadTextFormat(reader, ModelDesc.class);

        Node mesh_node = factory.load(monitor, scene, model_desc.m_Mesh);
        return new ModelNode(scene, name, (MeshNode) mesh_node);
    }

    @Override
    public void save(IProgressMonitor monitor, String name, Node node, OutputStream stream,
            LoaderFactory loaderFactory) throws IOException, LoaderException {
        // TODO:
        throw new RuntimeException("TODO");

    }


}
