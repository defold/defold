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
import java.util.Map;

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
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.handlers.HandlerUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Platform;
import com.dynamo.bob.util.Exec;
import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.engine.Engine;
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

        @Override
        public void run(IProgressMonitor monitor)
                throws InvocationTargetException, InterruptedException {

            monitor.beginTask("Signing and uploading...", 10);

            try {
                String identity = presenter.getIdentity();
                String profile = presenter.getProfile();

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

                String engineArmv7 = Engine.getDefault().getEnginePath("ios");
                String engineArm64 = Engine.getDefault().getEnginePath("arm64-ios");

                // Create universal binary
                Path tmpDir = Files.createTempDirectory("lipoTmp");
                tmpDir.toFile().deleteOnExit();
                File tmpFile = new File(tmpDir.toAbsolutePath().toString(), "dmengine");
                tmpFile.deleteOnExit();
                String engine = tmpFile.getPath();

                // Run lipo to add armv7 + arm64 together into universal bin
                Exec.exec( Bob.getExe(Platform.getHostPlatform(), "lipo"), "-create", engineArmv7, engineArm64, "-output", engine );

                String ipaPath = signer.sign(identity, profile, engine, properties);
                monitor.worked(1);

                File ipaFile = new File(ipaPath);

                SubProgressMonitor sub = new SubProgressMonitor(monitor, 9);
                sub.beginTask("Uploading...", (int) ipaFile.length());
                BufferedInputStream fileStream = new BufferedInputStream(new FileInputStream(ipaPath));
                ProgressInputStream stream = new ProgressInputStream(sub, fileStream);

                projectClient.uploadEngine("ios", stream);
            } catch (Exception e) {
                final String msg = e.getMessage();
                final Status status = new Status(IStatus.ERROR, "com.dynamo.cr", msg);
                shell.getDisplay().asyncExec(new Runnable() {
                    @Override
                    public void run() {
                        ErrorDialog.openError(shell, "Error signing executable", "Unable to sign application", status);
                    }
                });
                logger.error("Unable to sign application", e);
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
