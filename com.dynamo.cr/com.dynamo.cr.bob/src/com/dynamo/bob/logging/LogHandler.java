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

import java.util.logging.LogRecord;
import java.util.logging.Handler;
import java.util.logging.Formatter;


/**
 * Super basic log handler which publishes log records directly to
 * system out as soon as they are received.
 */
public class LogHandler extends Handler {
    public LogHandler() {
        super();
    }

    @Override
    public void publish(final LogRecord record) {
        Formatter f = getFormatter();
        if (f != null) {
            System.out.print(f.format(record));
        }
        else {
            System.out.println(record.getMessage());
        }
    }

    @Override
    public void close() throws SecurityException {}

    @Override
    public void flush() {}
}
