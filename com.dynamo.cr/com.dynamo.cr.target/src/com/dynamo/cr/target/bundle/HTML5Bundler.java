package com.dynamo.cr.target.bundle;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.List;
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

    private static final String[] CopiedResourceScripts = {
    	"modernizr.custom.js",
    	"IndexedDBShim.min.js",
    	"combine.js"
    };

    private static final String SplitFileDir = "split";
    private static final String SplitFileDescriptor = "preload_files.txt";
    private static int SplitFileSegmentSize = 2 * 1024 * 1024;
    private static final String[] SplitFileNames = {
    	"game.projectc",
    	"game.darc"
    };

    class SplitFile {
    	private File source;
    	private List<File> subdivisions;

    	public SplitFile(File src) {
    		source = src;
    		subdivisions = new ArrayList<File>();
    	}

    	public void PerformSplit(File destDir) throws IOException {
    		InputStream input = null;
    		try {
    			input = new BufferedInputStream(new FileInputStream(source));
    			long remaining = source.length();
    			while (0 < remaining) {
    				int thisRead = (int)Math.min(SplitFileSegmentSize, remaining);

    				byte[] readBuffer = new byte[thisRead];
    				long bytesRead = input.read(readBuffer, 0, thisRead);
    				assert(bytesRead == thisRead);

    				File output = new File(destDir, source.getName() + subdivisions.size());
    				WriteChunk(output, readBuffer);
    				subdivisions.add(output);

    				remaining -= thisRead;
    			}
    		}
    		finally {
    			if (null != input) {
    				input.close();
    			}
    		}
    	}

    	public void WriteDescriptor(BufferedWriter writer) throws IOException {
    		writer.write(String.format("/%1s %2d %3d\n", source.getName(), source.length(), subdivisions.size()));
    		int offset = 0;
    		for (File split : subdivisions) {
    			File path = new File(SplitFileDir, split.getName());
    			writer.write(String.format("%1s %2d\n", path.toString(), offset));
    			offset += split.length();
    		}
    	}

    	private void WriteChunk(File dest, byte[] data) throws IOException {
    		OutputStream output = null;
    		try {
    			output = new BufferedOutputStream(new FileOutputStream(dest));
    			output.write(data);
    		}
    		finally {
    			output.close();
    		}
    	}
    }

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

        File splitDir = new File(appDir, SplitFileDir);
        splitDir.mkdirs();
        CreateSplitFiles(splitDir);

        // Copy engine
        File jsOut = new File(appDir, String.format("%s.js", title));
        FileUtils.copyFile(new File(js), jsOut);

        // Copy html (and replace placeholders)
        // TODO: More efficient way to do the placeholder replacing needed...
        Charset cs = Charset.forName("UTF-8");
        File htmlOut = new File(appDir, String.format("%s.html", title));
        String htmlText = GetHtmlText();
        htmlText = htmlText.replaceAll(Pattern.quote("${DMENGINE_SPLIT}$"), new File(SplitFileDir, SplitFileDescriptor).toString());
        htmlText = htmlText.replaceAll(Pattern.quote("${DMENGINE_JS}$"), title + ".js");
        IOUtils.write(htmlText, new FileOutputStream(htmlOut), cs);

        for (String r : CopiedResourceScripts) {
        	CopyResource(appDir, r);
        }
    }

    private void CreateSplitFiles(File targetDir) throws IOException {
    	List<SplitFile> allSplits = new ArrayList<SplitFile>();
    	for (String name : SplitFileNames) {
    		SplitFile toSplit = new SplitFile(new File(contentRoot, name));
    		toSplit.PerformSplit(targetDir);
    		allSplits.add(toSplit);
    	}
    	CreateSplitFileDescriptor(targetDir, allSplits);
    }

    private void CreateSplitFileDescriptor(File targetDir, List<SplitFile> splits) throws IOException {
    	File descFile = new File(targetDir, SplitFileDescriptor);

    	BufferedWriter writer = null;
    	try {
    		writer = new BufferedWriter(new FileWriter(descFile));
    		for(SplitFile split : splits) {
    			split.WriteDescriptor(writer);
    		}
    	}
    	finally {
    		writer.close();
    	}
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
