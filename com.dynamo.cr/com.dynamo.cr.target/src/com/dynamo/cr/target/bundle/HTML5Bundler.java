package com.dynamo.cr.target.bundle;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.io.FileUtils;

import com.dynamo.cr.editor.core.ProjectProperties;

public class HTML5Bundler {
    private String contentRoot;
    private File appDir;
    private String html;
    private String js;
    private String title;

    /**
     *
     * @param projectProperties corresponding game.project file
     * @param js path to engine javascript file
     * @param templateHtml path to engine template html file
     * @param projectRoot project root
     * @param contentRoot path to *compiled* content
     * @param outputDir output directory
     */
    public HTML5Bundler(ProjectProperties projectProperties, String js, String templateHtml, String projectRoot, String contentRoot, String outputDir) {
        this.js = js;
        String projectHtml = projectProperties.getStringValue("project", "htmlfile", "").trim();
        this.html = (projectHtml != null && projectHtml.length() > 0 ? projectHtml : templateHtml);        	
        this.contentRoot = contentRoot;

        File packageDir = new File(outputDir);
        this.title = projectProperties.getStringValue("project", "title", "Unnamed");
        this.appDir = new File(packageDir, title);
    }

    public void bundleApplication() throws IOException, ConfigurationException {
        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        // Copy archive and game.projectc
        for (String name : Arrays.asList("game.projectc", "game.darc")) {
            FileUtils.copyFile(new File(contentRoot, name), new File(appDir, name));
        }

        // Copy engine
        File jsOut = new File(appDir, String.format("%s.js", title));
        FileUtils.copyFile(new File(js), jsOut);        

        // Copy html
        File htmlOut = new File(appDir, String.format("%s.html", title));
        FileUtils.copyFile(new File(html), htmlOut);        
    }
}
