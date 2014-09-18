package com.dynamo.cr.target.core;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.MalformedURLException;
import java.net.Socket;
import java.net.URL;
import java.util.concurrent.atomic.AtomicBoolean;

import org.apache.commons.io.IOUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.common.util.NetworkUtil;
import com.dynamo.cr.editor.core.IConsole;

public class LogClient extends Thread {
    private Socket logSocket;
    private InetSocketAddress logSocketAddress;
    // Number of times the socket connection has been attempted without success
    private int logSocketConnectionAttempts = 0;
    private IConsole console;
    private AtomicBoolean stopped = new AtomicBoolean(false);

    // Attempt to connect for 2 seconds in case of failure
    private static final int MAX_LOG_CONNECTION_ATTEMPTS = 20;

    private static Logger logger = LoggerFactory.getLogger(LogClient.class);

    public LogClient(IConsole console) {
        this.console = console;
    }

    // Resets existing log socket, if any.
    public synchronized void resetLogSocket(ITarget targetToLaunch) {
        if (this.logSocket != null) {
            try {
                this.logSocket.close();
            } catch (IOException e) {
                logger.warn("Log socket could not be closed: {}", e.getMessage());
            }
        }
        this.logSocket = null;
        this.logSocketConnectionAttempts = 0;

        if (targetToLaunch != null) {
            this.logSocketAddress = findLogSocketAddress(targetToLaunch);
        } else {
            logSocketAddress = null;
        }
    }

    public void stopLog() {
        this.stopped.set(true);
        this.interrupt();
        try {
            this.join();
        } catch (InterruptedException e) {
            logger.error("Failed to join log client thread", e);
        }
    }

    private InetSocketAddress findLogSocketAddress(ITarget target) {
        // Ignore the address if it's local
        // Local targets are already logged from the process streams instead of a socket
        if (!isTargetLocal(target)) {
            try {
                URL url = new URL(target.getUrl());
                String host = url.getHost();
                return new InetSocketAddress(host, target.getLogPort());
            } catch (MalformedURLException e) {
                logger.warn("Could not open log socket: {}", e.getMessage());
            }
        }
        return null;
    }

    private boolean isTargetLocal(ITarget target) {
        if (target.getUrl() == null) {
            return true;
        } else {
            // Check if the target address is a local network interface
            try {
                if (NetworkUtil.getValidHostAddresses().contains(target.getInetAddress()))
                    return true;
            } catch (Exception e) {
                logger.warn("Could not retrieve host addresses: {}", e.getMessage());
            }
        }
        return false;
    }

    // Obtain a socket connection to the specified address
    private static Socket newLogConnection(InetSocketAddress address) {
        Socket socket = new Socket();

        InputStream is = null;
        try {
            socket.setSoTimeout(2000);
            socket.connect(address);

            StringBuilder sb = new StringBuilder();
            is = socket.getInputStream();
            int c = is.read();
            while (c != '\n' && c != -1) {
                if (c != '\r') {
                    sb.append((char) c);
                }
                c = is.read();
            }
            if (!sb.toString().equals("0 OK")) {
                throw new IOException(String.format("Unable to connect to log-service (%s)", sb.toString()));
            }
            socket.setSoTimeout(0);
            return socket;
        } catch (IOException e) {
            IOUtils.closeQuietly(is);
            IOUtils.closeQuietly(socket);
            logger.debug("Could not create log connection: {}", e.getMessage());
        }
        return null;
    }

    // Make one attempt to connect to the log socket.
    // If the maximum number of retries is reached, the stored address is reset to avoid further connections.
    private void connectLogSocket() {
        if (this.logSocketAddress != null && this.logSocket == null) {
            this.logSocket = newLogConnection(this.logSocketAddress);
            if (this.logSocket == null) {
                ++this.logSocketConnectionAttempts;
                if (this.logSocketConnectionAttempts >= MAX_LOG_CONNECTION_ATTEMPTS) {
                    logger.error("Terminally giving up log connection after max tries.");
                    this.logSocketAddress = null;
                }
            }
        }
    }

    private synchronized void updateLog() {
        // Close stale socket connections
        if (this.logSocket != null && !this.logSocket.isConnected()) {
            resetLogSocket(null);
        }
        // Check to see if we should make a new connection
        connectLogSocket();
        // Read from the socket when available
        if (this.logSocket != null && this.logSocket.isConnected()) {
            try {
                byte[] data = null;
                OutputStream out = null;
                InputStream in = this.logSocket.getInputStream();
                int available = in.available();
                while (available > 0) {
                    if (out == null) {
                        out = console.createOutputStream();
                        data = new byte[128];
                    }
                    int count = Math.min(available, data.length);
                    in.read(data, 0, count);
                    out.write(data, 0, count);
                    available = in.available();
                }
                if (out != null) {
                    out.close();
                }
            } catch (IOException e) {
                logger.error(e.getMessage(), e);
                resetLogSocket(null);
            }
        }
    }

    @Override
    public void run() {
        while (!stopped.get()) {
            try {
                Thread.sleep(100);
                updateLog();
            } catch (InterruptedException e) {}
        }
    }

}
