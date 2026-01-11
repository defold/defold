// Copyright 2020-2026 The Defold Foundation
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

import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.logging.LogManager;
import java.util.logging.Handler;

public class LogHelper {
    
    private static Level logLevel = Level.INFO;

    /**
     * Configure a Logger instance by adding a handler and setting a log level.
     * @param logger The logger instance to configure
     */
    public static void configureLogger(Logger logger) {
        // remove any existing handlers
        for (Handler h : logger.getHandlers()) {
            logger.removeHandler(h);
        }
        // add our own handler and set the log level
        Handler handler = new LogHandler();
        handler.setFormatter(new LogFormatter());
        logger.addHandler(handler);
        logger.setLevel(logLevel);
    }

    /**
     * Set the log level to use for all loggers in the com.dynamo.bob namespace
     * @param level The java.util.logging.Level to use
     */
    public static void setLogLevel(Level level) {
        logLevel = level;

        // set log level on existing loggers
        LogManager logManager = LogManager.getLogManager();
        logManager.getLoggerNames().asIterator().forEachRemaining(loggerName -> {
            if (loggerName.startsWith("com.dynamo.bob")) {
                Logger logger = logManager.getLogger(loggerName);
                if (logger != null) {
                    configureLogger(logger);
                }
            }
        });
    }

    /**
     * Enable or disable verbose logging
     * @param enable Set to true to enable verbose logging
     */
    public static void setVerboseLogging(boolean enabled) {
        setLogLevel(enabled ? Level.FINE : Level.INFO);
    }
}
