package com.dynamo.cr.target.bundle;

import static org.apache.commons.io.FilenameUtils.normalize;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.io.filefilter.DirectoryFileFilter;
import org.apache.commons.io.filefilter.RegexFileFilter;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.common.util.Exec;
import com.dynamo.cr.common.util.Exec.Result;
import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.target.core.TargetPlugin;

public class AndroidBundler {
    private static Logger logger = LoggerFactory.getLogger(AndroidBundler.class);
    private ProjectProperties projectProperties;
    private String exe;
    private String projectRoot;
    private String contentRoot;
    private String title;
    private File appDir;
    private String certificate;
    private String key;
    private File packageDir;
    private String version;
    private String versionCode;
    private String androidPackage;
    private File resDir;

    private static List<PropertyAlias> propertyAliases = new ArrayList<AndroidBundler.PropertyAlias>();

    static class PropertyAlias {
        String bundleProperty;
        String category;
        String key;
        String defaultValue;

        PropertyAlias(String bundleProperty, String category, String key,
                String defaultValue) {
            this.bundleProperty = bundleProperty;
            this.category = category;
            this.key = key;
            this.defaultValue = defaultValue;
        }
    }

    private static void addProperty(String bundleProperty, String category,
            String key, String defaultValue) {
        PropertyAlias alias = new PropertyAlias(bundleProperty, category, key,
                defaultValue);
        propertyAliases.add(alias);
    }

    static {
        addProperty("CFBundleIdentifier", "ios", "bundle_identifier", "example.unnamed");
        addProperty("CFBundleVersion", "project", "version", "1.0");
        addProperty("CFBundleShortVersionString", "project", "version", "1.0");
    }

    private boolean copyIcon(String name, String outName)
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

    /**
     *
     * @param projectProperties
     *            corresponding game.project file
     * @param exe
     *            path to executable
     * @param projectRoot
     *            project root
     * @param contentRoot
     *            path to *compiled* content
     * @param outputDir
     *            output directory
     */
    public AndroidBundler(String certificate, String key,
            ProjectProperties projectProperties, String exe,
            String projectRoot, String contentRoot, String outputDir) {
        this.certificate = certificate;
        this.key = key;
        this.projectProperties = projectProperties;
        this.exe = exe;
        this.projectRoot = projectRoot;
        this.contentRoot = contentRoot;

        packageDir = new File(outputDir);
        this.title = projectProperties.getStringValue("project", "title", "Unnamed");
        this.version = projectProperties.getStringValue("project", "version", "1.0");
        this.versionCode = projectProperties.getStringValue("android", "versionCode", "1");
        this.androidPackage = projectProperties.getStringValue("android", "package", "com.example.todo");
        appDir = new File(packageDir, title);
        resDir = new File(packageDir, title + "/res");
    }

