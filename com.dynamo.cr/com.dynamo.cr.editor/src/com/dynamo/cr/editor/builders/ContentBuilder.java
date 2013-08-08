package com.dynamo.cr.editor.builders;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.StringReader;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.apache.commons.lang.SerializationUtils;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IMarker;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceStatus;
import org.eclipse.core.resources.IncrementalProjectBuilder;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.OperationCanceledException;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.IPageLayout;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.PartInitException;
import org.osgi.framework.Bundle;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.DefaultFileSystem;
import com.dynamo.bob.OsgiScanner;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.TaskResult;
import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.ui.ViewUtil;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc.Activity;
import com.dynamo.cr.protocol.proto.Protocol.BuildLog;
import com.sun.jersey.core.util.Base64;

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
        if (args.get("location").equals("remote")) {
            ret = buildRemote(kind, args, monitor);
        } else {
            ret = buildLocal(kind, args, monitor);
        }
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

        /*
         * bob args is a serialized HashMap stored in bobArgs
         */
        String bobArgsEncoded = args.get("bobArgs");
        if (bobArgsEncoded != null) {
            @SuppressWarnings("unchecked")
            HashMap<String, String> bobArgs = (HashMap<String, String>) SerializationUtils.deserialize(Base64.decode(bobArgsEncoded));
            for (Entry<String, String> e : bobArgs.entrySet()) {
                project.setOption(e.getKey(), e.getValue());
            }
        }

        Bundle bundle = Activator.getDefault().getBundle();
        OsgiScanner scanner = new OsgiScanner(bundle);
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");

        Set<String> skipDirs = new HashSet<String>(Arrays.asList(".git", buildDirectory));

        String[] commands;
        if (kind == IncrementalProjectBuilder.FULL_BUILD) {
            commands = new String[] { "distclean", "build" };
        } else if (kind == IncrementalProjectBuilder.CLEAN_BUILD) {
            commands = new String[] { "distclean" };
        } else {
            commands = new String[] { "build" };
        }

        boolean ret = true;
        try {
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
        } catch (CompileExceptionError e) {
            if (e.getResource() != null && e.getResource().exists()) {
                ret = false;
                IFile resource = EditorUtil.getContentRoot(getProject()).getFile(e.getResource().getPath());
                IMarker marker = resource.createMarker(IMarker.PROBLEM);
                marker.setAttribute(IMarker.MESSAGE, e.getMessage());
                marker.setAttribute(IMarker.LINE_NUMBER, e.getLineNumber());
                marker.setAttribute(IMarker.SEVERITY, IMarker.SEVERITY_ERROR);
            } else {
                throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, IResourceStatus.BUILD_FAILED, "Build failed", e));
            }
        }
        return ret;
    }

    private boolean buildRemote(int kind, Map<String,String> args, IProgressMonitor monitor)
            throws CoreException {

        boolean result = false;
        try {
            BuildDesc build = branchClient.build(kind == IncrementalProjectBuilder.FULL_BUILD);

            boolean taskStarted = false;
            int lastProgress = 0;
            int totalWorked = 0;

            while (true) {
                build = branchClient.getBuildStatus(build.getId());

                if (!taskStarted && build.getWorkAmount() != -1) {
                    monitor.beginTask("Building...", build.getWorkAmount());
                    taskStarted = true;
                }

                if (build.getProgress() != -1) {
                    int work = build.getProgress() - lastProgress;
                    totalWorked += work;
                    lastProgress = build.getProgress();
                    monitor.worked(work);
                    taskStarted = true;
                }

                if (build.getBuildActivity() == Activity.IDLE || monitor.isCanceled()) {
                    break;
                }

                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                }
            }

            if (monitor.isCanceled()) {
                branchClient.cancelBuild(build.getId());
                throw new OperationCanceledException();
            }

            if (build.getWorkAmount() != -1) {
                int work = build.getProgress() - totalWorked;
                totalWorked += work;
                monitor.worked(work);
            }
            monitor.done();

            if (build.getBuildResult() != BuildDesc.Result.OK) {
                BuildLog logs = branchClient.getBuildLogs(build.getId());

                String stdErr = logs.getStdErr();
                parseLog(stdErr);
            }
            else
            {
                result = true;
            }

        } catch (RepositoryException e1) {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, IResourceStatus.BUILD_FAILED, "Build failed", e1));
        } catch (IOException e1) {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, IResourceStatus.BUILD_FAILED, "Build failed", e1));
        }

        return result;
    }

    private void parseLog(String stdErr)
            throws IOException, CoreException {
        BufferedReader r = new BufferedReader(new StringReader(stdErr));
        String line = r.readLine();

        // Patterns for error parsing
        // Group convention:
        // 1: Filename
        // 2: Line number
        // 3: Message
        Pattern[] pattens = new Pattern[] {
            // ../content/models/box.model:4:29 : Message type "dmModelDDF.ModelDesc" has no field named "Textures2".
            Pattern.compile("(.*?):(\\d+):\\d+[ ]?:[ ]*(.*)"),

            //../content/models/box.model:0: error: is missing dependent resource file materials/simple.material2
            Pattern.compile("(.*?):(\\d+):[ ]*(.*)")
        };
        while (line != null) {
            line = line.trim();

            for (Pattern p : pattens) {
                Matcher m = p.matcher(line);
                if (m.matches()) {
                    // NOTE: We assume that the built file is always relative to from the build folder,
                    // ie ../content/... This is how waf works.
                    IFile resource = EditorUtil.getContentRoot(getProject()).getFolder("build").getFile(m.group(1));
                    if (resource.exists())
                    {
                        IMarker marker = resource.createMarker(IMarker.PROBLEM);
                        marker.setAttribute(IMarker.MESSAGE, m.group(3));
                        marker.setAttribute(IMarker.LINE_NUMBER, Integer.parseInt(m.group(2)));
                        marker.setAttribute(IMarker.SEVERITY, IMarker.SEVERITY_ERROR);
                    }
                    else {
                        logger.warn("Unable to locate: " + resource.getFullPath());
                    }
                    break; // for (Pattern...)
                }
            }
            line = r.readLine();
        }
    }
}
