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
import org.junit.Test;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

public class StateTest {
    private static final int WORKERS = 4;
    private static final int OUTPUTS_PER_WORKER = 128;
    private static final String LEGACY_PATH = "/build/legacy.oggc";
    private static final byte[] LEGACY_SIGNATURE = {1, 2, 3};

    // Parallel build workers must not lose signatures or produce unreadable state files.
    @Test
    public void testConcurrentSignaturesSurviveSaveAndLoad() throws Exception {
        assertConcurrentRoundTrips(null);
    }

    // Old HashMap-backed caches must retain their signatures and support parallel builds after loading.
    @Test
    public void testLegacyStateSupportsConcurrentUpdates() throws Exception {
        State legacyState = new State();
        Map<String, byte[]> legacySignatures = new HashMap<>();
        legacySignatures.put(LEGACY_PATH, LEGACY_SIGNATURE);
        Field signatures = State.class.getDeclaredField("signatures");
        signatures.setAccessible(true);
        signatures.set(legacyState, legacySignatures);

        IResource resource = new MockFileSystem().get("_BobBuildState_");
        legacyState.save(resource);
        assertConcurrentRoundTrips(resource.getContent());
    }

    private static String outputPath(int worker, int output) {
        return "/build/" + worker + "/" + output + ".compiled";
    }

    private static byte[] signature(int worker, int output) {
        return new byte[]{(byte) worker, (byte) output};
    }

    private static void assertSignatures(State state, boolean legacy) {
        Set<String> expectedPaths = new HashSet<>();
        if (legacy) {
            assertArrayEquals(LEGACY_SIGNATURE, state.getSignature(LEGACY_PATH));
            expectedPaths.add(LEGACY_PATH);
        }
        for (int worker = 0; worker < WORKERS; worker++) {
            for (int output = 0; output < OUTPUTS_PER_WORKER; output++) {
                String path = outputPath(worker, output);
                if (output % 2 == 0) {
                    assertNull(path, state.getSignature(path));
                } else {
                    assertArrayEquals(path, signature(worker, output), state.getSignature(path));
                    expectedPaths.add(path);
                }
            }
        }
        assertEquals(expectedPaths, new HashSet<>(state.getPaths()));
    }

    private static void assertConcurrentRoundTrips(byte[] initialContent) throws Exception {
        ExecutorService executor = Executors.newFixedThreadPool(WORKERS);
        try {
            // Repeated fresh maps exercise resizing while workers insert and remove outputs.
            for (int round = 0; round < 100; round++) {
                IResource resource = new MockFileSystem().get("_BobBuildState_");
                if (initialContent != null) {
                    resource.setContent(initialContent);
                }
                State state = State.load(resource);
                CountDownLatch ready = new CountDownLatch(WORKERS);
                CountDownLatch start = new CountDownLatch(1);
                List<Future<?>> futures = new ArrayList<>();
                for (int worker = 0; worker < WORKERS; worker++) {
                    final int workerIndex = worker;
                    futures.add(executor.submit(() -> {
                        ready.countDown();
                        start.await();
                        for (int output = 0; output < OUTPUTS_PER_WORKER; output++) {
                            String path = outputPath(workerIndex, output);
                            byte[] signature = signature(workerIndex, output);
                            state.putSignature(path, signature);
                            assertArrayEquals(path, signature, state.getSignature(path));
                            if (output % 2 == 0) {
                                state.removeSignature(path);
                            }
                        }
                        return null;
                    }));
                }
                assertTrue("Build workers did not become ready", ready.await(30, TimeUnit.SECONDS));
                start.countDown();
                for (Future<?> future : futures) {
                    future.get(30, TimeUnit.SECONDS);
                }
                assertSignatures(state, initialContent != null);
                state.save(resource);
                assertSignatures(State.load(resource), initialContent != null);
            }
        } finally {
            executor.shutdownNow();
        }
    }
}
