package com.dynamo.cr.editor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.jobs.Job;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.handlers.HandlerUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.BobUtil;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.preferences.PreferenceConstants;

public class ResolveLibsHandler extends AbstractHandler {

    private static Logger logger = LoggerFactory
            .getLogger(ResolveLibsHandler.class);

    @Override
    public boolean isEnabled() {
        return Activator.getDefault().getBranchClient() != null;
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {

        final Shell shell = HandlerUtil.getActiveShell(event);
        IProject project = EditorUtil.getProject();
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        final String email = store.getString(PreferenceConstants.P_EMAIL);
        final String authCookie = store.getString(PreferenceConstants.P_AUTH_COOKIE);
        if (project != null) {
            final IFolder contentRoot = EditorUtil.getContentRoot(project);
            Job job = new Job("fetch") {
                @Override
                protected IStatus run(IProgressMonitor monitor) {
                    try {
                        BobUtil.resolveLibs(contentRoot, email, authCookie, monitor);
                    } catch (Throwable e) {
                        final String msg = e.getMessage();
                        final Status status = new Status(IStatus.ERROR, "com.dynamo.cr", msg);
                        shell.getDisplay().asyncExec(new Runnable() {
                            @Override
                            public void run() {
                                ErrorDialog.openError(null, "Error occurred while fetching libraries", "Unable to fetch libraries", status);
                            }
                        });
                        logger.error("Unable to fetch libraries", e);
                    }
                    return Status.OK_STATUS;
                }
            };

            job.schedule();
        }

        return null;
    }
}
