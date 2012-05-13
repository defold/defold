package com.dynamo.cr.sceneed.ui;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.editor.core.ILogger;
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
    private final ILogger logger;

    @Inject
    public LoaderContext(IContainer contentRoot, INodeTypeRegistry nodeTypeRegistry, ILogger logger) {
        this.contentRoot = contentRoot;
        this.nodeTypeRegistry = nodeTypeRegistry;
        this.logger = logger;
    }

    @Override
    public void logException(Throwable exception) {
        this.logger.logException(exception);
    }

    @Override
    public Node loadNode(String path) throws IOException, CoreException {
        if (path != null && !path.equals("")) {
            IFile file = this.contentRoot.getFile(new Path(path));
            if (file.exists()) {
                return loadNode(file.getFileExtension(), file.getContents());
            }
        }
        return null;
    }

    @Override
    public Node loadNode(String extension, InputStream contents)
            throws IOException, CoreException {
        INodeType nodeType = this.nodeTypeRegistry.getNodeTypeFromExtension(extension);
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

}
