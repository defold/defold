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

package com.dynamo.bob.archive.publisher;

import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.CompileExceptionError;
import java.io.InputStream;

public class NullPublisher extends Publisher {

    public NullPublisher(PublisherSettings settings) {
        super(settings);
    }

    public NullPublisher() {
        super(new PublisherSettings());
    }

    @Override
    public void start() throws CompileExceptionError {}

    @Override
    public void stop() throws CompileExceptionError {}

    @Override
    public void publish(ArchiveEntry archiveEntry, InputStream data) throws CompileExceptionError {}

    @Override
    public void publish(ArchiveEntry archiveEntry, byte[] data) throws CompileExceptionError {}

}
