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
        final Platform platform = Platform.Arm64NX64;
        final List<Platform> architectures = Platform.getArchitecturesFromString(project.option("architectures", ""), platform);

        Platform hostPlatform = Platform.getHostPlatform();
        if (hostPlatform != Platform.X86_64Win32)
        {
            throw new IOException("The bundling can currently only be done on Windows!");
        }

        // Extract tools
        Bob.initSwitch();

        Hashtable<Platform, String> platformToLibMap = new Hashtable<Platform, String>();
        platformToLibMap.put(Platform.Arm64NX64, "arm64-nx64");

        Hashtable<Platform, File> platformToExeFileMap = new Hashtable<Platform, File>();
        Hashtable<Platform, String> platformToExePathMap = new Hashtable<Platform, String>();

        BobProjectProperties projectProperties = project.getProjectProperties();
        final String variant = project.option("variant", Bob.VARIANT_RELEASE);
        boolean isDebug = variant.equals(Bob.VARIANT_DEBUG);

        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String exeName = BundleHelper.projectNameToBinaryName(title);

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

        Platform targetPlatform = architectures.get(0);

        String libDir = isDebug ? "Develop" : "Release";

        File mainSrc = platformToExeFileMap.get(targetPlatform);
        File nnrtldSrc = new File(Bob.getLibExecPath(targetPlatform.getPair() + "/" + libDir + "/nnrtld.nso"));
        File nnSdkEnSrc = new File(Bob.getLibExecPath(targetPlatform.getPair() + "/" + libDir + "/nnSdkEn.nso"));
        File vulkanSrc = new File(Bob.getLibExecPath(targetPlatform.getPair() + "/" + libDir + "/vulkan.nso"));
        File openglSrc = new File(Bob.getLibExecPath(targetPlatform.getPair() + "/" + libDir + "/opengl.nso"));
        File applicationDesc = new File(Bob.getLibExecPath(targetPlatform.getPair() + "/Resources/SpecFiles/Application.desc"));

        String bundleType = "nspd";

        File nsp = new File(bundleDir, exeName + ".nsp");
        File appDir = new File(bundleDir, exeName + ".nspd");
        File programDir = new File(appDir, "program0.ncd");
        File codeDir = new File(programDir, "code");
        File dataDir = new File(programDir, "data");
        FileUtils.deleteDirectory(appDir);
        codeDir.mkdirs();

        // Copy binaries
        int subsdkIndex = 0;
        FileUtils.copyFile(mainSrc, new File(codeDir, "main"));
        FileUtils.copyFile(nnrtldSrc, new File(codeDir, "rtld"));
        FileUtils.copyFile(nnSdkEnSrc, new File(codeDir, "sdk"));
        FileUtils.copyFile(vulkanSrc, new File(codeDir, String.format("subsdk%d", subsdkIndex++)));
        FileUtils.copyFile(openglSrc, new File(codeDir, String.format("subsdk%d", subsdkIndex++)));
        File npdm = new File(codeDir, "main.npdm");

        String contentRoot = project.getBuildDirectory();
        String projectRoot = project.getRootDirectory();

        BundleHelper helper = new BundleHelper(project, targetPlatform, bundleDir, variant);

        // Copy resources
        File tmpResourceDir = Files.createTempDirectory("res").toFile();
        tmpResourceDir.mkdirs();

        // The application meta file refers to this relative path, next to the meta file itself
        IResource iconResource = helper.getResource("switch", "icon");

        File icon = new File(tmpResourceDir, "icon.bmp");
        helper.writeResourceToFile(iconResource, icon);

        // Only needed during the bundling
        helper.copyOrWriteManifestFile(architectures.get(0), tmpResourceDir);
        File mainManifest = new File(tmpResourceDir, helper.getMainManifestName(targetPlatform));

        BundleHelper.throwIfCanceled(canceled);

        // Collect bundle/package resources to be included in the bundle
        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, architectures);

        BundleHelper.throwIfCanceled(canceled);

        // Copy data
        for (String name : Arrays.asList("game.projectc", "game.arci", "game.arcd", "game.dmanifest", "game.public.der")) {
            BundleHelper.throwIfCanceled(canceled);
            File src = new File(new File(projectRoot, contentRoot), name);
            File dst = new File(dataDir, name);
            FileUtils.copyFile(src, dst);
        }

        BundleHelper.throwIfCanceled(canceled);

        // Copy bundle resources into the app folder
        ExtenderUtil.writeResourcesToDirectory(bundleResources, dataDir);

        BundleHelper.throwIfCanceled(canceled);

        String authoringTool = Bob.getPath("AuthoringTool/AuthoringTool.exe");
        String makemeta = Bob.getPath("MakeMeta/MakeMeta.exe");

        logger.log(Level.INFO, String.format("Creating meta file: %s",npdm));

        Result res;

        // Create the meta file (npdm)
        res = Exec.execResult(makemeta
                ,"--desc", applicationDesc.getAbsolutePath()
                ,"--meta", mainManifest.getAbsolutePath()
                ,"-o", npdm.getAbsolutePath()
                ,"-d", "DefaultIs64BitInstruction=True"
                ,"-d", "DefaultProcessAddressSpace=AddressSpace64Bit");
        if (res.ret != 0) {
            throw new IOException(new String(res.stdOutErr));
        }

        BundleHelper.throwIfCanceled(canceled);

        // Create the .nspd folder + .nspd_root file 
        logger.log(Level.INFO, String.format("Creating nspd: %s", appDir));

        res = Exec.execResult(authoringTool, "createnspd"
                ,"-o", appDir.getAbsolutePath()
                ,"--meta", mainManifest.getAbsolutePath()
                ,"--type", "Application"
                ,"--program", codeDir.getAbsolutePath(), dataDir.getAbsolutePath()
                ,"--utf8");
        if (res.ret != 0) {
            throw new IOException(new String(res.stdOutErr));
        }

        BundleHelper.throwIfCanceled(canceled);

        // Create the bundle version
        logger.log(Level.INFO, String.format("Creating nsp: %s", nsp));

        res = Exec.execResult(authoringTool, "creatensp"
                ,"-o", nsp.getAbsolutePath()
                ,"--meta", mainManifest.getAbsolutePath()
                ,"--desc", applicationDesc.getAbsolutePath()
                ,"--type", "Application"
                ,"--program", codeDir.getAbsolutePath(), dataDir.getAbsolutePath()
                ,"--utf8");
        if (res.ret != 0) {
            throw new IOException(new String(res.stdOutErr));
        }

        BundleHelper.throwIfCanceled(canceled);

        FileUtils.deleteDirectory(tmpResourceDir);

        // Copy debug symbols
        //if (has_symbols) {
            // TODO: Investigate if there's any data that we want to store here
        //}
    }
}
