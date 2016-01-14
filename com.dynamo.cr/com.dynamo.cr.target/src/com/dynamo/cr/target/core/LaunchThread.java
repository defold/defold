package com.dynamo.cr.target.core;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;

import javax.ws.rs.core.UriBuilder;

import org.apache.commons.io.IOUtils;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;
import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.console.MessageConsole;
import org.eclipse.ui.console.MessageConsoleStream;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.ui.ViewUtil;
import com.dynamo.cr.engine.Engine;
import com.dynamo.engine.proto.Engine.Reboot;
import com.dynamo.engine.proto.Engine.Reboot.Builder;

/* package */class LaunchThread extends Thread {

    private ITarget target;
    private String location;
    private boolean runInDebugger;
    private boolean autoRunDebugger;
    private String[] arguments;
    private String socksProxy;
    private int socksProxyPort;
    private MessageConsoleStream stream;
    private URL serverUrl;

    public LaunchThread(ITarget target, String customApplication,
            String location, boolean runInDebugger, boolean autoRunDebugger,
            String socksProxy, int socksProxyPort, URL serverUrl) {
        this.target = target;
        this.location = location;
        this.serverUrl = serverUrl;

        if (customApplication != null)
            this.arguments = new String[] { customApplication };
        else
            this.arguments = new String[] { Engine.getDefault().getEnginePath() };

        this.runInDebugger = runInDebugger;
        this.autoRunDebugger = autoRunDebugger;
        this.socksProxy = socksProxy;
        this.socksProxyPort = socksProxyPort;
    }

    private String[] createArgs() throws IOException {
        if (runInDebugger) {
            File gdbCommandsFile = File.createTempFile("gdb", "commands");
            gdbCommandsFile.deleteOnExit();
            PrintWriter gdbCommandsWriter = new PrintWriter(gdbCommandsFile);

            gdbCommandsWriter.println("set $_exitcode = -999");
            if (autoRunDebugger) {
                gdbCommandsWriter.println("run");
            }
            gdbCommandsWriter.println("if $_exitcode != -999");
            gdbCommandsWriter.println("  quit");
            gdbCommandsWriter.println("end");
            gdbCommandsWriter.close();

            String command = String.format("gdb -x %s --args ",
                    gdbCommandsFile.getAbsolutePath());
            for (String s : arguments) {
                command += s;
                command += " ";
            }

            String[] args;
            if (EditorCorePlugin.getPlatform().equals("x86-darwin")) {
                if (socksProxy.length() > 0) {
                    command = "DMSOCKS_PROXY=" + socksProxy + " " + command;
                    command = "DMSOCKS_PROXY_PORT=" + socksProxyPort + " "
                            + command;
                }
                command += " && exit";
                args = new String[] {
                        "osascript",
                        "-e",
                        "tell application \"Terminal\"\n do script \""
                                + command + '"' + "\nend tell" };
            } else {
                args = new String[] { "xterm", "-e", command };
            }

            return args;

        } else {
            return arguments;
        }
    }

    public void launchLocalExecutable() {
        try {
            String[] args = createArgs();

            Map<String, String> env = new HashMap<String, String>();
            if (!socksProxy.isEmpty()) {
                env.put("DMSOCKS_PROXY", socksProxy);
                env.put("DMSOCKS_PROXY_PORT", Integer.toString(socksProxyPort));
            }

            ProcessBuilder pb = new ProcessBuilder(args);
            pb.directory(new File(location));

            pb.redirectErrorStream(true);
            pb.environment().putAll(env);

            Process p = pb.start();
            BufferedReader std = new BufferedReader(new InputStreamReader(
                    p.getInputStream()));

            // Dump env and exe args when debugging
            if (System.getProperty("osgi.dev") != null) {
                stream.println("DMSOCKS_PROXY=" + socksProxy);
                stream.println("DMSOCKS_PROXY_PORT=" + socksProxyPort);
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

        } catch (IOException e) {
            stream.print(e.getMessage());
        }
    }

    private void reboot() throws IOException {
        String resourceUri;
        String projectUri;
        try {
            resourceUri = UriBuilder.fromUri(serverUrl.toURI()).path("/build/default").build().toString();
            projectUri = UriBuilder.fromUri(serverUrl.toURI()).path("/build/default/game.projectc").build().toString();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        Builder builder = com.dynamo.engine.proto.Engine.Reboot.newBuilder();

        Reboot reboot = builder
                .setArg1("--config=resource.uri=" + resourceUri)
                .setArg2(projectUri)
                .build();
        URL url = UriBuilder.fromUri(target.getUrl())
                .path("post/@system/reboot").build().toURL();
        HttpURLConnection c = (HttpURLConnection) url.openConnection();
        c.setDoOutput(true);
        c.setRequestMethod("POST");
        OutputStream os = c.getOutputStream();
        os.write(reboot.toByteArray());
        os.close();

        InputStream is = new BufferedInputStream(c.getInputStream());
        int n = is.read();
        while (n != -1)
            n = is.read();
        is.close();
    }

    private boolean pingEngine(String engineUrl) {
        try {
            URL url = UriBuilder.fromUri(engineUrl).path("info").build().toURL();
            HttpURLConnection c = (HttpURLConnection) url.openConnection();
            InputStream is = c.getInputStream();
            ByteArrayOutputStream os = new ByteArrayOutputStream();
            IOUtils.copy(is, os);

            ObjectMapper m = new ObjectMapper();
            JsonNode infoNode = m.readValue(os.toByteArray(), JsonNode.class);
            JsonNode versionNode = infoNode.get("version");
            if (versionNode != null) {
                final String version = versionNode.asText();
                final String editorVersion = EditorCorePlugin.getDefault().getVersion();
                if (!(editorVersion.equals(version) || EditorUtil.isDev())) {
                    final Display display = Display.getDefault();
                    display.syncExec(new Runnable() {

                        @Override
                        public void run() {
                            String msg = String
                                    .format("Engine is out of date and must be updated to latest version %s. \nA local instance will be launched instead.",
                                            editorVersion);
                            MessageDialog.openError(display.getActiveShell(),
                                                    "Engine version mismatch",
                                                    msg);
                        }
                    });

                    return false;
                }
                return true;
            } else {
                return false;
            }
        } catch (IOException e) {
            return false;
        }
    }

    @Override
    public void run() {
        MessageConsole console = ViewUtil.getConsole();
        console.activate();
        console.clearConsole();
        stream = console.newMessageStream();

        try {
            if (target.getUrl() == null) {
                launchLocalExecutable();
            } else {
                if (pingEngine(target.getUrl())) {
                    reboot();
                } else {
                    // Fallback to launch local
                    launchLocalExecutable();
                }
            }
            stream.close();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            try {
                if (!stream.isClosed())
                    stream.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

}
