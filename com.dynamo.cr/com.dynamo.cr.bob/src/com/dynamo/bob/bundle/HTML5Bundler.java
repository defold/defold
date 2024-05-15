// Copyright 2020-2024 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.util.StringUtil;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.util.BobProjectProperties;

@BundlerParams(platforms = {Platform.JsWeb, Platform.WasmWeb})
public class HTML5Bundler implements IBundler {
    private static Logger logger = Logger.getLogger(HTML5Bundler.class.getName());

    private static final String SplitFileDir = "archive";
    private static final String SplitFileJson = "archive_files.json";
    private static int SplitFileSegmentSize = 2 * 1024 * 1024;

    // previously it was hardcoded in dmloader.js
    private long WasmSize = 2000000;
    private long WasmjsSize = 250000;
    private long AsmjsSize = 4000000;
    public static final String MANIFEST_NAME = "engine_template.html";

    @Override
    public IResource getManifestResource(Project project, Platform platform) throws IOException {
        return project.getResource("html5", "htmlfile");
    }

    @Override
    public String getMainManifestName(Platform platform) {
        return MANIFEST_NAME;
    }

    @Override
    public String getMainManifestTargetPath(Platform platform) {
        return "index.html"; // relative to the appDir
    }

    @Override
    public void updateManifestProperties(Project project, Platform platform,
                                BobProjectProperties projectProperties,
                                Map<String, Map<String, Object>> propertiesMap,
                                Map<String, Object> properties) throws IOException {


        // Same value as engine is compiled with; 268435456
        int customHeapSize = projectProperties.getIntValue("html5", "heap_size", 256) * 1024 * 1024;

        {// Deprecated method of setting the heap size. For backwards compatibility
            if (projectProperties.getBooleanValue("html5", "set_custom_heap_size", false)) {
                Integer size = projectProperties.getIntValue("html5", "custom_heap_size");
                if (null != size) {
                    customHeapSize = size.intValue();
                }
            }
        }
        properties.put("DEFOLD_HEAP_SIZE", customHeapSize);
        properties.put("DEFOLD_WASM_SIZE", WasmSize);
        properties.put("DEFOLD_WASMJS_SIZE", WasmjsSize);
        properties.put("ASMJS_SIZE", AsmjsSize);

        String splashImage = projectProperties.getStringValue("html5", "splash_image", null);
        if (splashImage != null) {
            properties.put("DEFOLD_SPLASH_IMAGE", new File(project.getRootDirectory(), splashImage).getName());
        } else {
            // Without this value we can't use Inverted Sections (^) in Mustache and receive an error:
            // "No key, method or field with name 'DEFOLD_SPLASH_IMAGE' on line N"
            properties.put("DEFOLD_SPLASH_IMAGE", false);
        }

        // Check if game has configured a Facebook App ID
        String facebookAppId = projectProperties.getStringValue("facebook", "appid", null);
        properties.put("DEFOLD_HAS_FACEBOOK_APP_ID", facebookAppId != null ? "true" : "false");

        String engineArgumentsString = projectProperties.getStringValue("html5", "engine_arguments", null);
        List<String> engineArguments = BundleHelper.createArrayFromString(engineArgumentsString);

        properties.put("DEFOLD_ARCHIVE_LOCATION_PREFIX", projectProperties.getStringValue("html5", "archive_location_prefix", "archive"));
        properties.put("DEFOLD_ARCHIVE_LOCATION_SUFFIX", projectProperties.getStringValue("html5", "archive_location_suffix", ""));
        properties.put("DEFOLD_ENGINE_ARGUMENTS", engineArguments);

        String scaleMode = StringUtil.toUpperCase(projectProperties.getStringValue("html5", "scale_mode", "downscale_fit"));
        properties.put("DEFOLD_SCALE_MODE_IS_"+scaleMode, true);

        /// Legacy properties for backwards compatibility
        {
            properties.put("DEFOLD_DISPLAY_WIDTH", projectProperties.getIntValue("display", "width"));
            properties.put("DEFOLD_DISPLAY_HEIGHT", projectProperties.getIntValue("display", "height"));

            String version = projectProperties.getStringValue("project", "version", "0.0");
            String title = projectProperties.getStringValue("project", "title", "Unnamed");

            properties.put("DEFOLD_APP_TITLE", String.format("%s %s", title, version));

            String exeName = BundleHelper.projectNameToBinaryName(title);
            properties.put("DEFOLD_BINARY_PREFIX", exeName);
        }

        // When running "Build HTML and Launch" we need to ignore the archive location prefix/suffix.
        Boolean localLaunch = project.option("local-launch", "false").equals("true");
        if (localLaunch) {
            properties.put("DEFOLD_ARCHIVE_LOCATION_PREFIX", "archive");
            properties.put("DEFOLD_ARCHIVE_LOCATION_SUFFIX", "");
            properties.put("HAS_DEFOLD_ENGINE_ARGUMENTS", "true");

            engineArguments.add("--verify-graphics-calls=false");
            properties.put("DEFOLD_ENGINE_ARGUMENTS", engineArguments);
        }

        properties.put("DEFOLD_CUSTOM_CSS_INLINE", "");
        IResource customCSS = project.getResource("html5", "cssfile");
        if (customCSS != null) {
            properties.put("DEFOLD_CUSTOM_CSS_INLINE", BundleHelper.formatResource(propertiesMap, properties, customCSS));
        }

        // set flag so that we can disable wasm support in the engine_template.html if we're not
        // bundling with a wasm engine
        final List<Platform> architectures = Platform.getArchitecturesFromString(project.option("architectures", ""), platform);
        properties.put("DEFOLD_HAS_WASM_ENGINE", architectures.contains(Platform.WasmWeb));
    }

