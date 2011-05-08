package com.dynamo.cr.luaeditor;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.swt.graphics.Image;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

import com.dynamo.scriptdoc.proto.ScriptDoc;
import com.dynamo.scriptdoc.proto.ScriptDoc.Document;
import com.dynamo.scriptdoc.proto.ScriptDoc.Document.Builder;

public class LuaEditorPlugin extends AbstractUIPlugin {

    public static final String PLUGIN_ID = "com.dynamo.cr.luaeditor"; //$NON-NLS-1$

    private static LuaEditorPlugin plugin;
    private Map<String, List<ScriptDoc.Function>> nameSpaceToFunctionList = new HashMap<String, List<ScriptDoc.Function>>();

    private Image luaImage;

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

            List<ScriptDoc.Function> functions = doc.getFunctionsList();
            for (ScriptDoc.Function function : functions) {
                String qualName = function.getName();
                int index = qualName.indexOf('.');
                if (index != -1) {
                    String ns = qualName.substring(0, index);
                    if (!nameSpaceToFunctionList.containsKey(ns)) {
                        nameSpaceToFunctionList.put(ns, new ArrayList<ScriptDoc.Function>());
                    }
                    List<ScriptDoc.Function> list = nameSpaceToFunctionList.get(ns);
                    list.add(function);
                }
            }

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void loadDocumentation() {
        loadDocumentation("DYNAMO_HOME/share/doc/script_doc.sdoc");
        loadDocumentation("DYNAMO_HOME/share/doc/go_doc.sdoc");
        loadDocumentation("DYNAMO_HOME/share/doc/render_doc.sdoc");
        loadDocumentation("DYNAMO_HOME/share/doc/gui_doc.sdoc");
    }

    public ScriptDoc.Function[] getDocumentation(String prefix) {

        ArrayList<ScriptDoc.Function> list = new ArrayList<ScriptDoc.Function>(64);
        int index = prefix.indexOf('.');
        if (index != -1) {
            String ns = prefix.substring(0, index);

            if (nameSpaceToFunctionList.containsKey(ns)) {
                List<ScriptDoc.Function> functions = nameSpaceToFunctionList.get(ns);
                for (ScriptDoc.Function function : functions) {
                    if (function.getName().startsWith(prefix)) {
                        list.add(function);
                    }
                }
            }
        }

        return list.toArray(new ScriptDoc.Function[list.size()]);
    }

    public Image getLuaImage() {
        return luaImage;
    }

    @Override
    public void start(BundleContext context) throws Exception {
        LuaEditorPlugin.plugin = this;
        super.start(context);
        loadDocumentation();
        ImageDescriptor desc = imageDescriptorFromPlugin(PLUGIN_ID, "icons/lua.gif");
        if (desc != null)
            this.luaImage = desc.createImage();
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
