package com.dynamo.cr.editor.handlers;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IMarker;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IncrementalProjectBuilder;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.core.runtime.jobs.Job;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.console.ConsolePlugin;
import org.eclipse.ui.console.IConsole;
import org.eclipse.ui.console.IConsoleManager;
import org.eclipse.ui.console.MessageConsole;
import org.eclipse.ui.console.MessageConsoleStream;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.internal.ide.actions.BuildUtilities;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.preferences.PreferenceConstants;
import com.dynamo.cr.engine.Engine;

// this suppression is for the usage of BuildUtilities in the bottom of this class
@SuppressWarnings("restriction")
public class LaunchHandler extends AbstractHandler {

    private HashMap<String, String> variables;

     @Override
     public boolean isEnabled() {
         return Activator.getDefault().getBranchClient() != null;
     }

    private static final String PARM_MSG = "com.dynamo.crepo.commands.launch.type";

    IStatus launchGame() throws RepositoryException {
        final IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        final String socks_proxy = store.getString(PreferenceConstants.P_SOCKS_PROXY);
        final int socks_proxy_port = store.getInt(PreferenceConstants.P_SOCKS_PROXY_PORT);
        final boolean localBranch = store.getBoolean(PreferenceConstants.P_USE_LOCAL_BRANCHES);

        final List<String> argList = new ArrayList<String>();
        if (localBranch) {
            argList.add("{executable}");
        } else {
            argList.addAll(Activator.getDefault().projectClient.getLaunchInfo().getArgsList());
        }

        Thread thread = new Thread(new Runnable() {

            private String[] createArgs() throws IOException {
                if (store.getBoolean(PreferenceConstants.P_RUN_IN_DEBUGGER)) {
                    File gdbCommandsFile = File.createTempFile("gdb", "commands");
                    gdbCommandsFile.deleteOnExit();
                    PrintWriter gdbCommandsWriter = new PrintWriter(gdbCommandsFile);

                    gdbCommandsWriter.println("set $_exitcode = -999");
                    if (store.getBoolean(PreferenceConstants.P_AUTO_RUN_DEBUGGER)) {
                        gdbCommandsWriter.println("run");
                    }
                    gdbCommandsWriter.println("if $_exitcode != -999");
                    gdbCommandsWriter.println("  quit");
                    gdbCommandsWriter.println("end");
                    gdbCommandsWriter.close();

                    String command = String.format("gdb -x %s --args ", gdbCommandsFile.getAbsolutePath());
                    for (String s : argList) {
                        command += substituteVariables(s);
                        command += " ";
                    }

                    String[] args;
                    if (EditorCorePlugin.getPlatform().equals("darwin")) {
                        if (socks_proxy.length() > 0) {
                            command = "DMSOCKS_PROXY=" + socks_proxy + " " + command;
                            command = "DMSOCKS_PROXY_PORT=" + socks_proxy_port + " " + command;
                        }
                        command += " && exit";
                        args = new String[] {"osascript", "-e", "tell application \"Terminal\"\n do script \"" + command + '"' + "\nend tell"};
                    } else {
                        args = new String[] {"xterm", "-e", command};
                    }

                    return args;

                } else {
                    String[] args = new String[argList.size()];
                    int i = 0;
                    for (String s : argList) {
                        args[i++] = substituteVariables(s);
                    }
                    return args;

                }
            }

            @Override
            public void run() {
                try {
                    String[] args = createArgs();

                    Map<String, String> env = new HashMap<String, String>();
                    if (!socks_proxy.isEmpty())
                    {
                        env.put("DMSOCKS_PROXY", socks_proxy);
                        env.put("DMSOCKS_PROXY_PORT", Integer.toString(socks_proxy_port));
                    }

                    IBranchClient branchClient = Activator.getDefault().getBranchClient();
                    IPreferenceStore store = Activator.getDefault().getPreferenceStore();

                    ProcessBuilder pb = new ProcessBuilder(args);
                    if (store.getBoolean(PreferenceConstants.P_USE_LOCAL_BRANCHES)) {
                        pb.directory(new File(branchClient.getNativeLocation()));
                    }

                    pb.redirectErrorStream(true);
                    pb.environment().putAll(env);

                    Process p = pb.start();
                    BufferedReader std = new BufferedReader(new InputStreamReader(p.getInputStream()));

                    MessageConsole console = findConsole("console");
                    console.activate();
                    console.clearConsole();
                    MessageConsoleStream stream = console.newMessageStream();

                    // Dump env and exe args when debugging
                    if (System.getProperty("osgi.dev") != null) {
                        stream.println("DMSOCKS_PROXY=" + socks_proxy);
                        stream.println("DMSOCKS_PROXY_PORT=" + socks_proxy_port);
                        for (String s : args) {
                            stream.print(s + " ");
                        }
                        stream.println("");
                    }

                    String line = std.readLine();
                    while (line != null) {
                        stream.println(line);
                        line = std.readLine();
                    }

                    std.close();
                    stream.close();

                } catch (IOException e) {
                    Activator.showError("Error occurred while running game", e);
                }
            }
        });
        thread.start();
        return Status.OK_STATUS;
    }

