package com.dynamo.cr.sceneed.core.internal;

import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.runtime.IConfigurationElement;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.Plugin;
import org.eclipse.core.runtime.Status;
import org.osgi.framework.Bundle;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.sceneed.core.Activator;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;

public class NodeTypeRegistry implements INodeTypeRegistry {

    private final Map<String, Class<?>> extToClass;
    private final Map<Class<?>, INodeType> classToType;

    public NodeTypeRegistry() {
        this.extToClass = new HashMap<String, Class<?>>();
        this.classToType = new HashMap<Class<?>, INodeType>();
    }

    @SuppressWarnings("unchecked")
    public void init(Plugin plugin) {
        IConfigurationElement[] config = Platform.getExtensionRegistry()
                .getConfigurationElementsFor("com.dynamo.cr.nodetypes");
        try {
            for (IConfigurationElement e : config) {
                Bundle bundle = Platform.getBundle(e.getDeclaringExtension().getContributor().getName());
                String resourceTypeAttribute = e.getAttribute("resource-type");
                IResourceType resourceType = EditorCorePlugin.getDefault().getResourceTypeFromId(resourceTypeAttribute);
                String extension = resourceType.getFileExtension();

                Class<?> nodeClass = bundle.loadClass(e.getAttribute("node"));

                ISceneView.INodeLoader<Node> nodeLoader = (ISceneView.INodeLoader<Node>)e.createExecutableExtension("loader");

                ISceneView.INodePresenter<Node> nodePresenter = null;
                if (e.getAttribute("presenter") != null) {
                    nodePresenter = (ISceneView.INodePresenter<Node>)e.createExecutableExtension("presenter");
                }

                INodeRenderer<Node> nodeRenderer = null;
                if (e.getAttribute("renderer") != null) {
                    nodeRenderer = (INodeRenderer<Node>) e.createExecutableExtension("renderer");
                }

                NodeType type = new NodeType(extension, nodeLoader, nodePresenter, nodeRenderer, resourceType, nodeClass);
                this.extToClass.put(extension, nodeClass);
                this.classToType.put(nodeClass, type);
            }
        } catch (Exception exception) {
            Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, exception.getMessage(), exception);
            plugin.getLog().log(status);
        }
    }

    @Override
    public INodeType[] getNodeTypes() {
        INodeType[] nodeTypes = new INodeType[this.classToType.size()];
        return this.classToType.values().toArray(nodeTypes);
    }

    @Override
    public INodeType getNodeType(String extension) {
        Class<?> nodeClass = this.extToClass.get(extension);
        if (nodeClass != null) {
            return this.classToType.get(nodeClass);
        }
        return null;
    }

    @Override
    public INodeType getNodeType(Class<? extends Node> nodeClass) {
        return this.classToType.get(nodeClass);
    }

}
