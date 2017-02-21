package com.dynamo.bob.bundle;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.StringWriter;
import java.net.MalformedURLException;
import java.net.URL;
import java.nio.file.FileSystem;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;

import com.defold.extender.client.ExtenderClient;
import com.defold.extender.client.ExtenderClientException;
import com.defold.extender.client.ExtenderResource;
import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.MultipleCompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.util.BobProjectProperties;
import com.samskivert.mustache.Mustache;
import com.samskivert.mustache.Template;


public class BundleHelper {
    private Project project;
    private String title;
    private File buildDir;
    private File appDir;
    private Map<String, Map<String, Object>> propertiesMap;

    public BundleHelper(Project project, Platform platform, File bundleDir, String appDirSuffix) throws IOException {
        BobProjectProperties projectProperties = project.getProjectProperties();

        this.project = project;
        this.title = projectProperties.getStringValue("project", "title", "Unnamed");

        this.buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        this.appDir = new File(bundleDir, title + appDirSuffix);

        this.propertiesMap = createPropertiesMap(project.getProjectProperties());
    }

    static Object convert(String value, String type) {
        if (type != null && type.equals("bool")) {
            if (value != null) {
                return value.equals("1");
            } else {
                return false;
            }
        }
        return value;
    }

    public static Map<String, Map<String, Object>> createPropertiesMap(BobProjectProperties projectProperties) throws IOException {
        BobProjectProperties meta = new BobProjectProperties();
        InputStream is = Bob.class.getResourceAsStream("meta.properties");
        try {
            meta.load(is);
        } catch (ParseException e) {
            throw new RuntimeException("Failed to parse meta.properties", e);
        } finally {
            IOUtils.closeQuietly(is);
        }

        Map<String, Map<String, Object>> map = new HashMap<>();

        for (String c : meta.getCategoryNames()) {
            map.put(c, new HashMap<String, Object>());

            for (String k : meta.getKeys(c)) {
                if (k.endsWith(".default")) {
                    String k2 = k.split("\\.")[0];
                    String v = meta.getStringValue(c, k);
                    Object v2 = convert(v, meta.getStringValue(c, k2 + ".type"));
                    map.get(c).put(k2, v2);
                }
            }
        }

        for (String c : projectProperties.getCategoryNames()) {
            if (!map.containsKey(c)) {
                map.put(c, new HashMap<String, Object>());
            }

            for (String k : projectProperties.getKeys(c)) {
                String def = meta.getStringValue(c, k + ".default");
                map.get(c).put(k, def);
                String v = projectProperties.getStringValue(c, k);
                Object v2 = convert(v, meta.getStringValue(c, k + ".type"));
                map.get(c).put(k, v2);
            }
        }

        return map;
    }

    URL getResource(String category, String key, String defaultValue) {
        BobProjectProperties projectProperties = project.getProjectProperties();
        File projectRoot = new File(project.getRootDirectory());
        String s = projectProperties.getStringValue(category, key);
        if (s != null && s.trim().length() > 0) {
            try {
                return new File(projectRoot, s).toURI().toURL();
            } catch (MalformedURLException e) {
                throw new RuntimeException(e);
            }
        } else {
            return getClass().getResource(defaultValue);
        }
    }

    public BundleHelper format(Map<String, Object> properties, String templateCategory, String templateKey, String defaultTemplate, File toFile) throws IOException {
        URL templateURL = getResource(templateCategory, templateKey, defaultTemplate);
        Template template = Mustache.compiler().compile(IOUtils.toString(templateURL));
        StringWriter sw = new StringWriter();
        template.execute(this.propertiesMap, properties, sw);
        sw.flush();
        FileUtils.write(toFile, sw.toString());
        return this;
    }

    public BundleHelper format(String templateCategory, String templateKey, String defaultTemplate, File toFile) throws IOException {
        return format(new HashMap<String, Object>(), templateCategory, templateKey, defaultTemplate, toFile);
    }


    public BundleHelper copyBuilt(String name) throws IOException {
        FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
        return this;
    }

    static class ResourceInfo
    {
        public String resource;
        public String message;
        public int lineNumber;

        public ResourceInfo(String r, int l, String msg) {
            resource = r;
            lineNumber = l;
            message = msg;
        }
    };

    private static Pattern gccRe = Pattern.compile("^\\/tmp\\/upload[0-9]+\\/(.*):([0-9]+):([0-9]+):\\s+(.*)");

    private static void parseLog(String log, List<ResourceInfo> infos) {
        String[] lines = log.split("\n");
            for (String line : lines) {

            Matcher m = BundleHelper.gccRe.matcher(line);
            if (m.matches()) {
                String resource = m.group(1);
                int lineNumber = Integer.parseInt(m.group(2)); // column is group 3
                String msg = m.group(4);
                infos.add( new BundleHelper.ResourceInfo(resource, lineNumber, msg) );
            }
        }
    }

    public static void buildEngineRemote(ExtenderClient extender, String platform, String sdkVersion, List<ExtenderResource> allSource, File logFile, String srcName, File outputEngine) throws CompileExceptionError, MultipleCompileExceptionError {
        File zipFile = null;

        try {
            zipFile = File.createTempFile("build_" + sdkVersion, ".zip");
            zipFile.deleteOnExit();
        } catch (IOException e) {
            throw new CompileExceptionError("Failed to create temp zip file", e.getCause());
        }

        try {
            extender.build(platform, sdkVersion, allSource, zipFile, logFile);
        } catch (ExtenderClientException e) {
            String buildError = "<no log file>";
            if (logFile != null) {
                try {
                    buildError = FileUtils.readFileToString(logFile);

                    List<ResourceInfo> infos = new ArrayList<>();
                    parseLog(buildError, infos);

                    MultipleCompileExceptionError exception = new MultipleCompileExceptionError("Build error", e);

                    IResource firstresource = null;
                    if (infos.size() > 0) {
                        for (ResourceInfo info : infos) {
                            IResource resource = ExtenderUtil.getResource(info.resource, allSource);
                            if (resource != null) {
                                exception.addError(resource, info.message, info.lineNumber);

                                if (firstresource == null) {
                                    firstresource = resource;
                                }
                            }
                        }
                    }

                    // Trick to always supply the full log
                    // 1. We pick a resource, the first one generating errors should be related.
                    //    Otherwise we fall back on an ext.manifest (possibly the wrong one!)
                    // 2. We put it first, because the list is reversed later when presented
                    if (firstresource == null) {
                        firstresource = ExtenderUtil.getResource(allSource.get(0).getPath(), allSource);
                    }
                    exception.addError(firstresource, buildError, 1);

                    throw exception;
                } catch (IOException ioe) {
                    buildError = "<failed reading log>";
                }
            }
            buildError = String.format("'%s' could not be built. Sdk version: '%s'\nLog: '%s'", platform, sdkVersion, buildError);
            throw new CompileExceptionError(buildError, e.getCause());
        }

        FileSystem zip = null;
        try {
            zip = FileSystems.newFileSystem(zipFile.toPath(), null);
        } catch (IOException e) {
            throw new CompileExceptionError(String.format("Failed to mount temp zip file %s", zipFile.getAbsolutePath()), e.getCause());
        }

        try {
            Path source = zip.getPath(srcName);
            Files.copy(source, new FileOutputStream(outputEngine));
        } catch (IOException e) {
            throw new CompileExceptionError(String.format("Failed to copy %s to %s", srcName, outputEngine.getAbsolutePath()), e.getCause());
        }
    }

}
