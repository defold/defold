package com.dynamo.cr.editor.handlers;

import java.io.BufferedInputStream;
import java.io.InputStream;
import java.net.URL;
import java.net.URLConnection;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IncrementalProjectBuilder;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.jobs.Job;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.editor.Activator;

public class ReloadResourceHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {

        final IEditorPart editor = HandlerUtil.getActiveEditor(event);
        if (editor != null)
        {
            IEditorInput input = editor.getEditorInput();
            if (input instanceof IFileEditorInput) {
                final IFileEditorInput file_input = (IFileEditorInput) input;
                IPath path = file_input.getFile().getFullPath();
                path = path.removeFirstSegments(3);
                final String string_path = path.toPortableString() + "c";
                editor.getSite().getPage().saveEditor(editor, false);

                Job job = new Job("reload") {
                    @Override
                    protected IStatus run(IProgressMonitor monitor) {
                        try {
                            file_input.getFile().getProject().build(IncrementalProjectBuilder.INCREMENTAL_BUILD, monitor);
                            URL url = new URL("http://localhost:8001/reload/" + string_path);
                            URLConnection c = url.openConnection();
                            InputStream is = new BufferedInputStream(c.getInputStream());
                            int n = is.read();
                            while (n != -1)
                                n = is.read();
                            is.close();

                        } catch (Throwable e) {
                            Activator.getDefault().logger.warning(e.getMessage());
                        }
                        return Status.OK_STATUS;
                    }
                };

                job.schedule();
            }
        }

        return null;
    }
}
