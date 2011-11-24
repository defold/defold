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
        public final String extension;
        public final ISceneView.INodeLoader loader;
        public final ISceneView.INodePresenter presenter;
        public final INodeRenderer<Node> renderer;

        public NodeImpl(String extension, ISceneView.INodeLoader loader, ISceneView.INodePresenter presenter, INodeRenderer<Node> renderer) {
            this.extension = extension;
            this.loader = loader;
            this.presenter = presenter;
            this.renderer = renderer;
        }
    }

    private Map<String, Class<?>> extToClass;
    private Map<Class<?>, NodeImpl> classToImpl;

    public Activator() {
        this.extToClass = new HashMap<String, Class<?>>();
        this.classToImpl = new HashMap<Class<?>, NodeImpl>();
    }

    // The shared instance
    private static Activator plugin;

    /*
     * (non-Javadoc)
     * @see org.osgi.framework.BundleActivator#start(org.osgi.framework.BundleContext)
     */
    @SuppressWarnings("unchecked")
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

            Class<?> nodeClass = (Class<?>) bundle.loadClass(e.getAttribute("node"));

            ISceneView.INodeLoader nodeLoader = (ISceneView.INodeLoader)e.createExecutableExtension("loader");

            ISceneView.INodePresenter nodePresenter = null;
            if (e.getAttribute("presenter") != null) {
                nodePresenter = (ISceneView.INodePresenter)e.createExecutableExtension("presenter");
            }

            INodeRenderer<Node> nodeRenderer = null;
            if (e.getAttribute("renderer") != null) {
                nodeRenderer = (INodeRenderer<Node>) e.createExecutableExtension("renderer");
            }

            registerNodeType(extension, nodeClass, nodeLoader, nodePresenter, nodeRenderer);
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

    public void registerNodeType(String extension, Class<?> c, ISceneView.INodeLoader loader, ISceneView.INodePresenter presenter, INodeRenderer<Node> renderer) {
        NodeImpl impl = new NodeImpl(extension, loader, presenter, renderer);
        this.extToClass.put(extension, c);
        this.classToImpl.put(c, impl);
    }

    @Override
    public ISceneView.INodeLoader getLoader(String extension) {
        NodeImpl impl = getImpl(extension);
        if (impl != null) {
            return impl.loader;
        }
        return null;
    }

    @Override
    public ISceneView.INodeLoader getLoader(Class<? extends Node> c) {
        NodeImpl impl = this.classToImpl.get(c);
        if (impl != null) {
            return impl.loader;
        } else {
            return null;
        }
    }

    @Override
    public ISceneView.INodePresenter getPresenter(String extension) {
        NodeImpl impl = getImpl(extension);
        if (impl != null) {
            return impl.presenter;
        }
        return null;
    }

    @Override
    public ISceneView.INodePresenter getPresenter(Class<? extends Node> c) {
        NodeImpl impl = this.classToImpl.get(c);
        if (impl != null) {
            return impl.presenter;
        } else {
            return null;
        }
    }

    @Override
    public INodeRenderer<Node> getRenderer(String extension) {
        NodeImpl impl = getImpl(extension);
        if (impl != null) {
            return impl.renderer;
        } else {
            return null;
        }
    }

    @Override
    public INodeRenderer<Node> getRenderer(Class<? extends Node> c) {
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
        Class<?> c = this.extToClass.get(extension);
        if (c != null) {
            return this.classToImpl.get(c);
        }
        return null;
    }

}
