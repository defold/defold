package com.dynamo.cr.editor.handlers;

import java.io.IOException;

import org.apache.commons.configuration.ConfigurationException;

import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.engine.Engine;
import com.dynamo.cr.target.bundle.OSXBundler;

/**
 * Bundle iOS handler
 * TODO: The BundleOSXHandler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author chmu
 *
 */
public class BundleOSXHandler extends AbstractBundleHandler {

    @Override
    protected void bundleApp(ProjectProperties projectProperties,
            String projectRoot, String contentRoot, String outputDir)
            throws ConfigurationException, IOException {

        String exe = Engine.getDefault().getEnginePath("darwin", true);
        OSXBundler bundler = new OSXBundler(projectProperties, exe, projectRoot, contentRoot, outputDir);
        bundler.bundleApplication();
    }
}
