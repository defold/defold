package com.dynamo.cr.editor;

import java.lang.reflect.InvocationTargetException;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.equinox.internal.p2.core.helpers.LogHelper;
import org.eclipse.equinox.internal.p2.core.helpers.ServiceHelper;
import org.eclipse.equinox.p2.core.IProvisioningAgent;
import org.eclipse.equinox.p2.operations.UpdateOperation;
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
import org.eclipse.ui.internal.ide.EditorAreaDropAdapter;

import com.dynamo.cr.editor.ui.EditorUIPlugin;
import com.dynamo.cr.editor.ui.IEditorWindow;

@SuppressWarnings("restriction")
public class ApplicationWorkbenchWindowAdvisor extends WorkbenchWindowAdvisor {

    private IEditorWindow editorWindow;

    public ApplicationWorkbenchWindowAdvisor(IWorkbenchWindowConfigurer configurer) {
        super(configurer);
    }

    public ActionBarAdvisor createActionBarAdvisor(IActionBarConfigurer configurer) {
        return new ApplicationActionBarAdvisor(configurer);
    }

    @Override
    public void createWindowContents(final Shell shell) {
        editorWindow = EditorUIPlugin.getDefault().createWindow(shell, getWindowConfigurer());
        editorWindow.createContents(shell);
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

    @Override
    public void postWindowOpen() {
        super.postWindowOpen();
        checkForUpdates();
    }
}
