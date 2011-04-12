package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;

import javax.xml.stream.XMLStreamException;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.cr.scene.util.ColladaUtil;

public class MeshLoader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory) throws IOException,
            CreateException, CoreException {
        try {
            return new MeshResource(name, ColladaUtil.loadMesh(stream));
        } catch (XMLStreamException e) {
            throw new CreateException(e.getMessage(), e);
        }
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory) throws IOException,
            CreateException, CoreException {
        try {
            ((MeshResource)resource).setMesh(ColladaUtil.loadMesh(stream));
        } catch (XMLStreamException e) {
            throw new CreateException(e.getMessage(), e);
        }
    }

}
