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
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc.Activity;
import com.dynamo.cr.protocol.proto.Protocol.BuildLog;

public class ContentBuilder extends IncrementalProjectBuilder {

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
    protected IProject[] build(int kind, @SuppressWarnings("rawtypes") Map args, IProgressMonitor monitor)
            throws CoreException {
        MessageConsole console = findConsole("console");
        console.activate();
        MessageConsoleStream stream = console.newMessageStream();
        console.clearConsole();
        stream.println("building...");
        getProject().deleteMarkers(IMarker.PROBLEM, true, IResource.DEPTH_INFINITE);

        IBranchClient branch_client = Activator.getDefault().getBranchClient();
        if (branch_client == null)
            return null;

        try {
            BuildDesc build = branch_client.build(kind == IncrementalProjectBuilder.FULL_BUILD);

            boolean task_started = false;
            int last_progress = 0;
            int total_worked = 0;

            while (true) {
                build = branch_client.getBuildStatus(build.getId());

                if (!task_started && build.getWorkAmount() != -1) {
                    monitor.beginTask("Building...", build.getWorkAmount());
                    task_started = true;
                }

                if (build.getProgress() != -1) {
                    int work = build.getProgress() - last_progress;
                    total_worked += work;
                    last_progress = build.getProgress();
                    monitor.worked(work);
                    task_started = true;
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
                branch_client.cancelBuild(build.getId());
                throw new OperationCanceledException();
            }

            if (build.getWorkAmount() != -1) {
                int work = build.getProgress() - total_worked;
                total_worked += work;
                monitor.worked(work);
            }
            monitor.done();

            if (build.getBuildResult() != BuildDesc.Result.OK) {
                BuildLog logs = branch_client.getBuildLogs(build.getId());

                BufferedReader r = new BufferedReader(new StringReader(logs.getStdErr()));
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
                stream.println(logs.getStdErr());

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
}
