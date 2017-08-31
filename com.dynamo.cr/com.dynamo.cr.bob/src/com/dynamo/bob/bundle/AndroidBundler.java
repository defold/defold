package com.dynamo.bob.bundle;

import static org.apache.commons.io.FilenameUtils.normalize;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;

public class AndroidBundler implements IBundler {
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

    private Pattern aaptResourceErrorRe = Pattern.compile("^invalid resource directory name:\\s(.+)\\s(.+)\\s.*$", Pattern.MULTILINE);

    @Override
    public void bundleApplication(Project project, File bundleDir)
            throws IOException, CompileExceptionError {

        // Collect bundle/package resources to be included in APK zip
        Map<String, IResource> bundleResources = ExtenderUtil.collectResources(project, Platform.Armv7Android);

        BobProjectProperties projectProperties = project.getProjectProperties();
        final boolean debug = project.hasOption("debug");

        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String exeName = title.replace(' ', '_');

        String certificate = project.option("certificate", "");
        String key = project.option("private-key", "");

        // If a custom engine was built we need to copy it
        Platform targetPlatform = Platform.Armv7Android;
        String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");
        File extenderExe = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(targetPlatform.getExtenderPair(), targetPlatform.formatBinaryName("dmengine"))));
        File defaultExe = new File(Bob.getDmengineExe(targetPlatform, debug));
        File bundleExe = defaultExe;
        File classesDex = new File(Bob.getPath("lib/classes.dex"));
        if (extenderExe.exists()) {
            bundleExe = extenderExe;
            classesDex = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(targetPlatform.getExtenderPair(), "classes.dex")));
        }

        File appDir = new File(bundleDir, title);
        File resDir = new File(appDir, "res");

        String contentRoot = project.getBuildDirectory();
        String projectRoot = project.getRootDirectory();

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();
        resDir.mkdirs();
        FileUtils.forceMkdir(new File(resDir, "drawable"));
        FileUtils.forceMkdir(new File(resDir, "drawable-ldpi"));
        FileUtils.forceMkdir(new File(resDir, "drawable-mdpi"));
        FileUtils.forceMkdir(new File(resDir, "drawable-hdpi"));
        FileUtils.forceMkdir(new File(resDir, "drawable-xhdpi"));
        FileUtils.forceMkdir(new File(resDir, "drawable-xxhdpi"));
        FileUtils.forceMkdir(new File(resDir, "drawable-xxxhdpi"));
        FileUtils.forceMkdir(new File(appDir, "libs/armeabi-v7a"));

        // Create AndroidManifest.xml and output icon resources (if available)
        BundleHelper helper = new BundleHelper(project, Platform.Armv7Android, bundleDir, "");
        File manifestFile = new File(appDir, "AndroidManifest.xml");
        helper.createAndroidManifest(projectProperties, projectRoot, manifestFile, resDir, exeName);

        // Create APK
        File ap1 = new File(appDir, title + ".ap1");

        Map<String, String> aaptEnv = new HashMap<String, String>();
        if (Platform.getHostPlatform() == Platform.X86_64Linux || Platform.getHostPlatform() == Platform.X86Linux) {
            aaptEnv.put("LD_LIBRARY_PATH", Bob.getPath(String.format("%s/lib", Platform.getHostPlatform().getPair())));
        }

        List<String> androidResourceFolders = ExtenderUtil.getAndroidResourcePaths(project, targetPlatform);

        // Remove any paths that begin with any android resource paths so they are not added twice (once by us, and once by aapt)
        {
            Map<String, IResource> newBundleResources = new HashMap<String, IResource>();
            Iterator<Map.Entry<String, IResource>> it = bundleResources.entrySet().iterator();
            while (it.hasNext()) {
                Map.Entry<String, IResource> entry = (Map.Entry<String, IResource>)it.next();

                boolean discarded = false;
                for( String resourceFolder : androidResourceFolders )
                {
                    if( entry.getValue().getAbsPath().startsWith(resourceFolder) )
                    {
                        discarded = true;
                        break;
                    }
                }
                if(!discarded) {
                    newBundleResources.put(entry.getKey(), entry.getValue());
                }
            }
            bundleResources = newBundleResources;
        }

        List<String> args = new ArrayList<String>();
        args.add(Bob.getExe(Platform.getHostPlatform(), "aapt"));
        args.add("package");
        args.add("--no-crunch");
        args.add("-f");
        args.add("--extra-packages");
        args.add("com.facebook:com.google.android.gms");
        args.add("-m");
        //args.add("--debug-mode");
        args.add("--auto-add-overlay");

        for( String s : androidResourceFolders )
        {
            args.add("-S"); args.add(s);
        }

        args.add("-S"); args.add(resDir.getAbsolutePath());
        args.add("-S"); args.add(Bob.getPath("res/facebook"));
        args.add("-S"); args.add(Bob.getPath("res/google-play-services"));

        args.add("-M"); args.add(manifestFile.getAbsolutePath());
        args.add("-I"); args.add(Bob.getPath("lib/android.jar"));
        args.add("-F"); args.add(ap1.getAbsolutePath());
        Result res = Exec.execResultWithEnvironment(aaptEnv, args);

        if (res.ret != 0) {
            String msg = new String(res.stdOutErr);

            // Try our best to visualize the error from aapt
            Matcher m = aaptResourceErrorRe.matcher(msg);
            if (m.matches()) {
                String path = m.group(1);
                if (path.startsWith(project.getRootDirectory())) {
                    path = path.substring(project.getRootDirectory().length());
                }
                IResource r = project.getResource(FilenameUtils.concat(path, m.group(2))); // folder + filename
                if (r != null) {
                    throw new CompileExceptionError(r, 1, String.format("Invalid Android resource folder name: '%s'\nSee https://developer.android.com/guide/topics/resources/providing-resources.html#table1 for valid directory names.\nAAPT Error: %s", m.group(2), msg));
                }
            }
            throw new IOException(msg);
        }

        File tmpClassesDex = new File(appDir, "classes.dex");
        FileUtils.copyFile(classesDex, tmpClassesDex);

        res = Exec.execResultWithEnvironmentWorkDir(aaptEnv, appDir, Bob.getExe(Platform.getHostPlatform(), "aapt"),
                "add",
                ap1.getAbsolutePath(),
                "classes.dex");

        tmpClassesDex.delete();

        if (res.ret != 0) {
            throw new IOException(new String(res.stdOutErr));
        }

        File ap2 = File.createTempFile(title, ".ap2");
        ap2.deleteOnExit();
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

            for (String name : Arrays.asList("game.projectc", "game.arci", "game.arcd", "game.dmanifest", "game.public.der")) {
                File source = new File(new File(projectRoot, contentRoot), name);
                ZipEntry ze = new ZipEntry(normalize("assets/" + name, true));
                zipOut.putNextEntry(ze);
                FileUtils.copyFile(source, zipOut);
            }

            // Copy bundle resources into .apk zip (actually .ap2 in this case)
            ExtenderUtil.writeResourcesToZip(bundleResources, zipOut);

            // Strip executable
            String strippedpath = bundleExe.getAbsolutePath();
            if( !debug )
            {
                File tmp = File.createTempFile(title, "." + bundleExe.getName() + ".stripped");
                tmp.deleteOnExit();
                strippedpath = tmp.getAbsolutePath();
                FileUtils.copyFile(bundleExe, tmp);

                res = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "strip_android"), strippedpath);
                if (res.ret != 0) {
                    throw new IOException(new String(res.stdOutErr));
                }
            }

            // Copy executable
            String filename = FilenameUtils.concat("lib/armeabi-v7a", "lib" + exeName + ".so");
            filename = FilenameUtils.normalize(filename, true);
            zipOut.putNextEntry(new ZipEntry(filename));
            FileUtils.copyFile(new File(strippedpath), zipOut);
        } finally {
            IOUtils.closeQuietly(zipIn);
            IOUtils.closeQuietly(zipOut);
        }

        File ap3 = new File(appDir, title + ".ap3");

        // Sign
        if (certificate.length() > 0 && key.length() > 0) {
            Result r = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "apkc"),
                    "--in=" + ap2.getAbsolutePath(),
                    "--out=" + ap3.getAbsolutePath(),
                    "-cert=" + certificate,
                    "-key=" + key);
            if (r.ret != 0 ) {
                throw new IOException(new String(r.stdOutErr));
            }
        } else {
            Result r = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "apkc"),
                    "--in=" + ap2.getAbsolutePath(),
                    "--out=" + ap3.getAbsolutePath());
            if (r.ret != 0) {
                if (r.ret != 0 ) {
                    throw new IOException(new String(r.stdOutErr));
                }
            }
        }

        // Rezip with some files as STORED
        File ap4 = File.createTempFile(title, ".ap4");
        ap4.deleteOnExit();
        ZipFile zipFileIn = null;
        zipOut = null;
        try {
            zipFileIn = new ZipFile(ap3);
            zipOut = new ZipOutputStream(new FileOutputStream(ap4));

            Enumeration<? extends ZipEntry> entries = zipFileIn.entries();
            while (entries.hasMoreElements()) {
                ZipEntry inE = entries.nextElement();

                ZipEntry ze = new ZipEntry(inE.getName());
                ze.setSize(inE.getSize());
                byte[] entryData = null;
                CRC32 crc = null;

                // Some files need to be STORED instead of DEFLATED to
                // get "correct" memory mapping at runtime.
                int zipMethod = ZipEntry.DEFLATED;
                if (Arrays.asList("assets/game.projectc", "assets/game.arci", "assets/game.arcd", "assets/game.dmanifest", "assets/game.public.der").contains(inE.getName())) {
                    // Set up uncompresed file, unfortunately need to calculate crc32 and other data for this to work.
                    // https://blogs.oracle.com/CoreJavaTechTips/entry/creating_zip_and_jar_files
                    crc = new CRC32();
                    zipMethod = ZipEntry.STORED;
                    ze.setCompressedSize(inE.getSize());
                }

                ByteArrayOutputStream byteOut = new ByteArrayOutputStream();
                try {
                	if (inE.getSize() > 0) {
	                    int count;
	                    entryData = new byte[(int) inE.getSize()];
	                    InputStream stream = zipFileIn.getInputStream(inE);
	                    while((count = stream.read(entryData, 0, (int)inE.getSize())) != -1) {
	                        byteOut.write(entryData, 0, count);
	                        if (zipMethod == ZipEntry.STORED) {
	                            crc.update(entryData, 0, count);
	                        }
	                    }
                	}
                } finally {
                    if(null != byteOut) {
                        byteOut.close();
                        entryData = byteOut.toByteArray();
                    }
                }

                if (zipMethod == ZipEntry.STORED) {
                    ze.setCrc(crc.getValue());
                    ze.setMethod(zipMethod);
                }

                zipOut.putNextEntry(ze);
                zipOut.write(entryData);
                zipOut.closeEntry();
            }

        } finally {
            IOUtils.closeQuietly(zipFileIn);
            IOUtils.closeQuietly(zipOut);
        }

        File apk = new File(appDir, title + ".apk");
        Result r = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "zipalign"),
                "-v", "4",
                ap4.getAbsolutePath(),
                apk.getAbsolutePath());

        if (r.ret != 0) {
            if (r.ret != 0 ) {
                throw new IOException(new String(r.stdOutErr));
            }
        }

        ap1.delete();
        ap2.delete();
        ap3.delete();
        ap4.delete();
    }
}
