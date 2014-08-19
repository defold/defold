package com.dynamo.cr.sceneed.core.internal;

import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.runtime.IConfigurationElement;
import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.Plugin;
import org.osgi.framework.Bundle;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;

public class NodeTypeRegistry implements INodeTypeRegistry {

    private final Map<String, Class<?>> extToClass;
    private final Map<Class<?>, INodeType> classToType;
    private final Map<String, INodeType> idToNodeType;
    private static Logger logger = LoggerFactory.getLogger(NodeTypeRegistry.class);

    public NodeTypeRegistry() {
        this.extToClass = new HashMap<String, Class<?>>();
        this.classToType = new HashMap<Class<?>, INodeType>();
        this.idToNodeType = new HashMap<String, INodeType>();
    }

    @SuppressWarnings("unchecked")
    public void init(Plugin plugin) {
        IConfigurationElement[] config = Platform.getExtensionRegistry()
                .getConfigurationElementsFor("com.dynamo.cr.nodetypes");
        try {
            for (IConfigurationElement e : config) {
                Bundle bundle = Platform.getBundle(e.getDeclaringExtension().getContributor().getName());
                String id = e.getAttribute("id");
                String resourceTypeAttribute = e.getAttribute("resource-type");

                IResourceType resourceType = null;
                String extension = null;

                if (resourceTypeAttribute != null) {
                    resourceType = EditorCorePlugin.getDefault().getResourceTypeFromId(resourceTypeAttribute);
                    extension = resourceType.getFileExtension();
                }

                Class<?> nodeClass = bundle.loadClass(e.getAttribute("node"));

                INodeLoader<Node> nodeLoader = null;
                if (e.getAttribute("loader") != null) {
                    nodeLoader = (INodeLoader<Node>) e.createExecutableExtension("loader");
                }

                ISceneView.INodePresenter<Node> nodePresenter = null;
                if (e.getAttribute("presenter") != null) {
                    nodePresenter = (ISceneView.INodePresenter<Node>)e.createExecutableExtension("presenter");
                }

                String displayGroup = e.getAttribute("display-group");
                boolean cached = Boolean.parseBoolean(e.getAttribute("cached"));
                NodeType type = new NodeType(extension, e, nodeLoader, nodePresenter, resourceType, nodeClass, displayGroup, cached);
                if (extension != null) {
                    this.extToClass.put(extension, nodeClass);
                }
                this.classToType.put(nodeClass, type);

                this.idToNodeType.put(id, type);
            }
            // resolve child node types
            for (IConfigurationElement e : config) {
                String id = e.getAttribute("id");
                NodeType type = (NodeType)this.idToNodeType.get(id);
                for (IConfigurationElement element : e.getChildren()) {
                    String refId = element.getAttribute("node-type");
                    type.addReferenceNodeType(this.idToNodeType.get(refId));
                }
            }
        } catch (Exception exception) {
            logger.error("Failed to initialize node types", exception);
        }
    }

    @Override
    public INodeType[] getNodeTypes() {
        INodeType[] nodeTypes = new INodeType[this.classToType.size()];
        return this.classToType.values().toArray(nodeTypes);
    }

    @Override
    public INodeType getNodeTypeFromExtension(String extension) {
        Class<?> nodeClass = this.extToClass.get(extension);
        if (nodeClass != null) {
            return this.classToType.get(nodeClass);
        }
        return null;
    }

    @Override
    public INodeType getNodeTypeFromID(String id) {
        return idToNodeType.get(id);
    }

    @Override
    public INodeType getNodeTypeClass(Class<? extends Node> nodeClass) {
        return this.classToType.get(nodeClass);
    }

}
