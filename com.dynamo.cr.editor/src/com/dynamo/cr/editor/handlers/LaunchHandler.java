package com.dynamo.cr.editor.handlers;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IncrementalProjectBuilder;
import org.eclipse.core.resources.ResourcesPlugin;
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

import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.dialogs.DialogUtil;
import com.dynamo.cr.editor.preferences.PreferenceConstants;
import com.dynamo.cr.protocol.proto.Protocol.LaunchInfo;

public class LaunchHandler extends AbstractHandler {

    private HashMap<String, String> variables;

     @Override
     public boolean isEnabled() {
         return Activator.getDefault().getBranchClient() != null;
     }

    private static final String PARM_MSG = "com.dynamo.crepo.commands.launch.type";

    IStatus launchGame() throws RepositoryException {
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        final String exe_name = store.getString(PreferenceConstants.P_APPLICATION);
        final String socks_proxy = store.getString(PreferenceConstants.P_SOCKSPROXY);
        final int socks_proxy_port = store.getInt(PreferenceConstants.P_SOCKSPROXYPORT);

        final File exe = new File(exe_name);

        if (!exe.exists()) {
            return new Status(IStatus.ERROR, Activator.PLUGIN_ID, String.format("Executable '%s' not found", exe_name));
        }

        final LaunchInfo launchInfo = Activator.getDefault().projectClient.getLaunchInfo();

        Thread thread = new Thread(new Runnable() {

            @Override
            public void run() {
                try {
                    String[] args = new String[launchInfo.getArgsCount()];

                    int i = 0;
                    for (String s : launchInfo.getArgsList()) {
                        args[i++] = substituteVariables(s);
                    }

                    Map<String, String> env = new HashMap<String, String>();
                    if (!socks_proxy.isEmpty())
                    {
                        env.put("DMSOCKS_PROXY", socks_proxy);
                        env.put("DMSOCKS_PROXY_PORT", Integer.toString(socks_proxy_port));
                    }

                    ProcessBuilder pb = new ProcessBuilder(args);
                    pb.redirectErrorStream(true);
                    pb.environment().putAll(env);

                    Process p = pb.start();
                    BufferedReader std = new BufferedReader(new InputStreamReader(p.getInputStream()));

                    MessageConsole console = findConsole("console");
                    console.activate();
                    console.clearConsole();
                    MessageConsoleStream stream = console.newMessageStream();

                    stream.println("DMSOCKS_PROXY=" + socks_proxy);
                    stream.println("DMSOCKS_PROXY_PORT=" + socks_proxy_port);
                    for (String s : args) {
                        stream.print(s + " ");
                    }
                    stream.println("");

                    String line = std.readLine();
                    while (line != null) {
                        stream.println(line);
                        line = std.readLine();
                    }

                    std.close();
                    stream.close();

                } catch (IOException e) {
                    DialogUtil.openErrorAsync("Error running game", "Error running game", e);
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

        this.variables = new HashMap<String, String>();
        this.variables.put("user", Long.toString(Activator.getDefault().userInfo.getId()));
        this.variables.put("branch", Activator.getDefault().activeBranch);
        this.variables.put("executable", store.getString(PreferenceConstants.P_APPLICATION));
        this.variables.put("project", Long.toString(Activator.getDefault().projectClient.getProjectId()));

        final IProject project = getActiveProject(event);

        Job job = new Job("Build") {
            @Override
            protected IStatus run(IProgressMonitor monitor) {
                try {
                    project.build(rebuild ? IncrementalProjectBuilder.FULL_BUILD : IncrementalProjectBuilder.INCREMENTAL_BUILD, monitor);
                    return launchGame();
                } catch (Throwable e) {
                    // Return "OK" here in order to avoid dialogs when the build fails
                    // We could perhaps check for a specific status value?
                    return Status.OK_STATUS;
                }
            }
        };
        job.schedule();
        return null;
    }
}
