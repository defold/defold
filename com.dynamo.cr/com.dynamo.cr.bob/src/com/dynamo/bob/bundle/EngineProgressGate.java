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

package com.dynamo.bob.bundle;

import com.dynamo.bob.IProgress;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/// Holds back remote engine build messages while another build step owns the
/// progress message, so the two do not alternate in the status line.
///
/// Work is always forwarded immediately; only messages are deferred. Once the
/// gate is opened the latest deferred message is emitted and later ones pass through.
public final class EngineProgressGate {
    private final AtomicBoolean open = new AtomicBoolean(false);
    private final AtomicReference<IProgress.Message> latest = new AtomicReference<>();

    public void message(IProgress progress, IProgress.Message message) {
        latest.set(message);
        if (open.get()) {
            progress.message(message);
        }
    }

    public void open(IProgress progress) {
        open.set(true);
        IProgress.Message message = latest.get();
        if (message != null) {
            progress.message(message);
        }
    }
}
