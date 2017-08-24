package com.dynamo.cr.luaeditor;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.swt.graphics.Image;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.eclipse.ui.statushandlers.StatusManager;
import org.osgi.framework.BundleContext;

import com.dynamo.scriptdoc.proto.ScriptDoc;
import com.dynamo.scriptdoc.proto.ScriptDoc.Document;
import com.dynamo.scriptdoc.proto.ScriptDoc.Document.Builder;
import com.dynamo.scriptdoc.proto.ScriptDoc.Element;

public class LuaEditorPlugin extends AbstractUIPlugin {

    public static final String PLUGIN_ID = "com.dynamo.cr.luaeditor"; //$NON-NLS-1$

    public static final String PACKAGE_IMAGE_ID = "icons/package.png";
    public static final String FUNCTION_IMAGE_ID = "icons/function.png";
    public static final String CONSTANT_IMAGE_ID = "icons/constant.png";
    public static final String MESSAGE_IMAGE_ID = "icons/message.png";

    private static LuaEditorPlugin plugin;
    private Map<String, List<ScriptDoc.Element>> nameSpaceToElementList = new HashMap<String, List<ScriptDoc.Element>>();

    private Map<String, Image> images = new HashMap<String, Image>();
    private Map<String, ImageDescriptor> imageDescs = new HashMap<String, ImageDescriptor>();

    public LuaEditorPlugin() {
    }

    private void loadDocumentation(String path) {
        URL docURI = plugin.getBundle().getEntry(path);
        try {
            InputStream in = docURI.openStream();
            Builder b = ScriptDoc.Document.newBuilder();
            b.mergeFrom(in);
            Document doc = b.build();
            in.close();

            List<ScriptDoc.Element> elements = doc.getElementsList();
            for (ScriptDoc.Element element : elements) {
                String qualName = element.getName();
                int index = qualName.indexOf('.');
                String ns = "";
                if (index != -1) {
                    ns = qualName.substring(0, index);
                }
                if (!nameSpaceToElementList.containsKey(ns)) {
                    nameSpaceToElementList.put(ns, new ArrayList<ScriptDoc.Element>());
                }
                List<ScriptDoc.Element> list = nameSpaceToElementList.get(ns);
                list.add(element);
            }

        } catch (IOException e) {
            Status status = new Status(IStatus.ERROR, PLUGIN_ID, e.getMessage(), e);
            StatusManager.getManager().handle(status, StatusManager.LOG);
        }
    }

    private void loadDocumentation() {
        // TODO: Enable http/json docs
        // Auto complete
        String[] docs = new String[] {
                "base",
                "bit",
                "buffer",
                "builtins",
                "camera",
                "collectionfactory",
                "collectionproxy",
                "coroutine",
                "crash",
                "debug",
                "facebook",
                "factory",
                "go",
                "gui",
                "html5",
                "http",
                "iac",
                "iap",
                "image",
                "io",
                "json",
                "label",
                "math",
                "model",
                "msg",
                "os",
                "package",
                "particlefx",
                "physics",
                "profiler",
                "push",
                "render",
                "resource",
                "sound",
                "socket",
                "spine",
                "sprite",
                "string",
                "sys",
                "table",
                "tilemap",
                "vmath",
                "webview",
                "window",
                "zlib"
        };
        for (String doc : docs) {
            loadDocumentation(String.format("doc/%s_doc.sdoc", doc));
        }

        // Add all namespaces to the global namespace
        for (String ns : nameSpaceToElementList.keySet()) {
            Element e = ScriptDoc.Element.newBuilder()
                    .setType(ScriptDoc.Type.NAMESPACE)
                    .setName(ns)
                    .setBrief("")
                    .setDescription("")
                    .build();
            if (!e.getName().equals("")) {
                // NOTE: Do not add namespace "" to "", i.e. to itself
                nameSpaceToElementList.get("").add(e);
            }
        }

    }

    public ScriptDoc.Element[] getDocumentation(LuaParseResult parseResult) {

        ArrayList<ScriptDoc.Element> list = new ArrayList<ScriptDoc.Element>(64);
        String ns = parseResult.getNamespace().toLowerCase();
        String function = parseResult.getFunction().toLowerCase();
        String qualified;

        if (ns.length() == 0) {
            qualified = function;
        } else {
            qualified = String.format("%s.%s", ns, function);
        }

        if (nameSpaceToElementList.containsKey(ns)) {
            List<ScriptDoc.Element> elements = nameSpaceToElementList.get(ns);
            for (ScriptDoc.Element element : elements) {
                if (element.getName().toLowerCase().startsWith(qualified)) {
                    list.add(element);
                }
            }
        }

        ScriptDoc.Element[] ret = list.toArray(new ScriptDoc.Element[list.size()]);
        Arrays.sort(ret, new Comparator<ScriptDoc.Element>() {
            public int compare(ScriptDoc.Element o1, ScriptDoc.Element o2) {
                return o1.getName().compareTo(o2.getName());
            };
        });

        return ret;

    }

    public Image getImage(String path) {
        ImageDescriptor id = imageDescs.get(path);
        if (id != null) {
            if (!images.containsKey(path)) {
                images.put(path, id.createImage());
            }
            return images.get(path);
        }
        return null;
    }

    @Override
    public void start(BundleContext context) throws Exception {
        LuaEditorPlugin.plugin = this;
        super.start(context);
        loadDocumentation();

        for (String s : new String[] { PACKAGE_IMAGE_ID, FUNCTION_IMAGE_ID, CONSTANT_IMAGE_ID, MESSAGE_IMAGE_ID } ) {
            imageDescs.put(s, imageDescriptorFromPlugin(PLUGIN_ID, s));
        }
    }

    @Override
    public void stop(BundleContext context) throws Exception {
        LuaEditorPlugin.plugin = null;
        super.stop(context);

        for (Image i : images.values()) {
            i.dispose();
        }

    }

    public static LuaEditorPlugin getDefault() {
        return plugin;
    }

}
