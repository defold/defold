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

import java.util.Date;
import java.util.logging.Level;
import java.util.logging.LogRecord;
import java.util.logging.Formatter;


/**
 * Log formatter with format set from constructor instead of system
 * property.
 */
public class LogFormatter extends Formatter {
    private String format = null;
    
    public LogFormatter() {
        this("%1$tF %1$tT %4$-7s %5$s %n");
    }

    public LogFormatter(String format) {
        this.format = format;
    }

    @Override
    public String format(LogRecord record) {
        Date date = new Date(record.getMillis());
        String loggerName = record.getLoggerName();
        String methodName = record.getSourceMethodName();
        String source = (methodName != null) ? methodName : loggerName;
        Level level = record.getLevel();
        String message = formatMessage(record);
        Throwable thrown = record.getThrown();
        if (record.getParameters() != null) {
            message = String.format(message, record.getParameters());
        }
        return String.format(this.format, date, source, loggerName, level, message, thrown);
    }
}
