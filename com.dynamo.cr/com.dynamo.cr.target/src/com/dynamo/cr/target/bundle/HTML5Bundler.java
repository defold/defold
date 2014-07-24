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
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.codehaus.jackson.JsonFactory;
import org.codehaus.jackson.JsonGenerator;

import com.dynamo.cr.editor.core.ProjectProperties;
import com.samskivert.mustache.Mustache;
import com.samskivert.mustache.Template;

public class HTML5Bundler {
    private String contentRoot;
    private File appDir;
    private String projectHtml;
    private String projectCss;
    private String js;
    private String title;
    private String version;
    private int customHeapSize;
    private boolean useApplicationCache;
    private boolean includeDevTool;

    private List<File> monolithicFiles;
    private List<SplitFile> splitFiles;

    private static final String[] CopiedResources = {
    	"modernizr.custom.js",
    	"IndexedDBShim.min.js",
    	"combine.js",
    	"splash_image.png"
    };

    private static final String SplitFileDir = "split";
    private static final String SplitFileJson = "preload_files.json";
    private static int SplitFileSegmentSize = 2 * 1024 * 1024;
    private static final String[] SplitFileNames = {
    	"game.projectc",
    	"game.darc"
    };

    private static final String ManifestFileExtension = ".appcache";

    private static final String DevToolInlineHtmlResource = "resources/jsweb/development.inl";
    private static final String DevToolCss = "development.css";
    private static final String DevToolJs = "development.js";

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

    	void writeJson(JsonGenerator generator) throws IOException {
    		generator.writeStartObject();

    		generator.writeFieldName("name");
    		generator.writeString(source.getName());
    		generator.writeFieldName("size");
    		generator.writeNumber(source.length());

    		generator.writeFieldName("pieces");
    		generator.writeStartArray();
    		int offset = 0;
    		for (File split : this.subdivisions) {
    			File path = new File(SplitFileDir, split.getName());

    			generator.writeStartObject();
    			generator.writeFieldName("name");
    			generator.writeString(path.toString());
    			generator.writeFieldName("offset");
    			generator.writeNumber(offset);
    			generator.writeEndObject();

    			offset += split.length();
    		}
    		generator.writeEndArray();

    		generator.writeEndObject();
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

        this.projectCss = projectProperties.getStringValue("jsweb", "cssfile", null);
        if (this.projectCss != null) {
        	this.projectCss = this.projectCss.trim();
        }

        this.contentRoot = contentRoot;
        this.version = projectProperties.getStringValue("project", "version", "1.0");

        File packageDir = new File(outputDir);
        this.title = projectProperties.getStringValue("project", "title", "Unnamed");
        this.appDir = new File(packageDir, this.title);

        this.customHeapSize = -1;
        Boolean use = projectProperties.getBooleanValue("jsweb", "set_custom_heap_size");
        if (null != use && use.booleanValue()) {
        	Integer size = projectProperties.getIntValue("jsweb", "custom_heap_size");
        	if (null != size) {
        		this.customHeapSize = size.intValue();
        	}
        }

        this.useApplicationCache = false;
        use = projectProperties.getBooleanValue("jsweb", "use_app_cache");
        if (null != use) {
        	this.useApplicationCache = use.booleanValue();
        }

        this.includeDevTool = false;
        use = projectProperties.getBooleanValue("jsweb", "include_dev_tool");
        if (null != use) {
        	this.includeDevTool = use.booleanValue();
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
        createCss();

        for (String r : CopiedResources) {
        	copyResource(appDir, r);
        	monolithicFiles.add(new File(r));
        }

        createManifest();

        if (this.includeDevTool) {
        	copyResource(appDir, DevToolCss);
        	copyResource(appDir, DevToolJs);
        }
    }

    private void createHtmlShell() throws FileNotFoundException, IOException {
    	String htmlFilename = String.format("%s.html", this.title);
        File htmlOut = new File(this.appDir, htmlFilename);

    	Template infoTemplate = Mustache.compiler().compile(getHtmlText());
        Map<String, Object> infoData = new HashMap<String, Object>();

        infoData.put("DMENGINE_APP_TITLE", String.format("%s1 %s2", this.title, this.version));
        infoData.put("DMENGINE_MANIFEST", getManifestFilename());
        infoData.put("DMENGINE_SPLIT", new File(SplitFileDir, SplitFileJson).toString());
        infoData.put("DMENGINE_JS", title + ".js");
        infoData.put("DMENGINE_CSS", getCssFilename());

        String js = "";
        if (0 < this.customHeapSize) {
        	js = String.format("TOTAL_MEMORY: %d, \n", this.customHeapSize);
        }
        infoData.put("DMENGINE_STACK_SIZE", js);

        String manifest = "";
        if (this.useApplicationCache) {
        	manifest = String.format("manifest=\"%s\"", getManifestFilename());
        }
        infoData.put("DMENGINE_MANIFEST", manifest);

        String devHead = "";
        String inlineHtml = "";
        String devInit = "";

        if (this.includeDevTool) {
        	devHead = "<link rel=\"stylesheet\" type=\"text/css\" href=\"development.css\"></style>";
        	inlineHtml = getTextResource(null, DevToolInlineHtmlResource);
        	devInit = ", callback: MemoryStats.Initialise";
        }
        infoData.put("DMENGINE_DEV_HEAD", devHead);
        infoData.put("DMENGINE_DEV_INLINE", inlineHtml);
        infoData.put("DMENGINE_DEV_INIT", devInit);

        String htmlText = infoTemplate.execute(infoData);
        IOUtils.write(htmlText, new FileOutputStream(htmlOut), Charset.forName("UTF-8"));
        monolithicFiles.add(new File(htmlFilename));
    }

    private void createCss() throws FileNotFoundException, IOException {
    	String filename = getCssFilename();
    	File cssOut = new File(this.appDir, filename);
    	IOUtils.write(this.getCssText(), new FileOutputStream(cssOut), Charset.forName("UTF-8"));
    	monolithicFiles.add(new File(filename));
    }

    private String getCssFilename() {
    	return this.title + ".css";
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
    	JsonGenerator generator = null;
    	try {
    		File descFile = new File(targetDir, SplitFileJson);
    		writer = new BufferedWriter(new FileWriter(descFile));
    		generator = (new JsonFactory()).createJsonGenerator(writer);

    		generator.writeStartObject();
    		generator.writeFieldName("content");
    		generator.writeStartArray();

    		for (SplitFile split : this.splitFiles) {
    			split.writeJson(generator);
    		}

    		generator.writeEndArray();
    		generator.writeEndObject();
    	}
    	finally {
    		if (null != generator) {
    			generator.close();
    		}
    		IOUtils.closeQuietly(writer);
    	}
    }

    private String getHtmlText() throws FileNotFoundException, IOException {
    	return getTextResource(this.projectHtml, "resources/jsweb/engine_template.html");
    }

    private String getCssText() throws FileNotFoundException, IOException {
    	return getTextResource(this.projectCss, "resources/jsweb/engine_template.css");
    }

    private String getTextResource(String projectPath, String resourceDefault) throws FileNotFoundException, IOException {
    	String data = null;
    	InputStream input = null;
    	try {
    		if (projectPath != null && projectPath.length() > 0) {
    			input = new FileInputStream(new File(projectPath));
    		} else {
    			input = getClass().getResourceAsStream(resourceDefault);
    		}
    		if (input != null) {
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
