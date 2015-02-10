package com.dynamo.cr.editor.handlers;

import java.util.Map;

/**
 * Bundle handler
 * TODO: The BundleLinuxHandler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author chmu
 *
 */
public class BundleLinuxHandler extends AbstractBundleHandler {

    @Override
    protected void setProjectOptions(Map<String, String> options) {
        options.put("platform", "x86-linux");
    }

}