    String substituteVariables(String string) {
        for (Entry<String, String> entry : variables.entrySet()) {
            String key = String.format("{%s}", entry.getKey());
            string = string.replace(key, entry.getValue());
        }
        return string;
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

    IProject getActiveProject(ExecutionEvent event) {
        IEditorPart editor = HandlerUtil.getActiveEditor(event);
        if (editor != null) {
            IEditorInput input = editor.getEditorInput();
            if (input instanceof IFileEditorInput) {
                IFileEditorInput file_input = (IFileEditorInput) input;
                final IProject project = file_input.getFile().getProject();
                return project;
            }
        }
        // else return first project
        return ResourcesPlugin.getWorkspace().getRoot().getProjects()[0];
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        String msg = event.getParameter(PARM_MSG);
        final boolean rebuild = msg != null && msg.equals("rebuild");

        IPreferenceStore store = Activator.getDefault().getPreferenceStore();

        String exeName;
        if (store.getBoolean(PreferenceConstants.P_CUSTOM_APPLICATION)) {
            exeName = store.getString(PreferenceConstants.P_APPLICATION);
        } else {
            exeName = Engine.getDefault().getEnginePath();
            String platform = EditorCorePlugin.getPlatform();
            if (!platform.equals("win32")) {
                try {
                    Runtime.getRuntime().exec("chmod +x " + exeName);
                } catch (IOException e) {
                    return new Status(IStatus.ERROR, Activator.PLUGIN_ID, String.format("'%s' could not be made executable.", exeName));
                }
            }
        }
        final File exe = new File(exeName);

        if (!exe.exists()) {
            return new Status(IStatus.ERROR, Activator.PLUGIN_ID, String.format("Executable '%s' could not be found.", exeName));
        }

        this.variables = new HashMap<String, String>();
        this.variables.put("user", Long.toString(Activator.getDefault().userInfo.getId()));
        this.variables.put("branch", Activator.getDefault().activeBranch);
        this.variables.put("executable", exeName);
        this.variables.put("project", Long.toString(Activator.getDefault().projectClient.getProjectId()));

        final IProject project = getActiveProject(event);

        // save all editors depending on user preferences (this is set to true by default in plugin_customization.ini)
        BuildUtilities.saveEditors(null);

        Job job = new Job("Build") {
            @Override
            protected IStatus run(IProgressMonitor monitor) {
                Map<String, String> args = new HashMap<String, String>();
                final IPreferenceStore store = Activator.getDefault().getPreferenceStore();
                final boolean localBranch = store.getBoolean(PreferenceConstants.P_USE_LOCAL_BRANCHES);
                if (localBranch)
                    args.put("location", "local");
                else
                    args.put("location", "remote");

                try {
                    project.build(rebuild ? IncrementalProjectBuilder.FULL_BUILD : IncrementalProjectBuilder.INCREMENTAL_BUILD,  "com.dynamo.cr.editor.builders.contentbuilder", args, monitor);
                    int severity = project.findMaxProblemSeverity(IMarker.PROBLEM, true, IResource.DEPTH_INFINITE);
                    if (severity < IMarker.SEVERITY_ERROR) {
                        return launchGame();
                    } else {
                        return Status.OK_STATUS;
                    }
                } catch (CoreException e) {
                    return e.getStatus();
                } catch (RepositoryException e) {
                    return new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Could not launch game.", e);
                }
            }
        };
        job.schedule();
        return null;
    }
}
