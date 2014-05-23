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
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.BobUtil;
import com.dynamo.cr.editor.core.EditorUtil;

public class ResolveLibsHandler extends AbstractHandler {

    private static Logger logger = LoggerFactory
            .getLogger(ResolveLibsHandler.class);

    @Override
    public boolean isEnabled() {
        return Activator.getDefault().getBranchClient() != null;
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {

        IProject project = EditorUtil.getProject();
        if (project != null) {
            final IFolder contentRoot = EditorUtil.getContentRoot(project);
            Job job = new Job("reload") {
                @Override
                protected IStatus run(IProgressMonitor monitor) {
                    try {
                        BobUtil.resolveLibs(contentRoot, monitor);
                    } catch (Throwable e) {
                        logger.warn(e.getMessage());
                    }
                    return Status.OK_STATUS;
                }
            };

            job.schedule();
        }

        return null;
    }
}
