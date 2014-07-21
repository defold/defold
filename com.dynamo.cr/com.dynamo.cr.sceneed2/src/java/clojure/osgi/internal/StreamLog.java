package clojure.osgi.internal;

import java.io.PrintStream;

import org.osgi.framework.ServiceReference;
import org.osgi.service.log.LogService;

public class StreamLog implements LogService {
    private final PrintStream stream;

    public StreamLog(PrintStream out) {
        this.stream = out;
    }

    @Override
    public void log(int severity, String message) {
        stream.println(String.format("%d - %s", severity, message));
    }

    @Override
    public void log(int severity, String message, Throwable exception) {
        log(severity, message);
        exception.printStackTrace(stream);
    }

    @SuppressWarnings("rawtypes")
    @Override
    public void log(ServiceReference sr, int level, String message) {
    }

    @SuppressWarnings("rawtypes")
    @Override
    public void log(ServiceReference sr, int level, String message, Throwable exception) {
    }
}
