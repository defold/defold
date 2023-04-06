// Copyright 2020-2023 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.logging;

import java.io.OutputStream;
import java.io.IOException;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.logging.LogRecord;
import java.util.logging.LogManager;
import java.util.logging.Handler;
import java.util.logging.StreamHandler;
import java.util.logging.ConsoleHandler;
import java.util.logging.Formatter;


public class LogHelper {

    /**
     * StreamHandler subclass which flushes the underlying stream after each
     * published log record
     */
    private static class AutoFlushingStreamHandler extends StreamHandler {

        public AutoFlushingStreamHandler(OutputStream out, Formatter formatter) {
            super(out, formatter);
        }

        public void changeOutputStream(OutputStream out) {
            super.setOutputStream(out);
        }

        @Override
        public synchronized void publish(final LogRecord record) {
            super.publish(record);
            flush();
        }

    }

    private static AutoFlushingStreamHandler logStreamHandler = null;

    private static Level logLevel = Level.INFO;

    /**
     * Configure a Logger instance. This will add a ConsoleHandler and
     * a StreamHandler if present. It will also set the log level.
     * @param logger The logger instance to configure
     */
    public static void configureLogger(Logger logger) {
        if (logStreamHandler != null) {
            logger.addHandler(logStreamHandler);
        }
        else {
            ConsoleHandler consoleHandler = new ConsoleHandler();
            consoleHandler.setFormatter(new LogFormatter());
            logger.addHandler(consoleHandler);
        }
        logger.setLevel(logLevel);
        logger.setUseParentHandlers(false);
    }

    /**
     * Set a log stream handler to use on all loggers.
     * Use this to send log records from bob to some other source, for instance
     * the Defold editor. If a log stream has already been set it will be
     * removed from all loggers.
     * @param out The output stream to write log records to
     */
    public static void setLogStream(OutputStream out) {
        removeLogStream();

        if (logStreamHandler == null) {
            logStreamHandler = new AutoFlushingStreamHandler(out, new LogFormatter());
        }
        else {
            logStreamHandler.changeOutputStream(out);
        }

        // apply stream handler to existing loggers
        LogManager logManager = LogManager.getLogManager();
        logManager.getLoggerNames().asIterator().forEachRemaining(loggerName -> {
            if (loggerName.startsWith("com.dynamo.bob")) {
                Logger logger = logManager.getLogger(loggerName);
                if (logger != null) {
                    logger.addHandler(logStreamHandler);
                }
            }
        });

        // add stream handler to root logger
        Logger rootLogger = logManager.getLogger("");
        if (rootLogger != null) {
            rootLogger.addHandler(logStreamHandler);
        }
    }

    /**
     * Remove the log stream handler from all loggers
     * Use this together with setLogStream()
     */
    public static void removeLogStream() {
        if (logStreamHandler == null) {
            return;
        }

        // remove stream handler from existing loggers
        LogManager logManager = LogManager.getLogManager();
        logManager.getLoggerNames().asIterator().forEachRemaining(loggerName -> {
            if (loggerName.startsWith("com.dynamo.bob")) {
                Logger logger = logManager.getLogger(loggerName);
                if (logger != null) {
                    logger.removeHandler(logStreamHandler);
                }
            }
        });

        // remove stream handler from root logger
        Logger rootLogger = logManager.getLogger("");
        if (rootLogger != null) {
            rootLogger.removeHandler(logStreamHandler);
        }
    }

    /**
     * Set the log level to use for all loggers
     * @param level The java.util.logging.Level to use
     */
    public static void setLogLevel(Level level) {
        LogHelper.logLevel = level;

        // set log level on existing loggers
        LogManager logManager = LogManager.getLogManager();
        logManager.getLoggerNames().asIterator().forEachRemaining(loggerName -> {
            if (loggerName.startsWith("com.dynamo.bob")) {
                Logger logger = logManager.getLogger(loggerName);
                if (logger != null) {
                    logger.setLevel(level);
                }
            }
        });
    }

    /**
     * Enable or disable verbose logging
     * @param enable Set to true to enable verbose logging
     */
    public static void setVerboseLogging(boolean enabled) {
        setLogLevel(enabled ? Level.CONFIG : Level.WARNING);
    }

}