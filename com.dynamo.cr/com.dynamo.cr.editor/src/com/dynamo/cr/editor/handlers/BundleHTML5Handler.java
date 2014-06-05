package com.dynamo.cr.editor.handlers;

import java.io.IOException;

import org.apache.commons.configuration.ConfigurationException;

import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.engine.Engine;
import com.dynamo.cr.target.bundle.HTML5Bundler;

/**
 * Bundle handler
 * TODO: The BundleHTML5Handler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author drlou
 *
 */
public class BundleHTML5Handler extends AbstractBundleHandler {

    @Override
    protected void bundleApp(ProjectProperties projectProperties,
            String projectRoot, String contentRoot, String outputDir) throws ConfigurationException, IOException {

        String js = Engine.getDefault().getEnginePath("html5", true);
        String html = Engine.getDefault().getHTMLPath();
        HTML5Bundler bundler = new HTML5Bundler(projectProperties, js, html, projectRoot, contentRoot, outputDir);
        bundler.bundleApplication();
    }

}
