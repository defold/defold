package com.dynamo.cr.editor.core;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.runtime.IConfigurationElement;
import org.eclipse.core.runtime.Platform;
import org.osgi.framework.Bundle;
import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.editor.core.internal.ResourceType;
import com.google.protobuf.GeneratedMessage;

public class EditorCorePlugin implements BundleActivator, IResourceTypeRegistry {

	private static BundleContext context;
    private static EditorCorePlugin plugin;
    private ArrayList<IResourceType> resourceTypes = new ArrayList<IResourceType>();
    private Map<String, IResourceType> extensionToResourceType = new HashMap<String, IResourceType>();

	static BundleContext getContext() {
		return context;
	}

    public static EditorCorePlugin getDefault() {
        return plugin;
    }

    public IResourceTypeRegistry getResourceTypeRegistry() {
        return this;
    }

    @Override
    public IResourceType[] getResourceTypes() {
        IResourceType[] ret = new ResourceType[this.resourceTypes.size()];
        return this.resourceTypes.toArray(ret);
    }

    @Override
    public IResourceType getResourceTypeFromExtension(String type) {
        return this.extensionToResourceType.get(type);
    }

	/*
	 * (non-Javadoc)
	 * @see org.osgi.framework.BundleActivator#start(org.osgi.framework.BundleContext)
	 */
	@SuppressWarnings("unchecked")
    public void start(BundleContext bundleContext) throws Exception {
		EditorCorePlugin.context = bundleContext;
        plugin = this;

        IConfigurationElement[] config = Platform.getExtensionRegistry()
        .getConfigurationElementsFor("com.dynamo.cr.resourcetypes");
        for (IConfigurationElement e : config) {
            Bundle bundle = Platform.getBundle(e.getDeclaringExtension().getContributor().getName());

            String id = e.getAttribute("id");
            String name = e.getAttribute("name");
            String fileExtension = e.getAttribute("file-extension");
            String templateData = e.getAttribute("template-data");
            String protoMessageClassName = e.getAttribute("proto-message-class");
            String embeddable = e.getAttribute("embeddable");


            Class<GeneratedMessage> messageClass = null;
            if (protoMessageClassName != null) {
                messageClass = bundle.loadClass(protoMessageClassName);
            }
            IResourceType resourceType = new ResourceType(id, name, fileExtension, templateData, messageClass, embeddable != null && embeddable.equals("true"));
            resourceTypes.add(resourceType);
            extensionToResourceType.put(fileExtension, resourceType);
        }
	}

	/*
	 * (non-Javadoc)
	 * @see org.osgi.framework.BundleActivator#stop(org.osgi.framework.BundleContext)
	 */
	public void stop(BundleContext bundleContext) throws Exception {
		EditorCorePlugin.context = null;
        plugin = null;
	}

}
