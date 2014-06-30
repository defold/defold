package com.dynamo.cr.target.bundle;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.regex.Pattern;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.cr.editor.core.ProjectProperties;

public class HTML5Bundler {
    private String contentRoot;
    private File appDir;
    private String projectHtml;
    private String js;
    private String title;

    /**
     *
     * @param projectProperties corresponding game.project file
     * @param js path to engine javascript file
     * @param projectRoot project root
     * @param contentRoot path to *compiled* content
     * @param outputDir output directory
     */
    public HTML5Bundler(ProjectProperties projectProperties, String js, String projectRoot, String contentRoot, String outputDir) {
        this.js = js;

        this.projectHtml = projectProperties.getStringValue("project", "htmlfile", null);
        if (this.projectHtml != null) {
        	this.projectHtml = this.projectHtml.trim();
        }

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

        // Copy html (and replace placeholders)
        // TODO: More efficient way to do the placeholder replacing needed...
        Charset cs = Charset.forName("UTF-8");
        File htmlOut = new File(appDir, String.format("%s.html", title));
        String htmlText = GetHtmlText();
        htmlText = htmlText.replaceAll(Pattern.quote("${DMENGINE_JS}$"), title + ".js");
        IOUtils.write(htmlText, new FileOutputStream(htmlOut), cs);

        CopyResource(appDir, "modernizr.custom.js");
        CopyResource(appDir, "IndexedDBShim.min.js");
    }

    private String GetHtmlText() throws FileNotFoundException, IOException {
    	String data = null;
    	if (this.projectHtml != null && this.projectHtml.length() > 0) {
    		data = IOUtils.toString(new FileInputStream(new File(this.projectHtml)), Charset.forName("UTF-8"));
    	} else {
    		InputStream resource = getClass().getResourceAsStream("resources/jsweb/engine_template.html");
            data = IOUtils.toString(resource);
            resource.close();
    	}
    	return data;
    }

    private void CopyResource(File targetDir, String sourceFile) throws FileNotFoundException, IOException {
    	String sourcePath = "resources/jsweb/" + sourceFile;

    	File file = new File(this.appDir, sourceFile);
    	FileOutputStream output = new FileOutputStream(file);

    	InputStream resource = getClass().getResourceAsStream(sourcePath);
    	IOUtils.copy(resource, output);
    }
}