    class SplitFile {
        private File source;
        private List<File> subdivisions;

        SplitFile(File src) {
            source = src;
            subdivisions = new ArrayList<File>();
        }

        public long getTotalSize() {
            return source.length();
        }

        private static String insertNumberBeforeExtension(String filePath, int number) {
            int dotIndex = filePath.indexOf('.');
            if (dotIndex > 0) {
                String baseName = filePath.substring(0, dotIndex);
                String extension = filePath.substring(dotIndex);
                return baseName + number + extension;
            } else {
                return filePath + number;
            }
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
                    File output = new File(destDir, insertNumberBeforeExtension(source.getName(), subdivisions.size()));
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
            long offset = 0;
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

    URL getResource(String name) {
        return getClass().getResource(String.format("resources/jsweb/%s", name));
    }

    private void createDmLoader(BundleHelper helper, URL inputResource, File targetFile) throws IOException {
        helper.formatResourceToFile(inputResource.openStream().readAllBytes(), inputResource.getPath(), targetFile);
    }
    @Override
    public void bundleApplication(Project project, Platform platform, File bundleDirectory, ICanceled canceled)
            throws IOException, CompileExceptionError {

        BundleHelper.throwIfCanceled(canceled);

        final List<Platform> architectures = Platform.getArchitecturesFromString(project.option("architectures", ""), platform);

        // Collect bundle/package resources to be included in bundle directory
        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, architectures);

        BobProjectProperties projectProperties = project.getProjectProperties();

        BundleHelper.throwIfCanceled(canceled);

        final String variant = project.option("variant", Bob.VARIANT_RELEASE);
        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String enginePrefix = BundleHelper.projectNameToBinaryName(title);

        BundleHelper.throwIfCanceled(canceled);

        File appDir = new File(bundleDirectory, title);
        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());

        FileUtils.deleteDirectory(appDir);
        File splitDir = new File(appDir, SplitFileDir);
        splitDir.mkdirs();
        createSplitFiles(buildDir, splitDir);

        BundleHelper.throwIfCanceled(canceled);
        // Copy bundle resources into bundle directory
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);


        BundleHelper.throwIfCanceled(canceled);
        // Copy debug symbols if they were generated
        String symbolsName = "dmengine.js.symbols";
        if (variant.equals(Bob.VARIANT_RELEASE)) {
            symbolsName = "dmengine_release.js.symbols";
        }
        String zipDir = FilenameUtils.concat(project.getBinaryOutputDirectory(), Platform.JsWeb.getExtenderPair());
        File bundleSymbols = new File(zipDir, symbolsName);
        if (!bundleSymbols.exists()) {
            zipDir = FilenameUtils.concat(project.getBinaryOutputDirectory(), Platform.WasmWeb.getExtenderPair());
            bundleSymbols = new File(zipDir, symbolsName);
        }
        if (bundleSymbols.exists()) {
            File symbolsOut = new File(appDir.getParentFile(), enginePrefix + ".symbols");
            FileUtils.copyFile(bundleSymbols, symbolsOut);
        }


