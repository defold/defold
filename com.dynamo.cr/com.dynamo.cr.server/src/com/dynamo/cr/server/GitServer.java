package com.dynamo.cr.server;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class GitServer {
    protected static Logger logger = LoggerFactory.getLogger(GitServer.class);

    private String crUrl;
    private String gitSrv;
    private String root;
    private int port;
    private Process process;

    private Consumer outConsumer;
    private Consumer errorConsumer;

    private static class Consumer extends Thread {
        private BufferedReader reader;

        public Consumer(InputStream stream) {
            reader = new BufferedReader(new InputStreamReader(stream));
        }

        @Override
        public void run() {
            String line = null;
            do {
                try {
                    line = reader.readLine();
                } catch (IOException e) {
                    return;
                }
                if (line != null) {
                    logger.error(line);
                }
            } while (line != null);
        }
    }

    public GitServer(String crUrl, String gitSrv, String root, int port) {
        this.crUrl = crUrl;
        this.gitSrv = gitSrv;
        this.root = root;
        this.port = port;
    }

    public void start() throws IOException {
        ProcessBuilder pb = new ProcessBuilder(gitSrv, "--port=" + port,
                "--repo-root=" + root, "--cr-url=" + crUrl);
        process = pb.start();

        outConsumer = new Consumer(process.getInputStream());
        outConsumer.start();

        errorConsumer = new Consumer(process.getErrorStream());
        errorConsumer.start();

        Thread shutdown = new Thread() {
            public void run() {
                if (process != null) {
                    process.destroy();
                }
            }
        };
        // Make sure that the gitsrv is shutdown
        Runtime.getRuntime().addShutdownHook(shutdown);
    }

    public void stop() {
        process.destroy();
        process = null;
        outConsumer.interrupt();
        errorConsumer.interrupt();
        try {
            outConsumer.join();
            errorConsumer.join();
        } catch (InterruptedException e) {
        }
    }
}
