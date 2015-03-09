package com.dynamo.cr.editor.core.logging;

import org.eclipse.core.runtime.ILog;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.Status;
import org.osgi.framework.Bundle;

import ch.qos.logback.classic.Level;
import ch.qos.logback.classic.spi.ILoggingEvent;
import ch.qos.logback.classic.spi.IThrowableProxy;
import ch.qos.logback.classic.spi.ThrowableProxy;
import ch.qos.logback.core.ConsoleAppender;

public class EclipseForwardingAppender extends ConsoleAppender<ILoggingEvent> {

    private Bundle bundle;
    private ILog log;

    public EclipseForwardingAppender() {
    }

    @Override
    protected void append(ILoggingEvent event) {
        if (!isStarted()) {
            return;
        }

        int severity = IStatus.ERROR;
        int levelInt = event.getLevel().levelInt;
        if (levelInt == Level.ERROR_INT) {
            severity = IStatus.ERROR;
        } else if (levelInt == Level.WARN_INT) {
            severity = IStatus.WARNING;
        } else {
            // We filter out all messages less than warning
            return;
        }

        IThrowableProxy proxy = event.getThrowableProxy();
        Throwable throwable = null;
        if (proxy instanceof ThrowableProxy) {
            ThrowableProxy p = (ThrowableProxy) proxy;
            throwable = p.getThrowable();
        }

        if (bundle == null) {
            bundle = Platform.getBundle("com.dynamo.cr.editor.core");
            if (bundle != null) {
                log = Platform.getLog(bundle);
            }
        }

        if (log != null) {
            Status status = new Status(severity, event.getLoggerName(), event.getFormattedMessage(), throwable);
            log.log(status);
        }
    }

}
