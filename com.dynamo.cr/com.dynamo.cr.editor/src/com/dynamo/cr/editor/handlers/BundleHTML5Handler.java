package com.dynamo.cr.editor.handlers;

import java.util.Map;

/**
 * Bundle handler
 * TODO: The BundleHTML5Handler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author drlou
 *
 */
public class BundleHTML5Handler extends AbstractBundleHandler {

    @Override
    protected void setProjectOptions(Map<String, String> options) {
        options.put("platform", "js-web");
    }

}
