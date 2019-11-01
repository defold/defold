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
import java.util.Hashtable;
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

        Hashtable<Platform, String> platformToLibMap = new Hashtable<Platform, String>();
        platformToLibMap.put(Platform.Armv7Android, "armeabi-v7a");
        platformToLibMap.put(Platform.Arm64Android, "arm64-v8a");

        Hashtable<Platform, String> platformToStripToolMap = new Hashtable<Platform, String>();
        platformToStripToolMap.put(Platform.Armv7Android, "strip_android");
        platformToStripToolMap.put(Platform.Arm64Android, "strip_android_aarch64");

        Hashtable<Platform, File> platformToExeFileMap = new Hashtable<Platform, File>();
        Hashtable<Platform, String> platformToExePathMap = new Hashtable<Platform, String>();

        BobProjectProperties projectProperties = project.getProjectProperties();
        final String variant = project.option("variant", Bob.VARIANT_RELEASE);
        final boolean strip_executable = project.hasOption("strip-executable");
        final boolean has_symbols = project.hasOption("with-symbols");

        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String exeName = BundleHelper.projectNameToBinaryName(title);

        String certificate = project.option("certificate", "");
        String key = project.option("private-key", "");

        BundleHelper.throwIfCanceled(canceled);

        // If a custom engine was built we need to copy it
        String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");

        ArrayList<File> classesDex = new ArrayList<File>();

        List<Platform> architectures = new ArrayList<Platform>();
        String[] architecturesStrings = project.option("architectures", "armv7-android,arm64-android").split(",");
        for (int i = 0; i < architecturesStrings.length; i++) {
            architectures.add(Platform.get(architecturesStrings[i]));
        }

        for (Platform architecture : architectures) {
            List<File> bundleExe = Bob.getNativeExtensionEngineBinaries(architecture, extenderExeDir);
            if (bundleExe == null) {
                bundleExe = Bob.getDefaultDmengineFiles(architecture, variant);
                if (classesDex.isEmpty()) {
                    classesDex.add(new File(Bob.getPath("lib/classes.dex")));
                }
            }
            else {
                logger.log(Level.INFO, "Using extender binary for architecture: " + architecture.toString());
                if (classesDex.isEmpty()) {
                    int i = 1;
                    while(true) {
                        String name = i == 1 ? "classes.dex" : String.format("classes%d.dex", i);
                        ++i;

                        File f = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(architecture.getExtenderPair(), name)));
                        if (!f.exists())
                            break;
                        classesDex.add(f);
                    }
                }
            }
            File exe = bundleExe.get(0);
            logger.log(Level.INFO, architecture.getPair() + " exe: " + exe.toString());

            platformToExeFileMap.put(architecture, exe);
            platformToExePathMap.put(architecture, exe.getAbsolutePath());

            BundleHelper.throwIfCanceled(canceled);
        }

        File appDir = new File(bundleDir, title);
        File resDir = new File(appDir, "res");
        File tmpResourceDir = Files.createTempDirectory("res").toFile();

        String contentRoot = project.getBuildDirectory();
        String projectRoot = project.getRootDirectory();

        BundleHelper.throwIfCanceled(canceled);

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();
        resDir.mkdirs();

        for(Platform architecture : architectures)
        {
            FileUtils.forceMkdir(new File(appDir, "libs/" + platformToLibMap.get(architecture)));
        }

        Platform targetPlatform = Platform.Armv7Android;
        BundleHelper helper = new BundleHelper(project, targetPlatform, bundleDir, "", variant);

        // Create APK
        File ap1 = new File(appDir, title + ".ap1");

        File manifestFile = new File(appDir, "AndroidManifest.xml"); // the final, merged manifest
        IResource sourceManifestFile = helper.getResource("android", "manifest");

        Map<String, Object> properties = helper.createAndroidManifestProperties(project.getRootDirectory(), resDir, exeName);
        try {
            helper.mergeManifests(properties, sourceManifestFile, manifestFile);
        } catch (CompileExceptionError e) {
            // Pass along
            throw e;
        } catch (Exception e) {
            throw new CompileExceptionError(sourceManifestFile, -1, e);
        }

        BundleHelper.throwIfCanceled(canceled);

        // Create properties and output icon resources (if available)
        helper.generateAndroidResources(project, resDir, manifestFile, ap1, tmpResourceDir);

        BundleHelper.throwIfCanceled(canceled);

        // AAPT needs all resources on disc
        Map<String, IResource> androidResources = ExtenderUtil.getAndroidResources(project);

        BundleHelper.throwIfCanceled(canceled);

        // Collect bundle/package resources to be included in APK zip
        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, targetPlatform);

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

            // Strip executables
            if( strip_executable )
            {
                for (Platform architecture : architectures) {
                    File exe = platformToExeFileMap.get(architecture);
                    File tmpStrippedExeFile = File.createTempFile(title, "." + exe.getName() + ".stripped");
                    String strippedExePath = tmpStrippedExeFile.getAbsolutePath();
                    tmpStrippedExeFile.deleteOnExit();
                    platformToExePathMap.put(architecture, tmpStrippedExeFile.getAbsolutePath());
                    FileUtils.copyFile(exe, tmpStrippedExeFile);

                    Result res = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), platformToStripToolMap.get(architecture)), strippedExePath);
                    if (res.ret != 0) {
                        throw new IOException(new String(res.stdOutErr));
                    }
                }
            }

            BundleHelper.throwIfCanceled(canceled);

            // Copy executables
            for (Platform architecture : architectures) {
                String filename = FilenameUtils.concat("lib/" + platformToLibMap.get(architecture), "lib" + exeName + ".so");
                filename = FilenameUtils.normalize(filename, true);
                zipOut.putNextEntry(new ZipEntry(filename));
                File file = new File(platformToExePathMap.get(architecture));
                file.deleteOnExit();
                FileUtils.copyFile(file, zipOut);
            }
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

        // Copy debug symbols
        if (has_symbols) {
            File symbolsDir = new File(appDir, title + ".apk.symbols");
            symbolsDir.mkdirs();

            for(Platform architecture : architectures)
            {
                //FileUtils.forceMkdir(new File(symbolsDir, "libs/" + platformToLibMap.get(architecture)));
                File buildExe = platformToExeFileMap.get(architecture);
                File symbolExe = new File(symbolsDir, FilenameUtils.concat("lib/" + platformToLibMap.get(architecture), "lib" + exeName + ".so"));
                FileUtils.copyFile(buildExe, symbolExe);
            }

            File proguardMapping = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(architectures.get(0).getExtenderPair(), "mapping.txt")));
            if (proguardMapping.exists()) {
                File symbolMapping = new File(symbolsDir, proguardMapping.getName());
                FileUtils.copyFile(proguardMapping, symbolMapping);
            }
        }
    }
}
