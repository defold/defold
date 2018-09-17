package com.dynamo.cr.editor.handlers;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.SubProgressMonitor;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.dialogs.ProgressMonitorDialog;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.handlers.HandlerUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.util.Exec;
import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.builders.ProgressDelegate;
import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.editor.preferences.PreferenceConstants;
import com.dynamo.cr.target.sign.IIdentityLister;
import com.dynamo.cr.target.sign.ISignView;
import com.dynamo.cr.target.sign.IdentityLister;
import com.dynamo.cr.target.sign.SignDialog;
import com.dynamo.cr.target.sign.SignPresenter;
import com.dynamo.cr.target.sign.Signer;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

/**
 * Sign and upload handler
 * TODO: The SignHandler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author chmu
 *
 */
public class SignHandler extends AbstractHandler {

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISignView.class).toInstance(view);
            bind(IIdentityLister.class).toInstance(identityLister);
        }
    }

    private static Logger logger = LoggerFactory.getLogger(SignHandler.class);
    private SignDialog view;
    private IdentityLister identityLister;
    private SignPresenter presenter;
    private Shell shell;
    private IProjectClient projectClient;


    static class ProgressInputStream extends InputStream {

        private InputStream stream;
        private IProgressMonitor monitor;

        public ProgressInputStream(IProgressMonitor monitor, InputStream stream) {
            this.monitor = monitor;
            this.stream = stream;
        }

        @Override
        public int read() throws IOException {
            int ret =  stream.read();
            if (ret > 0)
                monitor.worked(1);
            return ret;
        }

        @Override
        public int read(byte[] b) throws IOException {
            int ret = stream.read(b);
            if (ret > 0)
                monitor.worked(ret);
            return ret;
        }
    }

    class SignRunnable implements IRunnableWithProgress {

        private String profile;
        private void showErrorDialog(Exception e, final String title, String msg) {
            final Status status = new Status(IStatus.ERROR, "com.dynamo.cr", msg);
            if(e instanceof ConfigurationException) {
                msg = "Error reading provisioning profile '" + profile + "'. Make sure this is a valid provisioning profile file.";
            }
            shell.getDisplay().asyncExec(new Runnable() {
                @Override
                public void run() {
                    ErrorDialog.openError(shell, "Error signing executable", title, status);
                }
            });
            logger.error(title + (msg.isEmpty() == true ? "" : " (" + msg + ")"), e);
        }

        @Override
        public void run(IProgressMonitor monitor)
                throws InvocationTargetException, InterruptedException {

            monitor.beginTask("Signing and uploading...", 10);

            File exeArmv7 = null;
            File exeArm64 = null;

            IBranchClient branchClient = Activator.getDefault().getBranchClient();
            String sourceDirectory = branchClient.getNativeLocation();
            String buildDirectory = FilenameUtils.concat(sourceDirectory, "build");

            // Create dummy project used to trigger engine build
            Project tmpProject = null;
            Map<String, IResource> bundleResources = null;
            try {
                tmpProject = Bob.createProject("", sourceDirectory, "build", false, null, null);
                tmpProject.loadProjectFile();

                // Collect bundle/package resources to be included in .App directory
                bundleResources = ExtenderUtil.collectResources(tmpProject, Platform.Arm64Darwin);
            } catch (Exception e) {
                showErrorDialog(e, "Unable to build engine", e.getMessage());
            }


            try {
                final String variant = Bob.VARIANT_DEBUG;  // We always sign the debug non-stripped executable

                exeArmv7 = new File(Bob.getDefaultDmenginePath(Platform.Armv7Darwin, variant));
                exeArm64 = new File(Bob.getDefaultDmenginePath(Platform.Arm64Darwin, variant));

                final IPreferenceStore store = Activator.getDefault().getPreferenceStore();
                String nativeExtServerURI = store.getString(PreferenceConstants.P_NATIVE_EXT_SERVER_URI);

                tmpProject.setOption("build-server", nativeExtServerURI);
                tmpProject.setOption("binary-output", buildDirectory);
                tmpProject.setOption("platform", Platform.Arm64Darwin.getPair());

                EditorCorePlugin corePlugin = EditorCorePlugin.getDefault();
                String sdkVersion = corePlugin.getSha1();
                if (sdkVersion == "NO SHA1") {
                    sdkVersion = "";
                }
                tmpProject.setOption("defoldsdk", sdkVersion);

                final String[] platforms = tmpProject.getPlatformStrings();
                // Get or build engine binary
                boolean buildRemoteEngine = ExtenderUtil.hasNativeExtensions(tmpProject);
                if (!buildRemoteEngine) {
                    String engineName = Bob.getDefaultDmengineExeName(variant);
                    for (String platformString : platforms) {
                        Platform platform = Platform.get(platformString);
                        if (!Bob.hasExe(platform, engineName)){
                            buildRemoteEngine = true;
                            break;
                        }
                    }
                }
                if (buildRemoteEngine) {
                    tmpProject.buildEngine(new ProgressDelegate(monitor), platforms, variant);
	            }

                // Get engine executables
                // armv7 exe
                {
                    Platform targetPlatform = Platform.Armv7Darwin;
                    File extenderExe = new File(FilenameUtils.concat(buildDirectory, FilenameUtils.concat(targetPlatform.getExtenderPair(), targetPlatform.formatBinaryName("dmengine"))));
                    if (extenderExe.exists()) {
                        exeArmv7 = extenderExe;
                    }
                }

                // arm64 exe
                {
                    Platform targetPlatform = Platform.Arm64Darwin;
                    File extenderExe = new File(FilenameUtils.concat(buildDirectory, FilenameUtils.concat(targetPlatform.getExtenderPair(), targetPlatform.formatBinaryName("dmengine"))));
                    if (extenderExe.exists()) {
                        exeArm64 = extenderExe;
                    }
                }

            } catch (Exception e) {
                showErrorDialog(e, "Unable to build engine", e.getMessage());
            }

            profile = presenter.getProfile();
            try {
                String identity = presenter.getIdentity();

                IProject project = EditorUtil.getProject();
                InputStream projectIS = EditorUtil.getContentRoot(project).getFile("game.project").getContents();

                ProjectProperties projectProperties = new ProjectProperties();
                try {
                    projectProperties.load(projectIS);
                } finally {
                    IOUtils.closeQuietly(projectIS);
                }

                Signer signer = new Signer();
                Map<String, String> properties = new HashMap<String, String>();
                properties.put("CFBundleDisplayName", projectProperties.getStringValue("project", "title", "Unnamed"));
                properties.put("CFBundleExecutable", "dmengine");
                properties.put("CFBundleIdentifier", projectProperties.getStringValue("ios", "bundle_identifier", "dmengine"));

                if(projectProperties.getBooleanValue("display", "dynamic_orientation", false)==false) {
                    Integer displayWidth = projectProperties.getIntValue("display", "width");
                    Integer displayHeight = projectProperties.getIntValue("display", "height");
                    if((displayWidth != null & displayHeight != null) && (displayWidth > displayHeight)) {
                        properties.put("UISupportedInterfaceOrientations",      "UIInterfaceOrientationLandscapeRight");
                        properties.put("UISupportedInterfaceOrientations~ipad", "UIInterfaceOrientationLandscapeRight");
                    } else {
                        properties.put("UISupportedInterfaceOrientations",      "UIInterfaceOrientationPortrait");
                        properties.put("UISupportedInterfaceOrientations~ipad", "UIInterfaceOrientationPortrait");
                    }
                }

                // Create universal binary
                Path tmpDir = Files.createTempDirectory("lipoTmp");
                tmpDir.toFile().deleteOnExit();
                File tmpFile = new File(tmpDir.toAbsolutePath().toString(), "dmengine");
                tmpFile.deleteOnExit();
                String engine = tmpFile.getPath();

                // Run lipo to add armv7 + arm64 together into universal bin
                Exec.exec( Bob.getExe(Platform.getHostPlatform(), "lipo"), "-create", exeArmv7.getAbsolutePath(), exeArm64.getAbsolutePath(), "-output", engine );

                String ipaPath = signer.sign(identity, profile, engine, properties, bundleResources);
                monitor.worked(1);

                File ipaFile = new File(ipaPath);

                SubProgressMonitor sub = new SubProgressMonitor(monitor, 9);
                sub.beginTask("Uploading...", (int) ipaFile.length());
                BufferedInputStream fileStream = new BufferedInputStream(new FileInputStream(ipaPath));
                ProgressInputStream stream = new ProgressInputStream(sub, fileStream);

                projectClient.uploadEngine("ios", stream);
            } catch (Exception e) {
                showErrorDialog(e, "Unable to sign application", e.getMessage());
            }
        }

    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        projectClient = Activator.getDefault().projectClient;

        if (projectClient == null) {
            return null;
        }

        shell = HandlerUtil.getActiveShell(event);
        view = new SignDialog(shell);
        identityLister = new IdentityLister();
        Module module = new Module();
        Injector injector = Guice.createInjector(module);
        presenter = injector.getInstance(SignPresenter.class);
        view.setPresenter(presenter);
        int ret = view.open();
        if (ret == Dialog.OK) {

            ProgressMonitorDialog dialog = new ProgressMonitorDialog(shell);
            try {
                dialog.run(true, false, new SignRunnable());
            } catch (Exception e) {
                throw new RuntimeException(e);
            }

        }
        return null;
    }

    @Override
    public boolean isEnabled() {
        return EditorUtil.isMac() && Activator.getDefault().projectClient != null;
    }
}
