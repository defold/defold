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

package com.dynamo.bob.test;

import com.dynamo.bob.State;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.test.util.MockFileSystem;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import org.junit.Test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

public class StateTest {
    // Parallel compiler tasks must retain every signature and leave a readable incremental-build cache.
    @Test
    public void concurrentSignaturesSurviveReload() throws Exception {
        State state = new State();
        int workers = 8;
        int resources = 1000;
        CountDownLatch ready = new CountDownLatch(workers);
        CountDownLatch start = new CountDownLatch(1);
        try (var executor = Executors.newFixedThreadPool(workers)) {
            List<Future<?>> futures = new ArrayList<>();
            for (int worker = 0; worker < workers; ++worker) {
                String prefix = "/output/" + worker + "/";
                futures.add(executor.submit(() -> {
                    ready.countDown();
                    start.await();
                    for (int resource = 0; resource < resources; ++resource) {
                        String path = prefix + resource;
                        state.putSignature(path, path.getBytes(StandardCharsets.UTF_8));
                    }
                    return null;
                }));
            }
            ready.await();
            start.countDown();
            for (Future<?> future : futures) future.get();
        }
        IResource receipt = new MockFileSystem().get("_BobBuildState_");
        state.save(receipt);
        State reloaded = State.load(receipt);
        assertEquals(workers * resources, reloaded.getPaths().size());
        for (int worker = 0; worker < workers; ++worker) {
            for (int resource = 0; resource < resources; ++resource) {
                String path = "/output/" + worker + "/" + resource;
                assertArrayEquals(path.getBytes(StandardCharsets.UTF_8), reloaded.getSignature(path));
            }
        }
    }
}
