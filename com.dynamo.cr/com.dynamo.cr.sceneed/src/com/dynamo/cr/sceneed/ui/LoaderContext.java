package com.dynamo.cr.sceneed.ui;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.Node;
import com.google.inject.Inject;

public class LoaderContext implements
com.dynamo.cr.sceneed.core.ILoaderContext {

    private final IContainer contentRoot;
    private final INodeTypeRegistry nodeTypeRegistry;

    private Map<String, Node> nodeCache;

    @Inject
    public LoaderContext(IContainer contentRoot, INodeTypeRegistry nodeTypeRegistry) {
        this.contentRoot = contentRoot;
        this.nodeTypeRegistry = nodeTypeRegistry;
        this.nodeCache = new HashMap<String, Node>();
    }

    @Override
    public Node loadNode(String path) throws IOException, CoreException {
        if (path != null && !path.equals("")) {
            IFile file = this.contentRoot.getFile(new Path(path));
            if (file.exists()) {
                String ext = file.getFileExtension();
                Node n = null;
                INodeType nodeType = this.nodeTypeRegistry.getNodeTypeFromExtension(ext);
                boolean cached = nodeType != null && nodeType.isCached();
                if (cached) {
                    n = nodeCache.get(path);
                }
                if (n == null) {
                    n = loadNode(nodeType, file.getContents());
                    if (cached) {
                        nodeCache.put(path, n);
                    }
                }
                return n;
            }
        }
        return null;
    }

    private Node loadNode(INodeType nodeType, InputStream contents) throws IOException, CoreException {
        if (nodeType != null) {
            INodeLoader<Node> loader = nodeType.getLoader();
            try {
                return loader.load(this, contents);
            } catch (Exception e) {
                throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e));
            }
        }
        return null;
    }

    @Override
    public Node loadNode(String extension, InputStream contents)
            throws IOException, CoreException {
        INodeType nodeType = this.nodeTypeRegistry.getNodeTypeFromExtension(extension);
        return loadNode(nodeType, contents);
    }

    @Override
    public Node loadNodeFromTemplate(Class<? extends Node> nodeClass) throws IOException, CoreException {
        INodeType nodeType = this.nodeTypeRegistry.getNodeTypeClass(nodeClass);
        if (nodeType != null) {
            String extension = nodeType.getExtension();
            return loadNodeFromTemplate(extension);
        }
        return null;
    }

    @Override
    public Node loadNodeFromTemplate(String extension) throws IOException, CoreException {
        INodeType nodeType = this.nodeTypeRegistry.getNodeTypeFromExtension(extension);
        if (nodeType != null) {
            IResourceType resourceType = nodeType.getResourceType();
            ByteArrayInputStream stream = new ByteArrayInputStream(resourceType.getTemplateData());
            return loadNode(extension, stream);
        }
        return null;
    }

    @Override
    public INodeTypeRegistry getNodeTypeRegistry() {
        return this.nodeTypeRegistry;
    }

    @Override
    public void removeFromCache(String path) {
        this.nodeCache.remove(path);
    }
}
