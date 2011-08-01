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

public class LuaEditorPlugin extends AbstractUIPlugin {

    public static final String PLUGIN_ID = "com.dynamo.cr.luaeditor"; //$NON-NLS-1$

    private static LuaEditorPlugin plugin;
    private Map<String, List<ScriptDoc.Element>> nameSpaceToElementList = new HashMap<String, List<ScriptDoc.Element>>();

    private Image luaImage;

    private ImageDescriptor luaImageDesc;

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
                if (index != -1) {
                    String ns = qualName.substring(0, index);
                    if (!nameSpaceToElementList.containsKey(ns)) {
                        nameSpaceToElementList.put(ns, new ArrayList<ScriptDoc.Element>());
                    }
                    List<ScriptDoc.Element> list = nameSpaceToElementList.get(ns);
                    list.add(element);
                }
            }

        } catch (IOException e) {
            Status status = new Status(IStatus.ERROR, PLUGIN_ID, e.getMessage(), e);
            StatusManager.getManager().handle(status, StatusManager.LOG);
        }
    }

    private void loadDocumentation() {
        loadDocumentation("DYNAMO_HOME/share/doc/script_doc.sdoc");
        loadDocumentation("DYNAMO_HOME/share/doc/go_doc.sdoc");
        loadDocumentation("DYNAMO_HOME/share/doc/render_doc.sdoc");
        loadDocumentation("DYNAMO_HOME/share/doc/gui_doc.sdoc");
    }

    public ScriptDoc.Element[] getDocumentation(LuaParseResult parseResult) {

        ArrayList<ScriptDoc.Element> list = new ArrayList<ScriptDoc.Element>(64);
        String ns = parseResult.getNamespace().toLowerCase();
        String function = parseResult.getFunction().toLowerCase();
        String qualified = String.format("%s.%s", ns, function);

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

    public Image getLuaImage() {
        if (luaImage == null && luaImageDesc != null)
            this.luaImage = luaImageDesc.createImage();

        return luaImage;
    }

    @Override
    public void start(BundleContext context) throws Exception {
        LuaEditorPlugin.plugin = this;
        super.start(context);
        loadDocumentation();
        luaImageDesc = imageDescriptorFromPlugin(PLUGIN_ID, "icons/lua.gif");
    }

    @Override
    public void stop(BundleContext context) throws Exception {
        LuaEditorPlugin.plugin = null;
        super.stop(context);
    }

    public static LuaEditorPlugin getDefault() {
        return plugin;
    }

}
