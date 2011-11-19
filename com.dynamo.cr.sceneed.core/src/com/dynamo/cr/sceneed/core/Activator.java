package com.dynamo.cr.sceneed.core;

import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.runtime.IConfigurationElement;
import org.eclipse.core.runtime.Platform;
import org.osgi.framework.Bundle;
import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.IResourceType;

public class Activator implements BundleActivator, INodeTypeRegistry {

    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.sceneed.core"; //$NON-NLS-1$

    private static BundleContext context;

    static BundleContext getContext() {
        return context;
    }

    private static class NodeImpl {
        public String extension;
        public ISceneView.NodePresenter presenter;
        public INodeRenderer renderer;

        public NodeImpl(String extension, ISceneView.NodePresenter presenter, INodeRenderer renderer) {
            this.extension = extension;
            this.presenter = presenter;
            this.renderer = renderer;
        }
    }

    private Map<String, Class<? extends Node>> extToClass;
    private Map<Class<? extends Node>, NodeImpl> classToImpl;

    public Activator() {
        this.extToClass = new HashMap<String, Class<? extends Node>>();
        this.classToImpl = new HashMap<Class<? extends Node>, NodeImpl>();
    }

    // The shared instance
    private static Activator plugin;

    /*
     * (non-Javadoc)
     * @see org.osgi.framework.BundleActivator#start(org.osgi.framework.BundleContext)
     */
    @Override
    public void start(BundleContext bundleContext) throws Exception {
        Activator.context = bundleContext;
        plugin = this;

        IConfigurationElement[] config = Platform.getExtensionRegistry()
                .getConfigurationElementsFor("com.dynamo.cr.nodetypes");
        for (IConfigurationElement e : config) {
            Bundle bundle = Platform.getBundle(e.getDeclaringExtension().getContributor().getName());
            String resourceTypeAttribute = e.getAttribute("resource-type");
            IResourceType resourceType = EditorCorePlugin.getDefault().getResourceTypeFromId(resourceTypeAttribute);
            String extension = resourceType.getFileExtension();

            @SuppressWarnings("unchecked")
            Class<? extends Node> nodeClass = (Class<? extends Node>)bundle.loadClass(e.getAttribute("node"));
            ISceneView.NodePresenter nodePresenter = (ISceneView.NodePresenter)e.createExecutableExtension("presenter");

            INodeRenderer nodeRenderer = null;
            if (e.getAttribute("renderer") != null) {
                nodeRenderer = (INodeRenderer) e.createExecutableExtension("renderer");
            }

            registerNodeType(extension, nodeClass, nodePresenter, nodeRenderer);
        }
    }

    /*
     * (non-Javadoc)
     * @see org.osgi.framework.BundleActivator#stop(org.osgi.framework.BundleContext)
     */
    @Override
    public void stop(BundleContext bundleContext) throws Exception {
        Activator.context = null;
        plugin = null;

    }

    public void registerNodeType(String extension, Class<? extends Node> c, ISceneView.NodePresenter presenter, INodeRenderer renderer) {
        NodeImpl impl = new NodeImpl(extension, presenter, renderer);
        this.extToClass.put(extension, c);
        this.classToImpl.put(c, impl);
    }

    @Override
    public ISceneView.NodePresenter getPresenter(String extension) {
        NodeImpl impl = getImpl(extension);
        if (impl != null) {
            return impl.presenter;
        }
        return null;
    }

    @Override
    public ISceneView.NodePresenter getPresenter(Class<? extends Node> c) {
        NodeImpl impl = this.classToImpl.get(c);
        if (impl != null) {
            return impl.presenter;
        } else {
            return null;
        }
    }

    @Override
    public INodeRenderer getRenderer(String extension) {
        NodeImpl impl = getImpl(extension);
        if (impl != null) {
            return impl.renderer;
        } else {
            return null;
        }
    }

    @Override
    public INodeRenderer getRenderer(Class<? extends Node> c) {
        NodeImpl impl = this.classToImpl.get(c);
        if (impl != null) {
            return impl.renderer;
        } else {
            return null;
        }
    }

    @Override
    public String getExtension(Class<? extends Node> c) {
        NodeImpl impl = this.classToImpl.get(c);
        if (impl != null) {
            return impl.extension;
        } else {
            return null;
        }
    }

    /**
     * Returns the shared instance
     *
     * @return the shared instance
     */
    public static Activator getDefault() {
        return plugin;
    }

    private NodeImpl getImpl(String extension) {
        Class<? extends Node> c = this.extToClass.get(extension);
        if (c != null) {
            return this.classToImpl.get(c);
        }
        return null;
    }

}
