package com.dynamo.cr.editor.handlers;

import java.lang.reflect.InvocationTargetException;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.OperationCanceledException;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.operation.IRunnableWithProgress;
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
        final String authToken = store.getString(PreferenceConstants.P_AUTH_TOKEN);
        if (project != null) {
            final IFolder contentRoot = EditorUtil.getContentRoot(project);
            try {
                HandlerUtil.getActiveWorkbenchWindow(event).getWorkbench().getProgressService().busyCursorWhile(new IRunnableWithProgress() {

                    @Override
                    public void run(IProgressMonitor monitor) throws InvocationTargetException,
                            InterruptedException {
                        try {
                            monitor.beginTask("Fetch Libraries", IProgressMonitor.UNKNOWN);
                            BobUtil.resolveLibs(contentRoot, email, authToken, monitor);
                        } catch (OperationCanceledException e) {
                            // Normal, pass through
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
                        } finally {
                            monitor.done();
                        }

                    }
                });
            } catch (Exception e) {
                throw new ExecutionException(e.getMessage(), e);
            }
        }

        return null;
    }
}
