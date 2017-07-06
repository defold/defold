package com.dynamo.cr.editor;

import java.lang.reflect.InvocationTargetException;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.IExecutionListener;
import org.eclipse.core.commands.NotHandledException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.equinox.internal.p2.core.helpers.LogHelper;
import org.eclipse.equinox.internal.p2.core.helpers.ServiceHelper;
import org.eclipse.equinox.p2.core.IProvisioningAgent;
import org.eclipse.equinox.p2.operations.UpdateOperation;
import org.eclipse.equinox.p2.repository.IRepositoryManager;
import org.eclipse.equinox.p2.repository.artifact.IArtifactRepositoryManager;
import org.eclipse.equinox.p2.repository.metadata.IMetadataRepositoryManager;
import org.eclipse.jface.dialogs.ProgressMonitorDialog;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.application.ActionBarAdvisor;
import org.eclipse.ui.application.IActionBarConfigurer;
import org.eclipse.ui.application.IWorkbenchWindowConfigurer;
import org.eclipse.ui.application.WorkbenchWindowAdvisor;
import org.eclipse.ui.commands.ICommandService;
import org.eclipse.ui.internal.ide.EditorAreaDropAdapter;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.defold.editor.analytics.Analytics;
import com.defold.util.SupportPath;
import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.ui.EditorUIPlugin;
import com.dynamo.cr.editor.ui.IEditorWindow;

@SuppressWarnings("restriction")
public class ApplicationWorkbenchWindowAdvisor extends WorkbenchWindowAdvisor {

    private IEditorWindow editorWindow;
    private Analytics analytics;

    private static Logger logger = LoggerFactory.getLogger(ApplicationWorkbenchWindowAdvisor.class);

    public ApplicationWorkbenchWindowAdvisor(IWorkbenchWindowConfigurer configurer) {
        super(configurer);
    }

    public ActionBarAdvisor createActionBarAdvisor(IActionBarConfigurer configurer) {
        return new ApplicationActionBarAdvisor(configurer);
    }

    // TODO: This one isn't invoked in Eclipse 4. See WorkbenchWindowAdvisor#createWindowContents
    @Override
    public void createWindowContents(final Shell shell) {
        editorWindow = EditorUIPlugin.getDefault().createWindow(shell, getWindowConfigurer());
        editorWindow.createContents(shell);
        EditorCorePlugin corePlugin = EditorCorePlugin.getDefault();
        shell.setText(corePlugin.getTitle());
    }

    public void preWindowOpen() {
        IWorkbenchWindowConfigurer configurer = getWindowConfigurer();
        configurer.setInitialSize(new Point(1024, 768));
        configurer.setShowCoolBar(true);
        configurer.setShowStatusLine(true);
        configurer.setShowProgressIndicator(true);

        // Workaround for DnD null-pointer exception
        // http://dev.eclipse.org/newslists/news.eclipse.platform/msg79493.html
        configurer.configureEditorAreaDropListener(
                new EditorAreaDropAdapter(
                  configurer.getWindow()));
    }

    @Override
    public void dispose() {
        super.dispose();
        this.editorWindow.dispose();
    }

    private static final String JUSTUPDATED = "justUpdated";

    private void restartWorkbench() {
        Display.getDefault().syncExec(new Runnable() {
            @Override
            public void run() {
                final IPreferenceStore prefStore = Activator.getDefault().getPreferenceStore();
                prefStore.setValue(JUSTUPDATED, true);
                PlatformUI.getWorkbench().restart();
            }
        });
    }

    // Returns a list of URIs that contains `match` in the supplied IRepositoryManager
    private static List<URI> getReposContaining(IRepositoryManager<?> repoManager, String match) {
        ArrayList<URI> repoURIList = new ArrayList<URI>();
        for (URI repoURI : repoManager.getKnownRepositories(IRepositoryManager.REPOSITORIES_ALL)) {
            if (repoURI.toString().contains(match)) {
                repoURIList.add(repoURI);
            }
        }

        return repoURIList;
    }