    public void bundleApplication() throws IOException, ConfigurationException {
        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();
        resDir.mkdirs();
        FileUtils.forceMkdir(new File(resDir, "drawable"));
        FileUtils.forceMkdir(new File(resDir, "drawable-ldpi"));
        FileUtils.forceMkdir(new File(resDir, "drawable-mdpi"));
        FileUtils.forceMkdir(new File(resDir, "drawable-hdpi"));
        FileUtils.forceMkdir(new File(resDir, "drawable-xhdpi"));
        FileUtils.forceMkdir(new File(appDir, "libs/armeabi-v7a"));

        // Create AndroidManfiest.xml
        InputStream manifestIn = getClass().getResourceAsStream("resources/android/AndroidManifest.xml");
        String manifest = IOUtils.toString(manifestIn);
        manifestIn.close();

        manifest = manifest.replaceAll("\\{\\{label\\}\\}", this.title);
        manifest = manifest.replaceAll("\\{\\{package\\}\\}", this.androidPackage);
        manifest = manifest.replaceAll("\\{\\{versionCode\\}\\}", this.versionCode);
        manifest = manifest.replaceAll("\\{\\{versionName\\}\\}", this.version);

        // Copy icons
        //copyIcon("app_icon_36x36", "drawable-ldpi/icon.png");
        int iconCount = 0;
        if (copyIcon("app_icon_32x32", "drawable-ldpi/icon.png"))
            iconCount++;
        if (copyIcon("app_icon_48x48", "drawable-mdpi/icon.png"))
            iconCount++;
        if (copyIcon("app_icon_72x72", "drawable-hdpi/icon.png"))
            iconCount++;
        if (copyIcon("app_icon_96x96", "drawable-xhdpi/icon.png"))
            iconCount++;

        if (iconCount > 0) {
            manifest = manifest.replaceAll("\\{\\{application_attr\\}\\}", "android:icon=\"@drawable/icon\"");
        } else {
            manifest = manifest.replaceAll("\\{\\{application_attr\\}\\}", "");
        }

        File manifestFile = new File(appDir, "AndroidManifest.xml");
        FileUtils.write(manifestFile, manifest);

        // Create APK
        File ap1 = new File(appDir, title + ".ap1");
        TargetPlugin plugin = TargetPlugin.getDefault();

        Result res = Exec.execResult(plugin.getAaptPath(),
                "package",
                "--no-crunch",
                "-f",
                //"--debug-mode",
                "-S", resDir.getAbsolutePath(),
                "-M", manifestFile.getAbsolutePath(),
                "-I", plugin.getAndroirJarPath(),
                "-F", ap1.getAbsolutePath());

        if (res.ret != 0) {
            throw new IOException(new String(res.stdOutErr));
        }

        File ap2 = new File(appDir, title + ".ap2");
        ZipInputStream zipIn = null;
        ZipOutputStream zipOut = null;
        try {
            zipIn = new ZipInputStream(new FileInputStream(ap1));
            zipOut = new ZipOutputStream(new FileOutputStream(ap2));

            ZipEntry inE = zipIn.getNextEntry();
            while (inE != null) {
                zipOut.putNextEntry(new ZipEntry(inE.getName()));
                IOUtils.copy(zipIn, zipOut);
                inE = zipIn.getNextEntry();
            }

            Collection<File> resources = FileUtils.listFiles(new File(contentRoot),
                                                             new RegexFileFilter("^(.*?)"),
                                                             DirectoryFileFilter.DIRECTORY);

            int prefixLen = normalize(new File(contentRoot).getPath(), true).length();
            for (File r : resources) {
                String p = normalize(r.getPath(), true).substring(prefixLen);
                if (!(p.startsWith("/builtins") || p.equals("/game.arc"))) {
                    String ap = normalize("assets" + p, true);
                    System.out.println(ap);
                    zipOut.putNextEntry(new ZipEntry(ap));
                    FileUtils.copyFile(r, zipOut);
                }
            }

            // Copy executable
            zipOut.putNextEntry(new ZipEntry(FilenameUtils.concat("lib/armeabi-v7a", FilenameUtils.getName(exe))));
            FileUtils.copyFile(new File(exe), zipOut);

        } finally {
            IOUtils.closeQuietly(zipIn);
            IOUtils.closeQuietly(zipOut);
        }

        // Sign
        File apk = new File(appDir, title + ".apk");
        if (certificate.length() > 0 && key.length() > 0) {
            Result r = Exec.execResult(plugin.getApkcPath(),
                    "--in=" + ap2.getAbsolutePath(),
                    "--out=" + apk.getAbsolutePath(),
                    "-cert=" + certificate,
                    "-key=" + key);
            if (r.ret != 0 ) {
                throw new IOException(new String(r.stdOutErr));
            }
        } else {
            Result r = Exec.execResult(plugin.getApkcPath(),
                    "--in=" + ap2.getAbsolutePath(),
                    "--out=" + apk.getAbsolutePath());
            if (r.ret != 0) {
                if (r.ret != 0 ) {
                    throw new IOException(new String(r.stdOutErr));
                }
            }
        }

        ap1.delete();
        ap2.delete();
    }
}