        if (architectures.contains(Platform.JsWeb)) {
            BundleHelper.throwIfCanceled(canceled);
            Platform targetPlatform = Platform.JsWeb;
            List<File> binsAsmjs = ExtenderUtil.getNativeExtensionEngineBinaries(project, targetPlatform);
            if (binsAsmjs == null) {
                binsAsmjs = Bob.getDefaultDmengineFiles(targetPlatform, variant);
            }
            else {
                logger.info("Using extender binary for Asm.js");
            }
            // Copy engine binaries
            for (File bin : binsAsmjs) {
                BundleHelper.throwIfCanceled(canceled);
                String binExtension = FilenameUtils.getExtension(bin.getAbsolutePath());
                if (binExtension.equals("js")) {
                    FileUtils.copyFile(bin, new File(appDir, enginePrefix + "_asmjs.js"));
                    AsmjsSize = bin.length();
                } else {
                    throw new RuntimeException("Unknown extension '" + binExtension + "' of engine binary.");
                }
            }
        }

        if (architectures.contains(Platform.WasmWeb)) {
            BundleHelper.throwIfCanceled(canceled);
            Platform targetPlatform = Platform.WasmWeb;
            List<File> binsWasm = ExtenderUtil.getNativeExtensionEngineBinaries(project, targetPlatform);
            if (binsWasm == null) {
                binsWasm = Bob.getDefaultDmengineFiles(targetPlatform, variant);
            }
            else {
                logger.info("Using extender binary for WASM");
            }
            for (File bin : binsWasm) {
                BundleHelper.throwIfCanceled(canceled);
                String binExtension = FilenameUtils.getExtension(bin.getAbsolutePath());
                if (binExtension.equals("js")) {
                    FileUtils.copyFile(bin, new File(appDir, enginePrefix + "_wasm.js"));
                    WasmjsSize = bin.length();
                } else if (binExtension.equals("wasm")) {
                    FileUtils.copyFile(bin, new File(appDir, enginePrefix + ".wasm"));
                    WasmSize = bin.length();
                } else {
                    throw new RuntimeException("Unknown extension '" + binExtension + "' of engine binary.");
                }
            }
        }

        BundleHelper.throwIfCanceled(canceled);

        BundleHelper helper = new BundleHelper(project, platform, appDir, variant, this);

        helper.copyOrWriteManifestFile(architectures.get(0), appDir);

        helper.updateTemplateProperties();
        createDmLoader(helper, getResource("dmloader.js"), new File(appDir, "dmloader.js"));

        String splashImageName = projectProperties.getStringValue("html5", "splash_image");
        if (splashImageName != null && !splashImageName.isEmpty()) {
            File splashImage = new File(project.getRootDirectory(), splashImageName);
            if (splashImage.exists()) {
                FileUtils.copyFile(splashImage, new File(appDir, splashImage.getName()));
            }
        }
    }

    private void createSplitFiles(File buildDir, File targetDir) throws IOException {
        ArrayList<SplitFile> splitFiles = new ArrayList<SplitFile>();
        for (String name : BundleHelper.getArchiveFilenames(buildDir)) {
            SplitFile toSplit = new SplitFile(new File(buildDir, name));
            toSplit.performSplit(targetDir);
            splitFiles.add(toSplit);
        }
        createSplitFilesJson(splitFiles, targetDir);
    }

    private void createSplitFilesJson(ArrayList<SplitFile> splitFiles, File targetDir) throws IOException {
        BufferedWriter writer = null;
        JsonGenerator generator = null;
        long totalSize = 0;
        try {
            File descFile = new File(targetDir, SplitFileJson);
            writer = new BufferedWriter(new FileWriter(descFile));
            generator = (new JsonFactory()).createJsonGenerator(writer);

            generator.writeStartObject();
            generator.writeFieldName("content");
            generator.writeStartArray();

            for (SplitFile split : splitFiles) {
                split.writeJson(generator);
                totalSize += split.getTotalSize();
            }
            generator.writeEndArray();
            generator.writeNumberField("total_size", totalSize);
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
