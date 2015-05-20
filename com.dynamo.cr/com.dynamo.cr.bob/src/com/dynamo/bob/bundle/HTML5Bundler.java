package com.dynamo.bob.bundle;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.codehaus.jackson.JsonFactory;
import org.codehaus.jackson.JsonGenerator;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.util.BobProjectProperties;
import com.samskivert.mustache.Mustache;
import com.samskivert.mustache.Template;

public class HTML5Bundler implements IBundler {
    private static final String SplitFileDir = "split";
    private static final String SplitFileJson = "preload_files.json";
    private static int SplitFileSegmentSize = 2 * 1024 * 1024;
    private static final String[] SplitFileNames = {
        "game.projectc",
        "game.darc"
    };

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
                String path = String.format("%s/%s", SplitFileDir, split.getName());

                generator.writeStartObject();
                generator.writeFieldName("name");
                generator.writeString(path);
                generator.writeFieldName("offset");
                generator.writeNumber(offset);
                generator.writeEndObject();

                offset += split.length();
            }
            generator.writeEndArray();

            generator.writeEndObject();
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

    URL getResource(BobProjectProperties projectProperties, File projectRoot, String category, String key, String defaultValue) {
        String s = projectProperties.getStringValue(category, key);
        if (s != null && s.trim().length() > 0) {
            try {
                return new File(projectRoot, s).toURI().toURL();
            } catch (MalformedURLException e) {
                throw new RuntimeException(e);
            }
        } else {
            return getClass().getResource(String.format("resources/jsweb/%s", defaultValue));
        }
    }

    URL getResource(String name) {
        return getClass().getResource(String.format("resources/jsweb/%s", name));
    }

    String getName(URL url) {
        return FilenameUtils.getName(url.getPath());
    }

    void format(URL template, Map<String, Object> paramaters, File toFile) throws IOException {
        Template t = Mustache.compiler().compile(IOUtils.toString(template));
        String text = t.execute(paramaters);
        FileUtils.write(toFile, text, "UTF-8");
    }

    @Override
    public void bundleApplication(Project project, File bundleDirectory)
            throws IOException, CompileExceptionError {

        BobProjectProperties projectProperties = project.getProjectProperties();

        String js = Bob.getDmengineExe(Platform.JsWeb, project.hasOption("debug"));

        String jsMemInit = js + ".mem";

        File projectRoot = new File(project.getRootDirectory());
        URL html = getResource(projectProperties, projectRoot, "html5", "htmlfile", "engine_template.html");
        URL css = getResource(projectProperties, projectRoot, "html5", "cssfile", "engine_template.css");
        URL splashImage = getResource(projectProperties, projectRoot, "html5", "splash_image", "splash_image.png");
        String version = projectProperties.getStringValue("project", "version", "1.0");
        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        File appDir = new File(bundleDirectory, title);
        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());

        int customHeapSize = -1;
        if (projectProperties.getBooleanValue("html5", "set_custom_heap_size", false)) {
            Integer size = projectProperties.getIntValue("html5", "custom_heap_size");
            if (null != size) {
                customHeapSize = size.intValue();
            }
        }

        Map<String, Object> infoData = new HashMap<String, Object>();
        infoData.put("DEFOLD_DISPLAY_WIDTH", projectProperties.getIntValue("display", "width"));
        infoData.put("DEFOLD_DISPLAY_HEIGHT", projectProperties.getIntValue("display", "height"));
        infoData.put("DEFOLD_SPLASH_IMAGE", getName(splashImage));
        infoData.put("DEFOLD_SPLIT", String.format("%s/%s", SplitFileDir, SplitFileJson));
        if (0 < customHeapSize) {
            infoData.put("DEFOLD_STACK_SIZE", String.format("TOTAL_MEMORY: %d, \n", customHeapSize));
        } else {
            infoData.put("DEFOLD_STACK_SIZE", "");
        }

        infoData.put("DEFOLD_APP_TITLE", String.format("%s %s", title, version));
        infoData.put("DEFOLD_JS", String.format("%s.js", title));

        infoData.put("DEFOLD_MODULE_JS", String.format("%s_module.js", title));
        infoData.put("DEFOLD_CSS", String.format("%s.css", title));

        String devHead = "";
        String inlineHtml = "";
        String jsInit = String.format("<script type=\"text/javascript\" src=\"%s\" async", String.format("%s.js", title));

        boolean includeDevTool = projectProperties.getBooleanValue("html5", "include_dev_tool", false);
        if (includeDevTool) {
            devHead = "<link rel=\"stylesheet\" type=\"text/css\" href=\"development.css\"></style>";
            inlineHtml = IOUtils.toString(getResource("development.inl"));
            jsInit += " onload=\"MemoryStats.Initialise()\"";
        }

        jsInit += "></script>";
        infoData.put("DEFOLD_JS_INIT", jsInit);

        infoData.put("DEFOLD_DEV_HEAD", devHead);
        infoData.put("DEFOLD_DEV_INLINE", inlineHtml);

        FileUtils.deleteDirectory(appDir);
        File splitDir = new File(appDir, SplitFileDir);
        splitDir.mkdirs();
        createSplitFiles(buildDir, splitDir);

        // Copy engine
        FileUtils.copyFile(new File(js), new File(appDir, String.format("%s.js", title)));
        // Flash audio swf
        FileUtils.copyFile(new File(Bob.getLibExecPath("js-web/defold_sound.swf")), new File(appDir, "defold_sound.swf"));

        // Memory initialisation file
        File jsMemFile = new File(jsMemInit);
        if (jsMemFile.exists()) {
            FileUtils.copyFile(jsMemFile, new File(appDir, String.format("%s.js.mem", title)));
        }

        URL module = getResource("module_template.js");
        format(module, infoData, new File(appDir, String.format("%s_module.js", title)));
        format(html, infoData, new File(appDir, String.format("%s.html", title)));
        format(css, infoData, new File(appDir, String.format("%s.css", title)));

        FileUtils.copyURLToFile(getResource("combine.js"), new File(appDir, "combine.js"));
        FileUtils.copyURLToFile(splashImage, new File(appDir, getName(splashImage)));

        if (includeDevTool) {
            FileUtils.copyURLToFile(getResource("development.css"), new File(appDir, "development.css"));
            FileUtils.copyURLToFile(getResource("development.js"), new File(appDir, "development.js"));
        }
    }

    private void createSplitFiles(File buildDir, File targetDir) throws IOException {
        ArrayList<SplitFile> splitFiles = new ArrayList<SplitFile>();
        for (String name : SplitFileNames) {
            SplitFile toSplit = new SplitFile(new File(buildDir, name));
            toSplit.performSplit(targetDir);
            splitFiles.add(toSplit);
        }
        createSplitFilesJson(splitFiles, targetDir);
    }

    private void createSplitFilesJson(ArrayList<SplitFile> splitFiles, File targetDir) throws IOException {
        BufferedWriter writer = null;
        JsonGenerator generator = null;
        try {
            File descFile = new File(targetDir, SplitFileJson);
            writer = new BufferedWriter(new FileWriter(descFile));
            generator = (new JsonFactory()).createJsonGenerator(writer);

            generator.writeStartObject();
            generator.writeFieldName("content");
            generator.writeStartArray();

            for (SplitFile split : splitFiles) {
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

}
