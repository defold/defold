package com.dynamo.cr.editor.handlers;

import java.io.IOException;

import org.apache.commons.configuration.ConfigurationException;

import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.engine.Engine;
import com.dynamo.cr.target.bundle.LinuxBundler;

/**
 * Bundle handler
 * TODO: The BundleLinuxHandler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author chmu
 *
 */
public class BundleLinuxHandler extends AbstractBundleHandler {

    @Override
    protected void bundleApp(ProjectProperties projectProperties,
            String projectRoot, String contentRoot, String outputDir) throws ConfigurationException, IOException {

        String exe = Engine.getDefault().getEnginePath("linux", true);
        LinuxBundler bundler = new LinuxBundler(projectProperties, exe, projectRoot, contentRoot, outputDir);
        bundler.bundleApplication();
    }

}
