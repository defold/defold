package com.dynamo.cr.contenteditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;

import javax.xml.stream.XMLStreamException;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.resource.IResourceLoaderFactory;
import com.dynamo.cr.contenteditor.util.ColladaUtil;

public class MeshNodeLoader implements INodeLoader {

    Map<String, Mesh> meshCache = new HashMap<String, Mesh>();

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream,
            INodeLoaderFactory factory, IResourceLoaderFactory resourceFactory) throws IOException, LoaderException {

        try {
            Mesh mesh;
            if (!meshCache.containsKey(name)) {
                mesh = ColladaUtil.loadMesh(stream);
                meshCache.put(name, mesh);
            }
            mesh = meshCache.get(name);

            return new MeshNode(scene, name, mesh);
        } catch (XMLStreamException e) {
            throw new LoaderException(e);
        }
    }

    @Override
    public void save(IProgressMonitor monitor, String name, Node node, OutputStream stream,
            INodeLoaderFactory loaderFactory) throws IOException, LoaderException {
        throw new UnsupportedOperationException();
    }

}
