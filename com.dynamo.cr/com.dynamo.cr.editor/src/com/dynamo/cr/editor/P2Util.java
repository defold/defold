package com.dynamo.cr.editor;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.OperationCanceledException;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.SubMonitor;
import org.eclipse.equinox.p2.core.IProvisioningAgent;
import org.eclipse.equinox.p2.metadata.IInstallableUnit;
import org.eclipse.equinox.p2.operations.ProvisioningJob;
import org.eclipse.equinox.p2.operations.ProvisioningSession;
import org.eclipse.equinox.p2.operations.UpdateOperation;
import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.swt.widgets.Display;

/**
 * This class shows an example for checking for updates and performing the
 * update synchronously.  It is up to the caller to run this in a job if
 * a background update check is desired.  This is a reasonable way to run an
 * operation when user intervention is not required.   Another approach is
 * to separately perform the resolution and provisioning steps, deciding
 * whether to perform these synchronously or in a job.
 *
 * Any p2 operation can be run modally (synchronously), or the job
 * can be requested and scheduled by the caller.
 *
 * @see UpdateOperation#resolveModal(IProgressMonitor)
 * @see UpdateOperation#getResolveJob(IProgressMonitor)
 * @see UpdateOperation#getProvisioningJob(IProgressMonitor)
 */
public class P2Util {

    private static boolean shouldUpdate(final String toVersion) {
        final Display display = Display.getDefault();
        final boolean[] ret = new boolean[] { false };
        display.syncExec(new Runnable() {

            @Override
            public void run() {
                boolean updateEditor = MessageDialog.openQuestion(display.getActiveShell(),
                        "Update found",
                        String.format("New version (%s) of editor found. Update?", toVersion));

                ret[0] = updateEditor;
            }
        });
        return ret[0];
    }

    public static IStatus checkForUpdates(IProvisioningAgent agent, IProgressMonitor monitor) throws OperationCanceledException {
         // NOTE: This method doens't run in the UI-thread. Hence all UI interaction must be performed using
         // syncExec or asyncExec.

        ProvisioningSession session = new ProvisioningSession(agent);
        // the default update operation looks for updates to the currently
        // running profile, using the default profile root marker. To change
        // which installable units are being updated, use the more detailed
        // constructors.
        UpdateOperation operation = new UpdateOperation(session);
        SubMonitor sub = SubMonitor.convert(monitor,
                "Checking for application updates...", 200);
        IStatus status = operation.resolveModal(sub.newChild(100));
        if (status.getCode() == UpdateOperation.STATUS_NOTHING_TO_UPDATE) {
            return status;
        }

        if (status.getSeverity() == IStatus.CANCEL) {
            throw new OperationCanceledException();
        }

        if (status.getSeverity() != IStatus.ERROR && operation.getSelectedUpdates().length > 0) {
            // NOTE: We assume single unit update (com.dynamo.cr.editor)
            IInstallableUnit to = operation.getSelectedUpdates()[0].replacement;

            if (!shouldUpdate(to.getVersion().toString())) {
                // throw OperationCanceledException instead?
                return Status.CANCEL_STATUS;
            }

            // More complex status handling might include showing the user what updates
            // are available if there are multiples, differentiating patches vs. updates, etc.
            // In this example, we simply update as suggested by the operation.
            ProvisioningJob job = operation.getProvisioningJob(null);
            status = job.runModal(sub.newChild(100));
            if (status.getSeverity() == IStatus.CANCEL) {
                throw new OperationCanceledException();
            }
        }
        return status;
    }
}
