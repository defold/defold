package com.dynamo.cr.editor.builders;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IMarker;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceStatus;
import org.eclipse.core.resources.IncrementalProjectBuilder;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.IPageLayout;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.PartInitException;
import org.osgi.framework.Bundle;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.LibraryException;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.OsgiResourceScanner;
import com.dynamo.bob.OsgiScanner;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.BobUtil;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.ui.ViewUtil;

public class ContentBuilder extends IncrementalProjectBuilder {

    private static Logger logger = LoggerFactory
            .getLogger(ContentBuilder.class);
    private IBranchClient branchClient;

    public ContentBuilder() {
    }

    private void showProblemsView() {
        Display.getDefault().asyncExec(new Runnable() {
            @Override
            public void run() {
                try {
                    Activator.getDefault().getWorkbench().getActiveWorkbenchWindow().getActivePage().showView(IPageLayout.ID_PROBLEM_VIEW, null, IWorkbenchPage.VIEW_VISIBLE);
                } catch (PartInitException pe) {
                    // Unexpected
                    Activator.logException(pe);
                }
            }
        });
    }

    @Override
    protected IProject[] build(int kind, Map<String,String> args, IProgressMonitor monitor)
            throws CoreException {

        branchClient = Activator.getDefault().getBranchClient();
        if (branchClient == null)
            return null;

        getProject().deleteMarkers(IMarker.PROBLEM, true, IResource.DEPTH_INFINITE);

        boolean ret = false;
        ret = buildLocal(kind, args, monitor);
        if (!ret) {
            showProblemsView();
        } else {
            ViewUtil.showConsole();
        }
        return null;
    }

    private boolean buildLocal(int kind, Map<String,String> args, final IProgressMonitor monitor) throws CoreException {
        String branchLocation = branchClient.getNativeLocation();

        // NOTE: Bundle tasks rely on this structure (build/default)
        String buildDirectory = String.format("build/default");
        Project project = new Project(new DefaultFileSystem(), branchLocation, buildDirectory);

        Map<String, String> bobArgs = BobUtil.getBobArgs(args);
        if (bobArgs != null) {
            for (Entry<String, String> e : bobArgs.entrySet()) {
                project.setOption(e.getKey(), e.getValue());
            }
        }

        Bundle bundle = Activator.getDefault().getBundle();
        OsgiScanner scanner = new OsgiScanner(bundle);
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");

        Set<String> skipDirs = new HashSet<String>(Arrays.asList(".git", buildDirectory, ".internal"));

        List<String> commandList = new ArrayList<String>();
        if (kind == IncrementalProjectBuilder.FULL_BUILD) {
            commandList.add("distclean");
            commandList.add("build");
        } else if (kind == IncrementalProjectBuilder.CLEAN_BUILD) {
            commandList.add("distclean");
        } else {
            commandList.add("build");
        }

        ArrayList<String> extraCommands = BobUtil.getBobCommands(args);
        if (extraCommands != null) {
            commandList.addAll(extraCommands);
        }

        String[] commands = commandList.toArray(new String[commandList.size()]);

        boolean ret = true;
        try {
            project.createPublisher(project.hasOption("liveupdate"));

            project.setLibUrls(BobUtil.getLibraryUrls(branchLocation));
            project.mount(new OsgiResourceScanner(Platform.getBundle("com.dynamo.cr.builtins")));
            project.findSources(branchLocation, skipDirs);
            List<TaskResult> result = project.build(new ProgressDelegate(monitor), commands);
            for (TaskResult taskResult : result) {
                if (!taskResult.isOk()) {
                    ret = false;
                    Throwable exception = taskResult.getException();
                    if (exception != null) {
                        // This is an unexpected error. Log it.
                        Activator.logException(exception);
                    }
                    Task<?> task = taskResult.getTask();
                    String input = task.input(0).getPath();
                    IFile resource = EditorUtil.getContentRoot(getProject()).getFile(input);
                    while (!resource.exists() && task.getProductOf() != null) {
                        task = task.getProductOf();
                        input = task.input(0).getPath();
                        resource = EditorUtil.getContentRoot(getProject()).getFile(input);
                    }
                    if (resource.exists())
                    {
                        IMarker marker = resource.createMarker(IMarker.PROBLEM);
                        marker.setAttribute(IMarker.MESSAGE, taskResult.getMessage());
                        marker.setAttribute(IMarker.LINE_NUMBER, taskResult.getLineNumber());
                        marker.setAttribute(IMarker.SEVERITY, IMarker.SEVERITY_ERROR);
                    }
                    else {
                        logger.warn("Unable to locate: " + resource.getFullPath());
                    }
                }
            }
        } catch (IOException e) {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, IResourceStatus.BUILD_FAILED, "Build failed", e));
        } catch (LibraryException e) {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, IResourceStatus.BUILD_FAILED, "Build failed", e));
        } catch (CompileExceptionError e) {
            if (e.getResource() != null && e.getResource().exists()) {
                ret = false;
                IFile resource = EditorUtil.getContentRoot(getProject()).getFile(e.getResource().getPath());
                IMarker marker = resource.createMarker(IMarker.PROBLEM);
                marker.setAttribute(IMarker.MESSAGE, e.getMessage());
                marker.setAttribute(IMarker.LINE_NUMBER, e.getLineNumber());
                marker.setAttribute(IMarker.SEVERITY, IMarker.SEVERITY_ERROR);
            } else {
                throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, IResourceStatus.BUILD_FAILED, "Build failed: " + e.getMessage(), e));
            }
        } catch (MultipleCompileException e) {
            for (MultipleCompileException.Info info : e.errors) {
                ret = false;
                IFile resource = EditorUtil.getContentRoot(getProject()).getFile(info.getResource().getPath());
                IMarker marker = resource.createMarker(IMarker.PROBLEM);
                marker.setAttribute(IMarker.MESSAGE, info.getMessage());
                marker.setAttribute(IMarker.LINE_NUMBER, info.getLineNumber());
                marker.setAttribute(IMarker.SEVERITY, IMarker.SEVERITY_ERROR);
            }
            for (MultipleCompileException.Info info : e.warnings) {
                ret = false;
                IFile resource = EditorUtil.getContentRoot(getProject()).getFile(info.getResource().getPath());
                IMarker marker = resource.createMarker(IMarker.PROBLEM);
                marker.setAttribute(IMarker.MESSAGE, info.getMessage());
                marker.setAttribute(IMarker.LINE_NUMBER, info.getLineNumber());
                marker.setAttribute(IMarker.SEVERITY, IMarker.SEVERITY_WARNING);
            }
        } finally {
            project.dispose();
        }
        return ret;
    }
}
