package com.dynamo.cr.contenteditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;

import javax.xml.stream.XMLStreamException;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.util.ColladaUtil;

public class MeshLoader implements ILoader {

    Map<String, Mesh> meshCache = new HashMap<String, Mesh>();

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream,
            LoaderFactory factory) throws IOException, LoaderException {

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
            LoaderFactory loaderFactory) throws IOException, LoaderException {
        // TODO:
        throw new RuntimeException("TODO");

    }

}
