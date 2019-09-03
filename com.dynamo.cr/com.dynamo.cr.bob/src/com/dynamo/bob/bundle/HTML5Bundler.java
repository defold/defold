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
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.codehaus.jackson.JsonFactory;
import org.codehaus.jackson.JsonGenerator;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.util.BobProjectProperties;
import com.samskivert.mustache.Mustache;
import com.samskivert.mustache.Template;

public class HTML5Bundler implements IBundler {
    private static Logger logger = Logger.getLogger(HTML5Bundler.class.getName());

    private static final String SplitFileDir = "archive";
    private static final String SplitFileJson = "archive_files.json";
    private static int SplitFileSegmentSize = 2 * 1024 * 1024;
    private static final String[] SplitFileNames = {
        "game.projectc",
        "game.arci",
        "game.arcd",
        "game.dmanifest",
        "game.public.der"
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
                String path = split.getName();

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
            if (defaultValue != null) {
                return getClass().getResource(String.format("resources/jsweb/%s", defaultValue));
            }
            return null;
        }
    }

    URL getResource(String name) {
        return getClass().getResource(String.format("resources/jsweb/%s", name));
    }

    String getName(URL url) {
        return FilenameUtils.getName(url.getPath());
    }

    @Override
    public void bundleApplication(Project project, File bundleDirectory, ICanceled canceled)
            throws IOException, CompileExceptionError {

        BundleHelper.throwIfCanceled(canceled);
        // Collect bundle/package resources to be included in bundle directory
        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, Platform.JsWeb);

        BobProjectProperties projectProperties = project.getProjectProperties();

        BundleHelper.throwIfCanceled(canceled);
        Boolean localLaunch = project.option("local-launch", "false").equals("true");
        final String variant = project.option("variant", Bob.VARIANT_RELEASE);
        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String enginePrefix = BundleHelper.projectNameToBinaryName(title);
        String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");

        List<File> binsAsmjs = null;
        List<File> binsWasm = null;

        BundleHelper.throwIfCanceled(canceled);
        // asmjs binaries
        {
            Platform targetPlatform = Platform.JsWeb;
            binsAsmjs = Bob.getNativeExtensionEngineBinaries(targetPlatform, extenderExeDir);
            if (binsAsmjs == null) {
                binsAsmjs = Bob.getDefaultDmengineFiles(targetPlatform, variant);
            }
            else {
                logger.log(Level.INFO, "Using extender binary for Asm.js");
            }
            ;
        }

        BundleHelper.throwIfCanceled(canceled);
        // wasm binaries
        {
            Platform targetPlatform = Platform.WasmWeb;
            binsWasm = Bob.getNativeExtensionEngineBinaries(targetPlatform, extenderExeDir);
            if (binsWasm == null) {
                binsWasm = Bob.getDefaultDmengineFiles(targetPlatform, variant);
            }
            else {
                logger.log(Level.INFO, "Using extender binary for WASM");
            }
        }

        BundleHelper.throwIfCanceled(canceled);
        File projectRoot = new File(project.getRootDirectory());

        URL splashImage = getResource(projectProperties, projectRoot, "html5", "splash_image", null);
        File appDir = new File(bundleDirectory, title);
        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());

        // Same value as engine is compiled with; 268435456
        int customHeapSize = projectProperties.getIntValue("html5", "heap_size", 256) * 1024 * 1024;
        
        {// Deprecated method of setting the heap sie. For backwards compatibility
            if (projectProperties.getBooleanValue("html5", "set_custom_heap_size", false)) {
                Integer size = projectProperties.getIntValue("html5", "custom_heap_size");
                if (null != size) {
                    customHeapSize = size.intValue();
                }
            }
        }

        BundleHelper.throwIfCanceled(canceled);
        Map<String, Object> infoData = new HashMap<String, Object>();
        infoData.put("exe-name", enginePrefix);

        if (splashImage != null) {
            infoData.put("DEFOLD_SPLASH_IMAGE", getName(splashImage));
        } else {
            // Without this value we can't use Inverted Sections (^) in Mustache and recive an error:
            // "No key, method or field with name 'DEFOLD_SPLASH_IMAGE' on line N"
            infoData.put("DEFOLD_SPLASH_IMAGE", false);
        }
        infoData.put("DEFOLD_HEAP_SIZE", customHeapSize);

        // Check if game has configured a Facebook App ID
        String facebookAppId = projectProperties.getStringValue("facebook", "appid", null);
        infoData.put("DEFOLD_HAS_FACEBOOK_APP_ID", facebookAppId != null ? "true" : "false");

        String engineArgumentsString = projectProperties.getStringValue("html5", "engine_arguments", null);
        List<String> engineArguments = engineArgumentsString != null ? new ArrayList<String>(Arrays.asList(engineArgumentsString.split(","))) : new ArrayList<String>();

        // When running "Build HTML and Launch" we need to ignore the archive location prefix/suffix.
        if (localLaunch) {
            infoData.put("DEFOLD_ARCHIVE_LOCATION_PREFIX", "archive");
            infoData.put("DEFOLD_ARCHIVE_LOCATION_SUFFIX", "");
            infoData.put("HAS_DEFOLD_ENGINE_ARGUMENTS", "true");
            engineArguments.add("--verify-graphics-calls=false");
        } else {
            infoData.put("DEFOLD_ARCHIVE_LOCATION_PREFIX", projectProperties.getStringValue("html5", "archive_location_prefix", "archive"));
            infoData.put("DEFOLD_ARCHIVE_LOCATION_SUFFIX", projectProperties.getStringValue("html5", "archive_location_suffix", ""));
        }
        infoData.put("DEFOLD_ENGINE_ARGUMENTS", engineArguments);

        BundleHelper.throwIfCanceled(canceled);

        String scaleMode = projectProperties.getStringValue("html5", "scale_mode", "downscale_fit").toUpperCase();
        infoData.put("DEFOLD_SCALE_MODE_IS_"+scaleMode, true);

        /// Legacy properties for backwards compatibility
        {
            infoData.put("DEFOLD_DISPLAY_WIDTH", projectProperties.getIntValue("display", "width"));
            infoData.put("DEFOLD_DISPLAY_HEIGHT", projectProperties.getIntValue("display", "height"));

            String version = projectProperties.getStringValue("project", "version", "0.0");
            infoData.put("DEFOLD_APP_TITLE", String.format("%s %s", title, version));

            infoData.put("DEFOLD_BINARY_PREFIX", enginePrefix); // replaced by "exe-name"
        }

        BundleHelper.throwIfCanceled(canceled);
        FileUtils.deleteDirectory(appDir);
        File splitDir = new File(appDir, SplitFileDir);
        splitDir.mkdirs();
        createSplitFiles(buildDir, splitDir);

        BundleHelper.throwIfCanceled(canceled);
        // Copy bundle resources into bundle directory
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);

        // Copy engine binaries
        for (File bin : binsAsmjs) {
            BundleHelper.throwIfCanceled(canceled);
            String binExtension = FilenameUtils.getExtension(bin.getAbsolutePath());
            if (binExtension.equals("js")) {
                FileUtils.copyFile(bin, new File(appDir, enginePrefix + "_asmjs.js"));
            } else {
                throw new RuntimeException("Unknown extension '" + binExtension + "' of engine binary.");
            }
        }

        for (File bin : binsWasm) {
            BundleHelper.throwIfCanceled(canceled);
            String binExtension = FilenameUtils.getExtension(bin.getAbsolutePath());
            if (binExtension.equals("js")) {
                FileUtils.copyFile(bin, new File(appDir, enginePrefix + "_wasm.js"));
            } else if (binExtension.equals("wasm")) {
                FileUtils.copyFile(bin, new File(appDir, enginePrefix + ".wasm"));
            } else {
                throw new RuntimeException("Unknown extension '" + binExtension + "' of engine binary.");
            }
        }

        BundleHelper.throwIfCanceled(canceled);
        // Flash audio swf
        FileUtils.copyFile(new File(Bob.getLibExecPath("js-web/defold_sound.swf")), new File(appDir, "defold_sound.swf"));

        BundleHelper helper = new BundleHelper(project, Platform.JsWeb, appDir, "", variant);

        IResource customCSS = helper.getResource("html5", "cssfile");
        infoData.put("DEFOLD_CUSTOM_CSS_INLINE", helper.formatResource(infoData, customCSS));

        File manifestFile = new File(appDir, "index.html");
        IResource sourceManifestFile = helper.getResource("html5", "htmlfile");
        helper.mergeManifests(infoData, sourceManifestFile, manifestFile);


        FileUtils.copyURLToFile(getResource("dmloader.js"), new File(appDir, "dmloader.js"));
        if (splashImage != null) {
            FileUtils.copyURLToFile(splashImage, new File(appDir, getName(splashImage)));
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
