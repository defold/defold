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

    URL getResource(String name) {
        return getClass().getResource(String.format("resources/jsweb/%s", name));
    }

    @Override
    public void bundleApplication(Project project, File bundleDirectory, ICanceled canceled)
            throws IOException, CompileExceptionError {

        BundleHelper.throwIfCanceled(canceled);

        final Platform platform = Platform.JsWeb;
        final List<Platform> architectures = Platform.getArchitecturesFromString(project.option("architectures", ""), platform);

        // Collect bundle/package resources to be included in bundle directory
        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, architectures);

        BobProjectProperties projectProperties = project.getProjectProperties();

        BundleHelper.throwIfCanceled(canceled);

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

        File appDir = new File(bundleDirectory, title);
        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());

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

        // Copy debug symbols if they were generated
        String zipDir = FilenameUtils.concat(extenderExeDir, Platform.JsWeb.getExtenderPair());
        File bundleSymbols = new File(zipDir, "dmengine.js.symbols");
        if (!bundleSymbols.exists()) {
            zipDir = FilenameUtils.concat(extenderExeDir, Platform.WasmWeb.getExtenderPair());
            bundleSymbols = new File(zipDir, "dmengine.js.symbols");
        }
        if (bundleSymbols.exists()) {
            File symbolsOut = new File(appDir, enginePrefix + ".symbols");
            FileUtils.copyFile(bundleSymbols, symbolsOut);
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

        BundleHelper helper = new BundleHelper(project, platform, appDir, variant);

        helper.copyOrWriteManifestFile(platform, appDir);

        FileUtils.copyURLToFile(getResource("dmloader.js"), new File(appDir, "dmloader.js"));

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
