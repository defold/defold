package com.dynamo.cr.editor.core;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.runtime.IConfigurationElement;
import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.Plugin;
import org.osgi.framework.Bundle;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.editor.core.internal.ResourceType;
import com.google.protobuf.GeneratedMessage;

public class EditorCorePlugin extends Plugin implements IResourceTypeRegistry {

	public static final String PLUGIN_ID = "com.dynamo.cr.editor.core";

    private String version;
    private String sha1;
    private String channel;

    private static BundleContext context;
    private static EditorCorePlugin plugin;
    private ArrayList<IResourceType> resourceTypes = new ArrayList<IResourceType>();
    private Map<String, IResourceType> extensionToResourceType = new HashMap<String, IResourceType>();
    private Map<String, IResourceType> idToResourceType = new HashMap<String, IResourceType>();

	static BundleContext getContext() {
		return context;
	}

    public static EditorCorePlugin getDefault() {
        return plugin;
    }

    public String getVersion() {
        if (version == null) {
            version = System.getProperty("defold.version", "NO VERSION");
        }
        return version;
    }

    public String getSha1() {
        if (sha1 == null) {
            sha1 = System.getProperty("defold.sha1", "NO SHA1");
        }
        return sha1;
    }

    public String getChannel() {
        if (channel == null) {
            channel = System.getProperty("defold.channel", "stable");
        }
        return channel;
    }

    public String getTitle() {
        String title = "Defold Editor";
        String channel = getChannel();
        if (!channel.equals("stable")) {
            title += " - " + channel.toUpperCase();
        }
        return title;
    }

    public static String getPlatform() {
        String os_name = System.getProperty("os.name").toLowerCase();
        String arch = System.getProperty("os.arch").toLowerCase();

        if (os_name.indexOf("win") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return "x86_64-win32";
            } else {
                return "win32";
            }
        } else if (os_name.indexOf("mac") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return "x86_64-darwin";
            } else {
                return "darwin";
            }
        } else if (os_name.indexOf("linux") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return "x86_64-linux";
            } else {
                return "linux";
            }
        }

        return null;
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

    @Override
    public IResourceType getResourceTypeFromId(String id) {
        return idToResourceType.get(id);
    }

	/*
	 * (non-Javadoc)
	 * @see org.osgi.framework.BundleActivator#start(org.osgi.framework.BundleContext)
	 */
	@Override
    @SuppressWarnings("unchecked")
    public void start(BundleContext bundleContext) throws Exception {
		EditorCorePlugin.context = bundleContext;
        plugin = this;
        System.setProperty("com.defold.platform", getPlatform());

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
            String editSupportClassName = e.getAttribute("edit-support-class");
            String type = e.getAttribute("type-class");
            String resourceRefactorParticipantClassName = e.getAttribute("resource-refactor-participant-class");

            List<String> referenceTypeClasses = new ArrayList<String>();
            List<String> referenceResourceTypeIds = new ArrayList<String>();

            IConfigurationElement[] references = e.getChildren("references");
            for (IConfigurationElement reference : references) {
                IConfigurationElement[] referenceElements = reference.getChildren();
                for (IConfigurationElement referenceElement : referenceElements) {
                    if (referenceElement.getName().equals("reference-type-class")) {
                        referenceTypeClasses.add(referenceElement.getAttribute("type-class"));
                    }
                    else if (referenceElement.getName().equals("reference-resource-type")) {
                        referenceResourceTypeIds.add(referenceElement.getAttribute("resource-type"));
                    }
                    else {
                        System.err.println("WARNING: Unknown element: " + referenceElement.getName());
                    }
                }
            }

            IResourceTypeEditSupport editSupport = null;
            if (editSupportClassName != null) {
                editSupport = (IResourceTypeEditSupport) e.createExecutableExtension("edit-support-class");
            }

            IResourceRefactorParticipant refacorParticipant = null;
            if (resourceRefactorParticipantClassName != null) {
                refacorParticipant = (IResourceRefactorParticipant) e.createExecutableExtension("resource-refactor-participant-class");
            }

            Class<GeneratedMessage> messageClass = null;
            if (protoMessageClassName != null) {
                messageClass = (Class<GeneratedMessage>) bundle.loadClass(protoMessageClassName);
            }
            IResourceType resourceType = new ResourceType(id, name, fileExtension, templateData, messageClass, embeddable != null && embeddable.equals("true"), editSupport, type, refacorParticipant, referenceTypeClasses, referenceResourceTypeIds);

            if (extensionToResourceType.containsKey(fileExtension)) {
                System.err.println(String.format("ERROR: Resource type for extension '%s' already registred (%s)", fileExtension, resourceType));
            }

            resourceTypes.add(resourceType);
            extensionToResourceType.put(fileExtension, resourceType);
            idToResourceType.put(id, resourceType);
        }
	}

	/*
	 * (non-Javadoc)
	 * @see org.osgi.framework.BundleActivator#stop(org.osgi.framework.BundleContext)
	 */
	@Override
    public void stop(BundleContext bundleContext) throws Exception {
		EditorCorePlugin.context = null;
        plugin = null;
	}
}
