package com.dynamo.cr.editor.handlers;
import java.util.Map;

/**
 * Bundle Linux handler
 * TODO: The BundleLinuxHandler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author chmu
 *
 */
public class BundleLinuxHandler extends BundleGenericHandler {

    @Override
    protected boolean openBundleDialog() {
        setTitle("Package Linux Application");
        return super.openBundleDialog();
    }

    private String getPlatformString() {
        if (System.getProperty("os.arch") == "x86") {
            return "x86-linux";
        }
        else
        {
            return "x86_64-linux";
        }
    }

    @Override
    protected void setProjectOptions(Map<String, String> options) {
        options.put("platform", getPlatformString());
        super.setProjectOptions(options);
    }

    @Override
    protected String getOutputPlatformDir() {
        return getPlatformString();
    }

}
