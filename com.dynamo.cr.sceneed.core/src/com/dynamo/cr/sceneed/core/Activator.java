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
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.cr.sceneed.core.ISceneView.INodePresenter;

public class Activator implements BundleActivator, INodeTypeRegistry {

    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.sceneed.core"; //$NON-NLS-1$

    private static BundleContext context;

    static BundleContext getContext() {
        return context;
    }

    private static class NodeImpl {
        public final String extension;
        public final ISceneView.INodeLoader<Node> loader;
        public final ISceneView.INodePresenter<Node> presenter;
        public final INodeRenderer<Node> renderer;
        public final IResourceType resourceType;

        public NodeImpl(String extension, ISceneView.INodeLoader<Node> loader, ISceneView.INodePresenter<Node> presenter, INodeRenderer<Node> renderer, IResourceType resourceType) {
            this.extension = extension;
            this.loader = loader;
            this.presenter = presenter;
            this.renderer = renderer;
            this.resourceType = resourceType;
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

            registerNodeType(extension, nodeClass, nodeLoader, nodePresenter, nodeRenderer, resourceType);
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

    public void registerNodeType(String extension, Class<?> c, ISceneView.INodeLoader<Node> loader, ISceneView.INodePresenter<Node> presenter, INodeRenderer<Node> renderer, IResourceType resourceType) {
        NodeImpl impl = new NodeImpl(extension, loader, presenter, renderer, resourceType);
        this.extToClass.put(extension, c);
        this.classToImpl.put(c, impl);
    }

    @Override
    public INodeLoader<Node> getLoader(String extension) {
        NodeImpl impl = getImpl(extension);
        if (impl != null) {
            return impl.loader;
        }
        return null;
    }

    @Override
    public INodeLoader<Node> getLoader(Class<? extends Node> c) {
        NodeImpl impl = this.classToImpl.get(c);
        if (impl != null) {
            return impl.loader;
        } else {
            return null;
        }
    }

    @Override
    public INodePresenter<? extends Node> getPresenter(String extension) {
        NodeImpl impl = getImpl(extension);
        if (impl != null) {
            return impl.presenter;
        }
        return null;
    }

    @Override
    public INodePresenter<? extends Node> getPresenter(Class<? extends Node> c) {
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
    public Class<?> getNodeClass(String extension) {
        return this.extToClass.get(extension);
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

    @Override
    public IResourceType getResourceType(
            Class<? extends Node> c) {
        NodeImpl impl = this.classToImpl.get(c);
        if (impl != null) {
            return impl.resourceType;
        }
        return null;
    }

    @Override
    public IResourceType getResourceType(String extension) {
        Class<?> c = this.extToClass.get(extension);
        NodeImpl impl = this.classToImpl.get(c);
        if (impl != null) {
            return impl.resourceType;
        }
        return null;
    }

}
