package com.dynamo.cr.editor.handlers;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.net.URI;
import java.util.HashMap;
import java.util.Map;

import org.apache.commons.configuration.ConfigurationException;
import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.filesystem.EFS;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IncrementalProjectBuilder;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.SubProgressMonitor;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.dialogs.ProgressMonitorDialog;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.swt.widgets.DirectoryDialog;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.handlers.HandlerUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.engine.Engine;
import com.dynamo.cr.target.bundle.BundleiOSDialog;
import com.dynamo.cr.target.bundle.BundleiOSPresenter;
import com.dynamo.cr.target.bundle.IBundleiOSView;
import com.dynamo.cr.target.bundle.IOSBundler;
import com.dynamo.cr.target.sign.IIdentityLister;
import com.dynamo.cr.target.sign.IdentityLister;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

/**
 * Bundle iOS handler
 * TODO: The BundleiOSHandler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author chmu
 *
 */
public class BundleiOSHandler extends AbstractHandler {

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(IBundleiOSView.class).toInstance(view);
            bind(IIdentityLister.class).toInstance(identityLister);
        }
    }

    private static Logger logger = LoggerFactory.getLogger(BundleiOSHandler.class);
    private BundleiOSDialog view;
    private IdentityLister identityLister;
    private BundleiOSPresenter presenter;
    private Shell shell;
    private IProjectClient projectClient;
    private String outputDirectory;

    class BundleRunnable implements IRunnableWithProgress {

        private void buildProject(IProject project, IProgressMonitor monitor) throws CoreException {
            Map<String, String> args = new HashMap<String, String>();
            args.put("location", "local");
            project.build(IncrementalProjectBuilder.FULL_BUILD,  "com.dynamo.cr.editor.builders.contentbuilder", args, monitor);
        }

        private void bundle(IProgressMonitor monitor, IProject project)
                throws CoreException, IOException, ConfigurationException {
            String identity = presenter.getIdentity();
            String profile = presenter.getProvisioningProfile();

            IOSBundler bundler = new IOSBundler();
            Map<String, String> properties = new HashMap<String, String>();
            properties.put("CFBundleDisplayName", presenter.getApplicationName());
            properties.put("CFBundleIdentifier", presenter.getApplicationIdentifier());
            properties.put("CFBundleExecutable", "dmengine");
            String contentRoot = getCompiledContent(project);
            String engine = Engine.getDefault().getEnginePath("ios");
            bundler.bundleApplication(identity, profile, engine, contentRoot, outputDirectory, presenter.getIcons(), presenter.isPreRendered(), properties);
            monitor.worked(1);
        }

        private String getCompiledContent(IProject project) throws CoreException {
            URI contentLocation = EditorUtil.getContentRoot(project).getRawLocationURI();
            File localContentRoot = EFS.getStore(contentLocation).toLocalFile(0, new NullProgressMonitor());
            String contentRoot = new Path(localContentRoot.getAbsolutePath()).append("build").append("default").toPortableString();
            return contentRoot;
        }

        @Override
        public void run(IProgressMonitor monitor)
                throws InvocationTargetException, InterruptedException {
            IProject project = EditorUtil.getProject();
            monitor.beginTask("Bundling Application...", 10);
            try {
                buildProject(project, new SubProgressMonitor(monitor, 9));
                bundle(monitor, project);
                monitor.done();
            } catch (Exception e) {
                final String msg = e.getMessage();
                final Status status = new Status(IStatus.ERROR, "com.dynamo.cr", msg);
                shell.getDisplay().asyncExec(new Runnable() {
                    @Override
                    public void run() {
                        ErrorDialog.openError(shell, "Error occurred while bundling application", "Unable to bundle application", status);
                    }
                });
                logger.error("Unable to bundle application", e);
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
        view = new BundleiOSDialog(shell);
        identityLister = new IdentityLister();
        Module module = new Module();
        Injector injector = Guice.createInjector(module);
        presenter = injector.getInstance(BundleiOSPresenter.class);
        view.setPresenter(presenter);
        int ret = view.open();
        if (ret == Dialog.OK) {

            DirectoryDialog dirDialog = new DirectoryDialog(shell);
            dirDialog.setMessage("Select output directory for application");
            outputDirectory = dirDialog.open();
            if (outputDirectory == null)
                return null;

            ProgressMonitorDialog dialog = new ProgressMonitorDialog(shell);
            try {
                dialog.run(true, false, new BundleRunnable());
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
