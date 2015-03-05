package com.dynamo.cr.editor.handlers;

import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IncrementalProjectBuilder;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.SubProgressMonitor;
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
import com.dynamo.cr.editor.BobUtil;
import com.dynamo.cr.editor.core.EditorUtil;

/**
 * Bundle handler
 * TODO: The AbstractBundleHandler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author chmu
 *
 */
public abstract class AbstractBundleHandler extends AbstractHandler {

    private static Logger logger = LoggerFactory.getLogger(AbstractBundleHandler.class);
    protected Shell shell;
    private IProjectClient projectClient;
    private String outputDirectory;

    protected abstract void setProjectOptions(Map<String, String> options);

    class BundleRunnable implements IRunnableWithProgress {
        private void buildProject(IProject project, int kind, IProgressMonitor monitor) throws CoreException {

            HashMap<String, String> bobArgs = new HashMap<String, String>();
            bobArgs.put("archive", "true");
            bobArgs.put("bundle-output", outputDirectory);
            bobArgs.put("texture-profiles", "true"); // Always use texture profiles when bundling
            setProjectOptions(bobArgs);

            Map<String, String> args = new HashMap<String, String>();
            args.put("location", "local");
            BobUtil.putBobArgs(bobArgs, args);
            ArrayList<String> commands = new ArrayList<String>();
            commands.add("bundle");
            BobUtil.putBobCommands(commands, args);
            project.build(kind,  "com.dynamo.cr.editor.builders.contentbuilder", args, monitor);
        }

        @Override
        public void run(IProgressMonitor monitor)
                throws InvocationTargetException, InterruptedException {
            IProject project = EditorUtil.getProject();
            monitor.beginTask("Bundling Application...", 11);
            try {
                buildProject(project, IncrementalProjectBuilder.FULL_BUILD, new SubProgressMonitor(monitor, 9));
                buildProject(project, IncrementalProjectBuilder.CLEAN_BUILD, new SubProgressMonitor(monitor, 1));
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

    protected boolean openBundleDialog() {
        return true;
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        projectClient = Activator.getDefault().projectClient;

        if (projectClient == null) {
            return null;
        }

        shell = HandlerUtil.getActiveShell(event);

        if (!openBundleDialog()) {
            return null;
        }

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

        return null;
    }

    @Override
    public boolean isEnabled() {
        return Activator.getDefault().projectClient != null;
    }
}
