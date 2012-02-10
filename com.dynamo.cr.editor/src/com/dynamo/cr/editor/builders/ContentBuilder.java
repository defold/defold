package com.dynamo.cr.editor.builders;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.StringReader;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IMarker;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IncrementalProjectBuilder;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.OperationCanceledException;
import org.eclipse.core.runtime.Status;
import org.eclipse.ui.console.ConsolePlugin;
import org.eclipse.ui.console.IConsole;
import org.eclipse.ui.console.IConsoleManager;
import org.eclipse.ui.console.MessageConsole;
import org.eclipse.ui.console.MessageConsoleStream;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.common.util.CommandUtil;
import com.dynamo.cr.common.util.CommandUtil.IListener;
import com.dynamo.cr.common.util.CommandUtil.Result;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc.Activity;
import com.dynamo.cr.protocol.proto.Protocol.BuildLog;

public class ContentBuilder extends IncrementalProjectBuilder {

    private MessageConsoleStream stream;
    private IBranchClient branchClient;

    public ContentBuilder() {
    }

    private MessageConsole findConsole(String name) {
        ConsolePlugin plugin = ConsolePlugin.getDefault();
        IConsoleManager conMan = plugin.getConsoleManager();
        IConsole[] existing = conMan.getConsoles();
        for (int i = 0; i < existing.length; i++)
            if (name.equals(existing[i].getName()))
                return (MessageConsole) existing[i];
        // no console found, so create a new one
        MessageConsole myConsole = new MessageConsole(name, null);
        conMan.addConsoles(new IConsole[] { myConsole });
        return myConsole;
    }

    @Override
    protected IProject[] build(int kind, Map<String,String> args, IProgressMonitor monitor)
            throws CoreException {

        branchClient = Activator.getDefault().getBranchClient();
        if (branchClient == null)
            return null;

        MessageConsole console = findConsole("console");
        console.activate();
        stream = console.newMessageStream();
        console.clearConsole();
        stream.println("building...");
        getProject().deleteMarkers(IMarker.PROBLEM, true, IResource.DEPTH_INFINITE);

        IProject[] ret;
        if (args.get("location").equals("remote")) {
            ret = buildRemote(kind, args, monitor);
        } else {
            ret = buildLocal(kind, args, monitor);
        }
        try {
            stream.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return ret;
    }

    private IProject[] buildLocal(int kind, Map<String,String> args, final IProgressMonitor monitor) throws CoreException {
        String branchLocation = branchClient.getNativeLocation();
        String[] command;
        if (kind == IncrementalProjectBuilder.FULL_BUILD) {
            command = new String[] { "waf", "configure", "clean", "build" };

        } else {
            command = new String[] { "waf", "configure", "build" };
        }

        final Pattern workPattern = Pattern.compile("\\[\\s*(\\d+)/(\\d+)\\]");

        try {
            IListener listener = new IListener() {
                int lastProgress = 0;
                boolean taskStarted = false;

                @Override
                public void onStdErr(StringBuffer buffer) {
                    Matcher m = workPattern.matcher(buffer.toString());

                    int progress = -1;
                    int workAmount = -1;
                    while (m.find()) {
                        progress = Integer.valueOf(m.group(1));
                        workAmount = Integer.valueOf(m.group(2));
                    }

                    if (progress != -1) {
                        if (!taskStarted) {
                            monitor.beginTask("Building...", workAmount);
                            taskStarted = true;
                        }

                        int work = progress - lastProgress;
                        lastProgress = progress;
                        monitor.worked(work);
                        taskStarted = true;
                    }

                }

                @Override
                public void onStdOut(StringBuffer buffer) {}
            };
            Result r = CommandUtil.execCommand(branchLocation, null, listener, command);
            parseLog(r.stdErr.toString());
            stream.write(r.stdErr.toString());
            if (r.exitValue != 0) {
                throw new CoreException(new Status(IStatus.WARNING, Activator.PLUGIN_ID, "Build failed"));
            }
        } catch (IOException e) {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Error occurred while building", e));
        }
        return null;
    }

    private IProject[] buildRemote(int kind, Map<String,String> args, IProgressMonitor monitor)
            throws CoreException {


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
                stream.println(stdErr);

                throw new CoreException(new Status(IStatus.WARNING, Activator.PLUGIN_ID, "Build failed"));
            }
            else
            {
                stream.println("build finished successfully");
            }

        } catch (RepositoryException e1) {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Error occurred while building", e1));
        } catch (IOException e1) {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Error occurred while building", e1));
        }

        return null;
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
            stream.println(line);

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
                        Activator.getDefault().logger.warning("Unable to locate: " + resource.getFullPath());
                    }
                    break; // for (Pattern...)
                }
            }
            line = r.readLine();
        }
    }
}
