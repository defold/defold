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
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.MultipleCompileException.Info;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
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

    private boolean copyIcon(BobProjectProperties projectProperties, String projectRoot, File resDir, String name, String outName)
            throws IOException {
        String resource = projectProperties.getStringValue("android", name);
        if (resource != null && resource.length() > 0) {
            File inFile = new File(projectRoot, resource);
            File outFile = new File(resDir, outName);
            FileUtils.copyFile(inFile, outFile);
            return true;
        }
        return false;
    }

    private boolean copyIconDPI(BobProjectProperties projectProperties, String projectRoot, File resDir, String name, String outName, String dpi)
            throws IOException {
            return copyIcon(projectProperties, projectRoot, resDir, name + "_" + dpi, "drawable-" + dpi + "/" + outName);
    }

    public void createAndroidManifest(BobProjectProperties projectProperties, String projectRoot, File manifestFile, File resOutput, String exeName) throws IOException {
     // Copy icons
        int iconCount = 0;
        // copy old 32x32 icon first, the correct size is actually 36x36
        if (copyIcon(projectProperties, projectRoot, resOutput, "app_icon_32x32", "drawable-ldpi/icon.png")
            || copyIcon(projectProperties, projectRoot, resOutput, "app_icon_36x36", "drawable-ldpi/icon.png"))
            iconCount++;
        if (copyIcon(projectProperties, projectRoot, resOutput, "app_icon_48x48", "drawable-mdpi/icon.png"))
            iconCount++;
        if (copyIcon(projectProperties, projectRoot, resOutput, "app_icon_72x72", "drawable-hdpi/icon.png"))
            iconCount++;
        if (copyIcon(projectProperties, projectRoot, resOutput, "app_icon_96x96", "drawable-xhdpi/icon.png"))
            iconCount++;
        if (copyIcon(projectProperties, projectRoot, resOutput, "app_icon_144x144", "drawable-xxhdpi/icon.png"))
            iconCount++;
        if (copyIcon(projectProperties, projectRoot, resOutput, "app_icon_192x192", "drawable-xxxhdpi/icon.png"))
            iconCount++;

        // Copy push notification icons
        if (copyIcon(projectProperties, projectRoot, resOutput, "push_icon_small", "drawable/push_icon_small.png"))
            iconCount++;
        if (copyIcon(projectProperties, projectRoot, resOutput, "push_icon_large", "drawable/push_icon_large.png"))
            iconCount++;

        String[] dpis = new String[] { "ldpi", "mdpi", "hdpi", "xhdpi", "xxhdpi", "xxxhdpi" };
        for (String dpi : dpis) {
            if (copyIconDPI(projectProperties, projectRoot, resOutput, "push_icon_small", "push_icon_small.png", dpi))
                iconCount++;
            if (copyIconDPI(projectProperties, projectRoot, resOutput, "push_icon_large", "push_icon_large.png", dpi))
                iconCount++;
        }

        Map<String, Object> properties = new HashMap<>();
        if (iconCount > 0) {
            properties.put("has-icons?", true);
        } else {
            properties.put("has-icons?", false);
        }
        properties.put("exe-name", exeName);

        if(projectProperties.getBooleanValue("display", "dynamic_orientation", false)==false) {
            Integer displayWidth = projectProperties.getIntValue("display", "width");
            Integer displayHeight = projectProperties.getIntValue("display", "height");
            if((displayWidth != null & displayHeight != null) && (displayWidth > displayHeight)) {
                properties.put("orientation-support", "landscape");
            } else {
                properties.put("orientation-support", "portrait");
            }
        } else {
            properties.put("orientation-support", "sensor");
        }
        format(properties, "android", "manifest", "resources/android/AndroidManifest.xml", manifestFile);
    }

    public static class ResourceInfo
    {
        public String severity;
        public String resource;
        public String message;
        public int lineNumber;

        public ResourceInfo(String severity, String resource, String lineNumber, String message) {
            this.severity = severity;
            this.resource = resource;
            this.message = message;
            this.lineNumber = Integer.parseInt(lineNumber.equals("") ? "1" : lineNumber);
        }
    };

    // These regexp's works for both cpp and javac errors, warnings and note entries associated with a resource.
    private static Pattern resourceIssueGCCRe = Pattern.compile("^(?:\\/tmp\\/job[0-9]*\\/)?(?:upload|build)\\/([^:]+):([0-9]+):([0-9]*)?:?\\s*(error|warning|note|):?\\s*(.+)"); // GCC + Clang + Java
    private static Pattern resourceIssueCLRe = Pattern.compile("^(?:upload|build)\\/([^:]+)\\(([0-9]+)\\)([0-9]*):\\s*(fatal error|error|warning|note).*?:\\s*(.+)"); // CL.exe
    private static Pattern resourceIssueLinkerLINKRe = Pattern.compile("^.+?\\.lib\\((.+?)\\)\\s:([0-9]*)([0-9]*)\\s*(error|warning|note).*?:\\s*(.+)"); // LINK.exe (the line/column numbers won't really match anything)
    private static Pattern resourceIssueLinkerCLANGRe = Pattern.compile("^(Undefined symbols for architecture [\\w]+:\\n.*?referenced from:\\n.*)");

    // Some errors/warning have an extra line before or after the the reported error, which is also very good to have
    private static Pattern resourceIssueLineBeforeRe = Pattern.compile("^.*upload\\/([^:]+):\\s*(.+)");

    private static Pattern resourceIssueLinkerLINKLibError = Pattern.compile("^(.+\\.lib)(\\(.+\\.obj\\)\\s:\\s.+)");
    private static Pattern resourceIssueLinkerLINKCatchAll = Pattern.compile("^(.+error\\s.+)");

    // Matches ext.manifest and also _app/app.manifest
    private static Pattern manifestIssueRe = Pattern.compile("^.+'(.+\\.manifest)'.+");

    // This regexp catches errors, warnings and note entries that are not associated with a resource.
    private static Pattern nonResourceIssueRe = Pattern.compile("^(fatal|error|warning|note):\\s*(.+)");

    private static List<String> excludeMessages = new ArrayList<String>() {{
        add("[options] bootstrap class path not set in conjunction with -source 1.6"); // Mighty annoying message
    }};

    private static void parseLogGCC(String[] lines, List<ResourceInfo> issues) {
        final Pattern compilerPattern = resourceIssueGCCRe; // catches linker errors too

        for (int count = 0; count < lines.length; ++count) {
            String line = lines[count];
            Matcher m = compilerPattern.matcher(line);

            if (m.matches()) {
                // Groups: resource, line, column, "error", message
                String severity = m.group(4);
                if (severity == null || severity.equals(""))
                    severity = "error";
                BundleHelper.ResourceInfo info = new BundleHelper.ResourceInfo(severity, m.group(1), m.group(2), m.group(5));
                issues.add(info);

                // Some errors have a preceding line (with the same file name, but no line number)
                if (count > 0) {
                    String lineBefore = lines[count-1];
                    m = compilerPattern.matcher(lineBefore);
                    if (!m.matches() && lineBefore.contains(info.resource)) {
                        m = BundleHelper.resourceIssueLineBeforeRe.matcher(lineBefore);
                        if (m.matches()) {
                            if (info.resource.equals(m.group(1)))
                                info.message = m.group(2) + "\n" + info.message;
                        }
                    }
                }

                // Also, there might be a trailing line that belongs (See BundleHelperTest for examples)
                if (count+1 < lines.length) {
                    String lineAfter = lines[count+1];
                    m = BundleHelper.resourceIssueLineBeforeRe.matcher(lineAfter);
                    if (!line.equals("") && !m.matches()) {
                        info.message = info.message + "\n" + lineAfter;
                        count++;
                    }
                }
                continue;
            }
        }
    }

    private static void parseLogClang(String[] lines, List<ResourceInfo> issues) {
        final Pattern linkerPattern = resourceIssueLinkerCLANGRe;

        // Very similar, with lookBehind and lookAhead for errors
        parseLogGCC(lines, issues);

        for (int count = 0; count < lines.length; ++count) {
            String line = lines[count];
            // Compare with some lookahead if it matches
            for (int i = 1; i <= 2 && (count+i) < lines.length; ++i) {
                line += "\n" + lines[count+i];
            }
            Matcher m = linkerPattern.matcher(line);
            if (m.matches()) {
                // Groups: message
                issues.add(new BundleHelper.ResourceInfo("error", null, "", m.group(1)));
            }
        }
    }

    private static void parseLogWin32(String[] lines, List<ResourceInfo> issues) {

        for (int count = 0; count < lines.length; ++count) {
            String line = lines[count];
            Matcher m = resourceIssueCLRe.matcher(line);
            if (m.matches()) {
                // Groups: resource, line, column, "error", message
                issues.add(new BundleHelper.ResourceInfo(m.group(4), m.group(1), m.group(2), m.group(5)));
            }

            m = resourceIssueLinkerLINKRe.matcher(line);
            if (m.matches()) {
                // Groups: resource, line, column, "error", message
                issues.add(new BundleHelper.ResourceInfo(m.group(4), m.group(1), m.group(2), m.group(5)));
            }

            m = resourceIssueLinkerLINKLibError.matcher(line);
            if (m.matches()) {
                // Groups: resource, message
                issues.add(new BundleHelper.ResourceInfo("error", m.group(1), "", line));
            }

            m = resourceIssueLinkerLINKCatchAll.matcher(line);
            if (m.matches()) {
                // Groups: message
                issues.add(new BundleHelper.ResourceInfo("error", null, "", line));
            }
        }

        parseLogClang(lines, issues);
    }

    public static void parseLog(String platform, String log, List<ResourceInfo> issues) {
        String[] lines = log.split("\\r?\\n");

        List<ResourceInfo> allIssues = new ArrayList<ResourceInfo>();
        if (platform.contains("osx") || platform.contains("ios") || platform.contains("web")) {
            parseLogClang(lines, allIssues);
        } else if (platform.contains("android") || platform.contains("linux")) {
            parseLogGCC(lines, allIssues);
        } else if (platform.contains("win32")) {
            parseLogWin32(lines, allIssues);
        }

        for (int count = 0; count < lines.length; ++count) {
            String line = lines[count];

            // A bit special. Currently, no more errors are delivered after this
            // So we can consume the rest of the log into this message
            Matcher m = BundleHelper.manifestIssueRe.matcher(line);
            if (m.matches()) {
                allIssues.add( new BundleHelper.ResourceInfo("error", m.group(1), "1", log) );
                return;
            }

            m = BundleHelper.nonResourceIssueRe.matcher(line);

            if (m.matches()) {
                allIssues.add( new BundleHelper.ResourceInfo(m.group(1), null, "0", m.group(2)) );
                continue;
            }
        }

        for (ResourceInfo info : allIssues) {
            if (excludeMessages.contains(info.message)) {
                continue;
            }
            issues.add(info);
        }
    }

    public static void buildEngineRemote(ExtenderClient extender, String platform, String sdkVersion, List<ExtenderResource> allSource, File logFile, String srcName, File outputEngine, File outputClassesDex) throws CompileExceptionError, MultipleCompileException {
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
                    List<ResourceInfo> issues = new ArrayList<>();
                    buildError = FileUtils.readFileToString(logFile);
                    parseLog(platform, buildError, issues);
                    MultipleCompileException exception = new MultipleCompileException("Build error", e);
                    IResource extManifestResource = ExtenderUtil.getResource(allSource.get(0).getPath(), allSource);
                    IResource contextResource = null;

                    for (ResourceInfo info : issues) {

                        IResource issueResource = info.resource == null ? null : ExtenderUtil.getResource(info.resource, allSource);

                        // If it's the app manifest, let's translate it back into its original name
                        if (info.resource != null && info.resource.endsWith(ExtenderClient.appManifestFilename)) {
                            for (ExtenderResource extResource : allSource) {
                                if (extResource.getAbsPath().endsWith(info.resource)) {
                                    issueResource = ((ExtenderUtil.FSAliasResource)extResource).getResource();
                                    info.message = info.message.replace(extResource.getPath(), issueResource.getPath());
                                    break;
                                }
                            }
                        }

                        IResource exceptionResource = issueResource == null ? extManifestResource : issueResource;
                        int severity = info.severity.contains("error") ? Info.SEVERITY_ERROR : info.severity.equals("warning") ? Info.SEVERITY_WARNING : Info.SEVERITY_INFO;
                        exception.addIssue(severity, exceptionResource, info.message, info.lineNumber);

                        // The first resource generating errors should be related - we can use it to give context to the raw log.
                        if (contextResource == null && issueResource != null) {
                            contextResource = issueResource;
                        }
                    }

                    // If we do not yet have a context resource - fall back on an ext.manifest (possibly the wrong one!)
                    if (contextResource == null) {
                        contextResource = extManifestResource;
                    }

                    exception.attachLog(contextResource, buildError);
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

        // If we expect a classes.dex file, try to extract it from the zip
        if (outputClassesDex != null) {
            int nameindex = 1;
            while(true)
            {
                String name = nameindex == 1 ? "classes.dex" : String.format("classes%d.dex", nameindex);
                ++nameindex;

                File dex = new File(outputClassesDex.getParent(), name);
                try {
                    Path source = zip.getPath(name);
                    if (!Files.isReadable(source))
                        break;
                    try (FileOutputStream out = new FileOutputStream(dex)) {
                        Files.copy(source, out);
                    }
                } catch (IOException e) {
                    throw new CompileExceptionError(String.format("Failed to copy %s to %s", name, dex.getAbsolutePath()), e.getCause());
                }
            }
        }

        try {
            Path source = zip.getPath(srcName);
            try (FileOutputStream out = new FileOutputStream(outputEngine)) {
                Files.copy(source, out);
            }
            outputEngine.setExecutable(true);
        } catch (IOException e) {
            throw new CompileExceptionError(String.format("Failed to copy %s to %s", srcName, outputEngine.getAbsolutePath()), e.getCause());
        }
    }

}
