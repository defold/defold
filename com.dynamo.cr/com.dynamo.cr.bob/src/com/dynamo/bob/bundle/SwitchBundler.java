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

public class SwitchBundler implements IBundler {
    private static Logger logger = Logger.getLogger(SwitchBundler.class.getName());
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

    private File getLibraryDir(String variant) {
        final String buildTarget = "NX-NXFP2-a64";
        String mode = "Debug";
        if (variant.equals("release")) {
            mode = "Release";
        }
        return new File(String.format("Libraries/%s/%s", buildTarget, mode));
    }

    @Override
    public void bundleApplication(Project project, File bundleDir, ICanceled canceled) throws IOException, CompileExceptionError {
        final Platform platform = Platform.Armv7Android;
        final List<Platform> architectures = Platform.getArchitecturesFromString(project.option("architectures", ""), platform);

        Hashtable<Platform, String> platformToLibMap = new Hashtable<Platform, String>();
        platformToLibMap.put(Platform.Arm64NX64, "arm64-nx64");

        Hashtable<Platform, File> platformToExeFileMap = new Hashtable<Platform, File>();
        Hashtable<Platform, String> platformToExePathMap = new Hashtable<Platform, String>();

        BobProjectProperties projectProperties = project.getProjectProperties();
        final String variant = project.option("variant", Bob.VARIANT_RELEASE);
        boolean isDebug = variant.equals(Bob.VARIANT_DEBUG);

        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String exeName = "main";

        BundleHelper.throwIfCanceled(canceled);


        // If a custom engine was built we need to copy it
        String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");

        for (Platform architecture : architectures) {
            List<File> bundleExe = Bob.getNativeExtensionEngineBinaries(architecture, extenderExeDir);
            if (bundleExe == null) {
                bundleExe = Bob.getDefaultDmengineFiles(architecture, variant);
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
        BundleHelper helper = new BundleHelper(project, targetPlatform, bundleDir, variant);

        // Create APK
        File ap1 = new File(appDir, title + ".ap1");

        helper.copyOrWriteManifestFile(architectures.get(0), appDir);

        BundleHelper.throwIfCanceled(canceled);

        helper.copyAndroidResources(architectures.get(0), appDir);
        helper.aaptMakePackage(architectures.get(0), appDir, ap1);

        BundleHelper.throwIfCanceled(canceled);

        // AAPT needs all resources on disc
        Map<String, IResource> androidResources = ExtenderUtil.getAndroidResources(project);

        BundleHelper.throwIfCanceled(canceled);

        // Collect bundle/package resources to be included in APK zip
        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, architectures);

        BundleHelper.throwIfCanceled(canceled);


        /*
            for (String name : Arrays.asList("game.projectc", "game.arci", "game.arcd", "game.dmanifest", "game.public.der")) {
                BundleHelper.throwIfCanceled(canceled);
                File source = new File(new File(projectRoot, contentRoot), name);
                ZipEntry ze = new ZipEntry(normalize("assets/" + name, true));
                zipOut.putNextEntry(ze);
                FileUtils.copyFile(source, zipOut);
            }

            BundleHelper.throwIfCanceled(canceled);

            // Copy bundle resources into the data folder
            //ExtenderUtil.writeResourcesToZip(bundleResources, zipOut);

            BundleHelper.throwIfCanceled(canceled);
*/
            /*

    bundle_main = bundle_parent.exclusive_build_node(code_dir+'/main')
    bundle_npdm = bundle_parent.exclusive_build_node(code_dir+'/main.npdm')
    bundle_rtld = bundle_parent.exclusive_build_node(code_dir+'/rtld')
    bundle_sdk  = bundle_parent.exclusive_build_node(code_dir+'/sdk')
            */

    /*
            // Copy executables
            for (Platform architecture : architectures) {
                String filename = FilenameUtils.concat("lib/" + platformToLibMap.get(architecture), "lib" + exeName + ".so");
                filename = FilenameUtils.normalize(filename, true);
                zipOut.putNextEntry(new ZipEntry(filename));
                File file = new File(platformToExePathMap.get(architecture));
                file.deleteOnExit();
                FileUtils.copyFile(file, zipOut);
            }
        File ap3 = new File(appDir, title + ".ap3");

        BundleHelper.throwIfCanceled(canceled);

        // Sign

        Result r = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "apkc"),
                "--in=" + ap2.getAbsolutePath(),
                "--out=" + ap3.getAbsolutePath());
        if (r.ret != 0) {
            if (r.ret != 0 ) {
                throw new IOException(new String(r.stdOutErr));
            }
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

*/

        FileUtils.deleteDirectory(tmpResourceDir);

        // Copy debug symbols
        //if (has_symbols) {
            // TODO: Investigate if there's any data that we want to store here
        //}
    }
}
