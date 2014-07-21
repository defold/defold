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
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.ArrayList;
import java.util.Calendar;
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
    private String version;
    private int customHeapSize;

    private List<File> monolithicFiles;
    private List<SplitFile> splitFiles;

    private static final String[] CopiedResourceScripts = {
    	"modernizr.custom.js",
    	"IndexedDBShim.min.js",
    	"combine.js"
    };

    private static final String SplitFileDir = "split";
    private static final String SplitFileJson = "preload_files.json";
    private static int SplitFileSegmentSize = 2 * 1024 * 1024;
    private static final String[] SplitFileNames = {
    	"game.projectc",
    	"game.darc"
    };

    private static final String ManifestFileExtension = ".appcache";

    class SplitFile {
    	private File source;
    	private List<File> subdivisions;

    	SplitFile(File src) {
    		source = src;
    		subdivisions = new ArrayList<File>();
    	}

    	void performSplit(File destDir) throws IOException {
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
    				writeChunk(output, readBuffer);
    				subdivisions.add(output);

    				remaining -= thisRead;
    			}
    		}
    		finally {
    			IOUtils.closeQuietly(input);
    		}
    	}

    	void writeJson(BufferedWriter writer) throws IOException {
    		int offset = 0;
    		writer.write("\t{\n");
    			writer.write(String.format("\t\t\"name\": \"%1s\",\n", source.getName()));
    			writer.write(String.format("\t\t\"size\": %1d,\n", source.length()));
    			writer.write("\t\t\"pieces\": [\n");
    			for (int i=0; i<subdivisions.size(); ++i) {
    				File split = subdivisions.get(i);
    				File path = new File(SplitFileDir, split.getName());
    				writer.write(String.format("\t\t{ \"name\": \"%1s\", \"offset\": %2d }", path.toString(), offset));
    				if (i < subdivisions.size() -1) {
    					writer.write(",\n");
    				} else {
    					writer.write("\n");
    				}
    				offset += split.length();
    			}
    			writer.write("\t\t]\n");
    		writer.write("\t}");
    	}

    	void writeManifestContribution(BufferedWriter writer) throws IOException {
    		for (File s : subdivisions) {
    			File relative = new File(SplitFileDir, s.getName());
    			writer.write(String.format("%s\n", relative.getPath()));
    		}
    	}

    	void writeChunk(File dest, byte[] data) throws IOException {
    		OutputStream output = null;
    		try {
    			output = new BufferedOutputStream(new FileOutputStream(dest));
    			output.write(data);
    		}
    		finally {
    			IOUtils.closeQuietly(output);
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

        this.projectHtml = projectProperties.getStringValue("jsweb", "htmlfile", null);
        if (this.projectHtml != null) {
        	this.projectHtml = this.projectHtml.trim();
        }

        this.contentRoot = contentRoot;
        this.version = projectProperties.getStringValue("project", "version", "1.0");

        File packageDir = new File(outputDir);
        this.title = projectProperties.getStringValue("project", "title", "Unnamed");
        this.appDir = new File(packageDir, title);

        this.customHeapSize = -1;
        Boolean use = projectProperties.getBooleanValue("jsweb", "set_custom_heap_size");
        if (null != use && use.booleanValue()) {
        	Integer size = projectProperties.getIntValue("jsweb", "custom_heap_size");
        	if (null != size) {
        		this.customHeapSize = size.intValue();
        	}
        }
    }

    public void bundleApplication() throws IOException, ConfigurationException {
    	monolithicFiles = new ArrayList<File>();

        FileUtils.deleteDirectory(appDir);

        File splitDir = new File(appDir, SplitFileDir);
        splitDir.mkdirs();
        createSplitFiles(splitDir);

        // Copy engine
        String jsFilename = String.format("%s.js", title);
        File jsOut = new File(appDir, jsFilename);
        FileUtils.copyFile(new File(js), jsOut);
        monolithicFiles.add(new File(jsFilename));

        createHtmlShell();

        for (String r : CopiedResourceScripts) {
        	copyResource(appDir, r);
        	monolithicFiles.add(new File(r));
        }

        createManifest();
    }

    private void createHtmlShell() throws FileNotFoundException, IOException {
    	// Copy html (and replace placeholders)
        // TODO: More efficient way to do the placeholder replacing needed...
        Charset cs = Charset.forName("UTF-8");
        String htmlFilename = String.format("%s.html", title);
        File htmlOut = new File(appDir, htmlFilename);
        String htmlText = getHtmlText();
        htmlText = htmlText.replaceAll(Pattern.quote("${DMENGINE_APP_TITLE}$"), String.format("%s1 %s2", this.title, this.version));
        htmlText = htmlText.replaceAll(Pattern.quote("${DMENGINE_MANIFEST}$"), getManifestFilename());
        htmlText = htmlText.replaceAll(Pattern.quote("${DMENGINE_SPLIT}$"), new File(SplitFileDir, SplitFileJson).toString());
        htmlText = htmlText.replaceAll(Pattern.quote("${DMENGINE_JS}$"), title + ".js");

        String pattern = Pattern.quote("${DMENGINE_STACK_SIZE}$");
        String js = "";
        if (0 < this.customHeapSize) {
        	js = String.format("TOTAL_MEMORY: %d1, \n", this.customHeapSize);
        }
        htmlText = htmlText.replaceAll(pattern, js);

        IOUtils.write(htmlText, new FileOutputStream(htmlOut), cs);
        monolithicFiles.add(new File(htmlFilename));
    }

    private void createSplitFiles(File targetDir) throws IOException {
    	splitFiles = new ArrayList<SplitFile>();
    	for (String name : SplitFileNames) {
    		SplitFile toSplit = new SplitFile(new File(contentRoot, name));
    		toSplit.performSplit(targetDir);
    		splitFiles.add(toSplit);
    	}
    	createSplitFilesJson(targetDir);
    }

    private void createSplitFilesJson(File targetDir) throws IOException {
    	BufferedWriter writer = null;
    	try {
    		File descFile = new File(targetDir, SplitFileJson);
    		writer = new BufferedWriter(new FileWriter(descFile));
    		writer.write("{ \"content\": [\n");
    		for (int i=0; i<splitFiles.size(); ++i) {
    			splitFiles.get(i).writeJson(writer);
    			if (i < splitFiles.size() - 1) {
    				writer.write(",\n");
    			} else {
    				writer.write("\n");
    			}
    		}
    		writer.write("]}");
    	}
    	finally {
    		IOUtils.closeQuietly(writer);
    	}
    }

    private String getHtmlText() throws FileNotFoundException, IOException {
    	String data = null;
    	InputStream input = null;
    	try {
	    	if (this.projectHtml != null && this.projectHtml.length() > 0) {
	    		input = new FileInputStream(new File(this.projectHtml));//, Charset.forName("UTF-8");
	    	} else {
	    		input = getClass().getResourceAsStream("resources/jsweb/engine_template.html");
	    	}
	    	if (null != input) {
	    		data = IOUtils.toString(input);
	    	}
    	}
    	finally {
    		IOUtils.closeQuietly(input);
    	}
    	return data;
    }

    private void copyResource(File targetDir, String sourceFile) throws FileNotFoundException, IOException {
    	OutputStream output = null;
    	InputStream resource = null;
    	try {
    		String sourcePath = "resources/jsweb/" + sourceFile;
	    	File file = new File(this.appDir, sourceFile);
	    	output = new FileOutputStream(file);

	    	resource = getClass().getResourceAsStream(sourcePath);
	    	IOUtils.copy(resource, output);
    	}
    	finally {
    		IOUtils.closeQuietly(output);
    		IOUtils.closeQuietly(resource);
    	}
    }

    private String getManifestFilename() {
    	return this.title + ManifestFileExtension;
    }

    private void createManifest() throws IOException {
    	File manifestFile = new File(this.appDir, getManifestFilename());

    	BufferedWriter writer = null;
    	try {
    		writer = new BufferedWriter(new FileWriter(manifestFile));

    		writer.write("CACHE MANIFEST\n");
    		Date now = Calendar.getInstance().getTime();
    		DateFormat dateFormat = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss");
    		writer.write(String.format("# %1s %2s\n", this.version, dateFormat.format(now)));

    		writer.write("\n");

    		writer.write("CACHE: \n");
    		for (File m : this.monolithicFiles) {
    			writer.write(String.format("%s\n", m.getPath()));
    		}
    		for (SplitFile s : this.splitFiles) {
    			s.writeManifestContribution(writer);
    		}

    		writer.write("\n");

    		writer.write("NETWORK:\n");
    		writer.write("*\n");
    	}
    	finally {
    		IOUtils.closeQuietly(writer);
    	}
    }
}