    private void checkForUpdates() {

        if (System.getProperty("osgi.dev") != null) {
            // Do not run update when running from eclipse
            return;
        }

        final IProvisioningAgent agent = (IProvisioningAgent) ServiceHelper
                .getService(Activator.getContext(),
                        IProvisioningAgent.SERVICE_NAME);
        if (agent == null) {
            LogHelper.log(new Status(IStatus.ERROR, Activator.PLUGIN_ID,
                    "No provisioning agent found.  This application is not set up for updates."));
        }

        IMetadataRepositoryManager metadataManager = (IMetadataRepositoryManager) agent.getService(IMetadataRepositoryManager.SERVICE_NAME);
        IArtifactRepositoryManager artifactManager = (IArtifactRepositoryManager) agent.getService(IArtifactRepositoryManager.SERVICE_NAME);

        try {
            // Changed default p2 location to stable-channel on s3 for old installations
            URI oldLocation = new URI("http://cr.defold.com/downloads/cr-editor/1.0/repository/");
            URI newLocation = new URI("http://d.defold.com.s3-website-eu-west-1.amazonaws.com/update/stable");
            if (metadataManager.contains(oldLocation)) {
                logger.info(String.format("Changing p2 location from %s to %s", oldLocation, newLocation));
                metadataManager.removeRepository(oldLocation);
                artifactManager.removeRepository(oldLocation);

                metadataManager.addRepository(newLocation);
                artifactManager.addRepository(newLocation);
            }

            // Check if we have both beta and stable repositories in
            // either of artifactManager or metadataManager.
            List<URI> betaURIs = getReposContaining(metadataManager, "beta/update/");
            List<URI> stableURIs = getReposContaining(metadataManager, "stable/update/");
            betaURIs.addAll(getReposContaining(artifactManager, "beta/update/"));
            stableURIs.addAll(getReposContaining(artifactManager, "stable/update/"));

            // Remove all beta URIs if we have both beta and stable entries
            // from both artifactManager and metadataManager.
            if (betaURIs.size() > 0 && stableURIs.size() > 0) {
                System.out.println("Found both beta and stable update URIs; removing all beta entries.");

                for (URI repoURI : betaURIs) {
                    System.out.println(String.format("Removing URI: %s", repoURI.toString()));
                    metadataManager.removeRepository(repoURI);
                    artifactManager.removeRepository(repoURI);
                }
            }

            // Change AWS URIs into d.defold.com
            String awsURL = "d.defold.com.s3-website-eu-west-1.amazonaws.com";
            List<URI> awsURIs = getReposContaining(metadataManager, awsURL);
            awsURIs.addAll(getReposContaining(artifactManager, awsURL));
            for (URI repoURI : awsURIs) {
                URI newURI = new URI(repoURI.toString().replace(awsURL, "d.defold.com"));
                System.out.println(String.format("Changing p2 location from %s to %s", repoURI.toString(), newURI.toString()));

                metadataManager.removeRepository(repoURI);
                artifactManager.removeRepository(repoURI);

                metadataManager.addRepository(newURI);
                artifactManager.addRepository(newURI);
            }

        } catch (URISyntaxException e) {
            logger.error("Unexpected error", e);
        }

        final IPreferenceStore prefStore = Activator.getDefault().getPreferenceStore();
        if (prefStore.getBoolean(JUSTUPDATED)) {
            prefStore.setValue(JUSTUPDATED, false);
            return;
        }

        final IRunnableWithProgress runnable = new IRunnableWithProgress() {
            public void run(IProgressMonitor monitor)
                    throws InvocationTargetException, InterruptedException {
                IProgressMonitor safeMonitor = new UIDelegateProgressMonitor(monitor, Display.getDefault());
                IStatus updateStatus = P2Util.checkForUpdates(agent, safeMonitor);
                if (updateStatus.getCode() == UpdateOperation.STATUS_NOTHING_TO_UPDATE) {
                    // We do nothing, ie no dialog or similar :-)
                } else if (updateStatus.getSeverity() != IStatus.ERROR
                        && updateStatus.getSeverity() != IStatus.CANCEL) {
                    prefStore.setValue(JUSTUPDATED, true);
                    restartWorkbench();
                } else {
                    LogHelper.log(updateStatus);
                }
            }
        };

        final Display display = Display.getCurrent();
        display.asyncExec(new Runnable() {
            @Override
            public void run() {
                try {
                    // NOTE: We fork the update process to ensure we don't run in the user-interface thread
                    // If we block the user-interface thread the progress-bar isn't updated and windows
                    // might think that the application has hang
                    new ProgressMonitorDialog(display.getActiveShell()).run(true, true, runnable);
                } catch (InvocationTargetException e) {
                    Activator.logException(e);
                } catch (InterruptedException e) {
                    Activator.logException(e);
                }
            }
        });

    }

    private class ExecutionListener implements IExecutionListener {
        @Override
        public void notHandled(String commandId, NotHandledException exception) {}

        @Override
        public void postExecuteFailure(String commandId,
                ExecutionException exception) {}

        @Override
        public void postExecuteSuccess(String commandId, Object returnValue) {
            if (analytics != null) {
                analytics.trackScreen("defold", commandId);
            }
        }

        @Override
        public void preExecute(String commandId, ExecutionEvent event) { }
    }

    private static Path getSupportPath() {
        try {
            Path supportPath = SupportPath.getPlatformSupportPath("Defold");
            if (supportPath != null && (Files.exists(supportPath) || supportPath.toFile().mkdirs())) {
                return supportPath;
            }
        } catch (Exception e) {
            System.err.println("Unable to determine support path: " + e);
        }

        return null;
    }

    private void installAnalytics() {
        try {
            ICommandService commandService = (ICommandService) PlatformUI
                    .getWorkbench().getService(ICommandService.class);

            commandService.addExecutionListener(new ExecutionListener());

            String version = System.getProperty("defold.version");
            if (version != null) {
                version = "0." + version;
                String tid = "UA-83690-7";
                analytics = new Analytics(getSupportPath().toString(), tid, version);
            }

        } catch (Throwable e) {
            Activator.logException(e);
        }
    }

    @Override
    public void postWindowOpen() {
        super.postWindowOpen();
        checkForUpdates();
        installAnalytics();
    }
}
