package com.dynamo.cr.rlog;

import java.io.IOException;
import java.net.InetAddress;
import java.util.LinkedList;

import org.eclipse.core.runtime.ILogListener;
import org.eclipse.core.runtime.IStatus;
import org.joda.time.DateTime;
import org.joda.time.format.DateTimeFormatter;
import org.joda.time.format.ISODateTimeFormat;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.rlog.proto.RLog;
import com.dynamo.cr.rlog.proto.RLog.Record;
import com.dynamo.cr.rlog.proto.RLog.Severity;

public class RLogListener implements ILogListener, Runnable {

    public static final int RETAIN_COUNT = 20;
    public static final int MAX_RECORDS = 100;
    private int logRecordCount = 0;
    private static DateTimeFormatter isoFormat = ISODateTimeFormat.dateTime();
    private IRLogTransport transport = null;

    /*
     * NOTE: buffer must be synchronized on as buffer is
     * accessed from several threads.
     */
    private LinkedList<Entry> buffer = new LinkedList<Entry>();
    private boolean run = true;
    private Thread thread;
    private String localAddress;

    private static class Entry {
        private IStatus status;
        private String plugin;
        private DateTime date;

        public Entry(IStatus status, String plugin) {
            this.status = status;
            this.plugin = plugin;
            this.date = new DateTime();
        }
    }

    public RLogListener(IRLogTransport transport) {
        this.transport = transport;
        thread = new Thread(this);

        localAddress = "127.0.0.1";
        try {
            localAddress = InetAddress.getLocalHost().getHostAddress();
        } catch (Throwable e) {}
    }

    @Override
    public void logging(IStatus status, String plugin) {
        if (this.logRecordCount >= MAX_RECORDS) {
            // Too many records. Discard
            return;
        }

        /*
         * NOTE: buffer must be synchronized on
         */
        if (status.getSeverity() >= IStatus.WARNING) {
            synchronized (buffer) {
                buffer.add(new Entry(status, plugin));
                while (buffer.size() > RETAIN_COUNT) {
                    buffer.removeFirst();
                }
            }
        }
    }

    public void start() {
        thread.start();
    }

    public void stop() {
        run = false;
        thread.interrupt();
        try {
            thread.join();
        } catch (InterruptedException e) {}
    }

    public void process() {
        /*
         * NOTE: buffer must be synchronized on
         */
        Entry entry;
        boolean ok, removeEntry;
        do {
            entry = null;
            ok = false;
            removeEntry = false;
            synchronized (buffer) {
                // Try to fetch an entry
                if (buffer.size() > 0) {
                    entry = buffer.getFirst();
                    logRecordCount++;
                }
            }
            if (entry != null) {
                try {
                    transport.send(buildRecord(entry));
                    ok = true;
                    removeEntry = true;
                } catch (IOException e) {
                    // Transient error (hopefully)
                    // Keep entry for later retry
                    ok = false;
                    removeEntry = false;
                } catch (Throwable e) {
                    // Should not happen. Exceptions should be handled gracefully in send()
                    // Treated as a permanent error and the entry is removed
                    e.printStackTrace();
                    ok = false;
                    removeEntry = true;
                }
                synchronized (buffer) {
                    // Remove after send is complete
                    // in order to handle transient errors
                    if (removeEntry) {
                        buffer.remove(entry);
                    }
                }
            }
            // Keep on processing while send is ok
            // and buffer isn't empty
        }
        while (ok && entry != null);
    }

    @Override
    public void run() {
        while (run) {
            try {
                Thread.sleep(1000);
                process();
            } catch (InterruptedException e) {
            }
        }
    }

    private Record buildRecord(Entry entry) {
        Severity severity = Severity.UNKNOWN;
        switch (entry.status.getSeverity()) {
        case IStatus.OK:
            severity = Severity.OK;
            break;
        case IStatus.INFO:
            severity = Severity.INFO;
            break;
        case IStatus.WARNING:
            severity = Severity.WARNING;
            break;
        case IStatus.ERROR:
            severity = Severity.ERROR;
            break;
        }

        Record.Builder recordBuilder = RLog.Record.newBuilder()
                .setDate(isoFormat.print(entry.date))
                .setIp(localAddress)
                .setMessage(entry.status.getMessage())
                .setPlugin(entry.plugin)
                .setSeverity(severity)
                .setVersion(EditorCorePlugin.VERSION);

        Throwable e = entry.status.getException();
        if (e != null) {
            RLog.StackTrace.Builder stb = RLog.StackTrace.newBuilder();
            for (StackTraceElement ste : e.getStackTrace()) {
                String fileName = ste.getFileName();
                if (fileName == null) {
                    fileName = "unknown";
                }
                String className = ste.getClassName();
                if (className == null) {
                    className = "unknown";
                }
                stb.addElement(RLog.StackTraceElement.newBuilder()
                        .setFile(fileName)
                        .setJavaClass(className)
                        .setLine(ste.getLineNumber())
                        .setMethod(ste.getMethodName()));
            }
            String msg = e.getMessage();
            if (msg != null) {
                stb.setMessage(msg);
            } else {
                stb.setMessage("No exception message found");
            }
            recordBuilder.setStackTrace(stb);
        }

        return recordBuilder.build();
    }

}
