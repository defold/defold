package com.dynamo.bob.bundle;

import static org.apache.commons.io.FilenameUtils.normalize;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;
import java.util.logging.Level;
import java.util.logging.Logger;
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
    private static Logger logger = Logger.getLogger(AndroidBundler.class.getName());
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

    @Override
    public void bundleApplication(Project project, File bundleDir, ICanceled canceled) throws IOException, CompileExceptionError {
        Bob.initAndroid(); // extract resources

        BobProjectProperties projectProperties = project.getProjectProperties();
        final String variant = project.option("variant", Bob.VARIANT_RELEASE);
        final boolean strip_executable = project.hasOption("strip-executable");

        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String exeName = BundleHelper.projectNameToBinaryName(title);

        String certificate = project.option("certificate", "");
        String key = project.option("private-key", "");
        boolean debuggable = Integer.parseInt(projectProperties.getStringValue("android", "debuggable", "0")) != 0;

        // If a custom engine was built we need to copy it

        BundleHelper.throwIfCanceled(canceled);

        ArrayList<File> classesDex = new ArrayList<File>();
        ArrayList<String> classesDexFilenames = new ArrayList<String>();
        ArrayList<String> classesDex64Filenames = new ArrayList<String>();

        classesDex.add(new File(Bob.getPath("lib/classes.dex")));
        String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");
        List<File> bundleExes = null;
        List<File> bundleArm64Exes = null;
        // armv7 exe
        {
            Platform targetPlatform = Platform.Armv7Android;
            bundleExes = Bob.getNativeExtensionEngineBinaries(targetPlatform, extenderExeDir);
            if (bundleExes == null) {
                bundleExes = Bob.getDefaultDmengineFiles(targetPlatform, variant);
            }
            else {
                logger.log(Level.INFO, "Using extender binary for Armv7");
                classesDex = new ArrayList<File>();
                int i = 1;
                while(true)
                {
                    String name = i == 1 ? "classes.dex" : String.format("classes%d.dex", i);
                    ++i;

                    File f = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(targetPlatform.getExtenderPair(), name)));
                    if (!f.exists())
                        break;
                    classesDex.add(f);
                    classesDexFilenames.add(f.getAbsolutePath());
                }
            }
        }
        if (bundleExes.size() > 1) {
            throw new IOException("Invalid number of armeabi-v7a binaries for Android when bundling: " + bundleExes.size());
        }
        File bundleExe = bundleExes.get(0);

        // arm64 exe
        {
            Platform targetPlatform = Platform.Arm64Android;
            bundleArm64Exes = Bob.getNativeExtensionEngineBinaries(targetPlatform, extenderExeDir);
            if (bundleArm64Exes == null) {
                bundleArm64Exes = Bob.getDefaultDmengineFiles(targetPlatform, variant);
            }
            else {
                logger.log(Level.INFO, "Using extender binary for Arm64");
                classesDex = new ArrayList<File>();
                int i = 1;
                while(true)
                {
                    String name = i == 1 ? "classes.dex" : String.format("classes%d.dex", i);
                    ++i;

                    File f = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(targetPlatform.getExtenderPair(), name)));
                    if (!f.exists())
                        break;
                    classesDex.add(f);
                    classesDex64Filenames.add(f.getAbsolutePath());
                }
            }
        }
        if (bundleArm64Exes.size() > 1) {
            throw new IOException("Invalid number of arm64-v8a binaries for Android when bundling: " + bundleArm64Exes.size());
        }
        File bundleArm64Exe = bundleArm64Exes.get(0);

        File appDir = new File(bundleDir, title);
        File resDir = new File(appDir, "res");
        File tmpResourceDir = Files.createTempDirectory("res").toFile();

        String contentRoot = project.getBuildDirectory();
        String projectRoot = project.getRootDirectory();

        BundleHelper.throwIfCanceled(canceled);

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();
        resDir.mkdirs();
        FileUtils.forceMkdir(new File(appDir, "libs/armeabi-v7a"));
        FileUtils.forceMkdir(new File(appDir, "libs/arm64-v8a"));

        BundleHelper helper = new BundleHelper(project, Platform.Armv7Android, bundleDir, "");

        // Create APK
        File ap1 = new File(appDir, title + ".ap1");

        File manifestFile = new File(appDir, "AndroidManifest.xml"); // the final, merged manifest
        IResource sourceManifestFile = helper.getResource("android", "manifest");

        Map<String, Object> properties = helper.createAndroidManifestProperties(project.getRootDirectory(), resDir, exeName);
        helper.mergeManifests(project, targetPlatform, properties, sourceManifestFile, manifestFile);

        BundleHelper.throwIfCanceled(canceled);

        // Create properties and output icon resources (if available)
        helper.generateAndroidResources(project, targetPlatform, resDir, manifestFile, ap1, tmpResourceDir);

        BundleHelper.throwIfCanceled(canceled);

        // AAPT needs all resources on disc
        Map<String, IResource> androidResources = ExtenderUtil.getAndroidResources(project);

        // Find the actual android resource folders on disc
        Set<String> androidResourceFolders = new HashSet<>();
        {
            for (Map.Entry<String, IResource> entry : androidResources.entrySet()) {
                String absPath = entry.getValue().getAbsPath();                                 // /topfolder/extension/res/android/res/path/to/res.xml
                String relativePath = entry.getKey();                                           // path/to/res.xml
                String dir = absPath.substring(0, absPath.length() - relativePath.length());    // /topfolder/extension/res/android/res
                androidResourceFolders.add(dir);
            }
        }

        BundleHelper.throwIfCanceled(canceled);

        // Collect bundle/package resources to be included in APK zip
        Map<String, IResource> allResources = ExtenderUtil.collectResources(project, Platform.Arm64Android);

        // bundleResources = allResources - androidResources
        // Remove any paths that begin with any android resource paths so they are not added twice (once by us, and once by aapt)
        // This step is used to detect which resources that shouldn't be manually bundled, since aapt does that for us.
        Map<String, IResource> bundleResources;
        {
            Map<String, IResource> newBundleResources = new HashMap<>();
            for (Map.Entry<String, IResource> entry : allResources.entrySet()) {
                boolean discarded = false;
                for (String resourceFolder : androidResourceFolders) {
                    if (entry.getValue().getAbsPath().startsWith(resourceFolder)) {
                        discarded = true;
                        break;
                    }
                }
                if (!discarded) {
                    newBundleResources.put(entry.getKey(), entry.getValue());
                }
            }
            bundleResources = newBundleResources;
        }

        BundleHelper.throwIfCanceled(canceled);

        for(File dex : classesDex)
        {
            String name = dex.getName();
            File tmpClassesDex = new File(appDir, name);
            FileUtils.copyFile(dex, tmpClassesDex);

            BundleHelper.throwIfCanceled(canceled);

            Map<String, String> aaptEnv = new HashMap<>();
            if (Platform.getHostPlatform() == Platform.X86_64Linux) {
                aaptEnv.put("LD_LIBRARY_PATH", Bob.getPath(String.format("%s/lib", Platform.getHostPlatform().getPair())));
            }

            Result res = Exec.execResultWithEnvironmentWorkDir(aaptEnv, appDir, Bob.getExe(Platform.getHostPlatform(), "aapt"),
                    "add",
                    ap1.getAbsolutePath(),
                    name);

            tmpClassesDex.delete();

            if (res.ret != 0) {
                throw new IOException(new String(res.stdOutErr));
            }
        }

        BundleHelper.throwIfCanceled(canceled);

        File ap2 = File.createTempFile(title, ".ap2");
        ap2.deleteOnExit();
        ZipInputStream zipIn = null;
        ZipOutputStream zipOut = null;
        try {
            zipIn = new ZipInputStream(new FileInputStream(ap1));
            zipOut = new ZipOutputStream(new FileOutputStream(ap2));

            ZipEntry inE = zipIn.getNextEntry();
            while (inE != null) {
                BundleHelper.throwIfCanceled(canceled);
                zipOut.putNextEntry(new ZipEntry(inE.getName()));
                IOUtils.copy(zipIn, zipOut);
                inE = zipIn.getNextEntry();
            }

            for (String name : Arrays.asList("game.projectc", "game.arci", "game.arcd", "game.dmanifest", "game.public.der")) {
                BundleHelper.throwIfCanceled(canceled);
                File source = new File(new File(projectRoot, contentRoot), name);
                ZipEntry ze = new ZipEntry(normalize("assets/" + name, true));
                zipOut.putNextEntry(ze);
                FileUtils.copyFile(source, zipOut);
            }

            BundleHelper.throwIfCanceled(canceled);
            // Copy bundle resources into .apk zip (actually .ap2 in this case)
            ExtenderUtil.writeResourcesToZip(bundleResources, zipOut);

            BundleHelper.throwIfCanceled(canceled);
            // Strip executable
            String strippedpathArmv7 = bundleExe.getAbsolutePath();
            String strippedpathArm64 = bundleArm64Exe.getAbsolutePath();
            if( strip_executable )
            {
                File tmpArmv7 = File.createTempFile(title, "." + bundleExe.getName() + ".stripped");
                tmpArmv7.deleteOnExit();
                strippedpathArmv7 = tmpArmv7.getAbsolutePath();
                FileUtils.copyFile(bundleExe, tmpArmv7);
                res = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "strip_android"), strippedpathArmv7);
                if (res.ret != 0) {
                    throw new IOException(new String(res.stdOutErr));
                }

                File tmpArm64 = File.createTempFile(title, "." + bundleArm64Exe.getName() + ".stripped");
                tmpArm64.deleteOnExit();
                strippedpathArm64 = tmpArm64.getAbsolutePath();
                FileUtils.copyFile(bundleArm64Exe, tmpArm64);
                res = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "strip_android_aarch64"), strippedpathArm64);

                if (res.ret != 0) {
                    throw new IOException(new String(res.stdOutErr));
                }
            }

            BundleHelper.throwIfCanceled(canceled);
            // Copy executables
            // armeabi-v7a
            String filenameArmv7 = FilenameUtils.concat("lib/armeabi-v7a", "lib" + exeName + ".so");
            filenameArmv7 = FilenameUtils.normalize(filenameArmv7, true);
            zipOut.putNextEntry(new ZipEntry(filenameArmv7));
            File fileArmv7 = new File(strippedpathArmv7);
            fileArmv7.deleteOnExit();
            FileUtils.copyFile(fileArmv7, zipOut);
            // arm64-v8a
            String filenameArm64 = FilenameUtils.concat("lib/arm64-v8a", "lib" + exeName + ".so");
            filenameArm64 = FilenameUtils.normalize(filenameArm64, true);
            zipOut.putNextEntry(new ZipEntry(filenameArm64));
            File fileArm64 = new File(strippedpathArm64);
            fileArm64.deleteOnExit();
            FileUtils.copyFile(fileArm64, zipOut);
        } finally {
            IOUtils.closeQuietly(zipIn);
            IOUtils.closeQuietly(zipOut);
        }

        File ap3 = new File(appDir, title + ".ap3");

        BundleHelper.throwIfCanceled(canceled);
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

        BundleHelper.throwIfCanceled(canceled);
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
                BundleHelper.throwIfCanceled(canceled);

                ZipEntry inE = entries.nextElement();

                ZipEntry ze = new ZipEntry(inE.getName());
                ze.setSize(inE.getSize());
                byte[] entryData = null;
                CRC32 crc = null;

                // Some files need to be STORED instead of DEFLATED to
                // get "correct" memory mapping at runtime.
                int zipMethod = ZipEntry.DEFLATED;
                boolean isAsset = inE.getName().startsWith("assets");
                if (isAsset) {
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

        BundleHelper.throwIfCanceled(canceled);

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
        FileUtils.deleteDirectory(tmpResourceDir);
    }
}
