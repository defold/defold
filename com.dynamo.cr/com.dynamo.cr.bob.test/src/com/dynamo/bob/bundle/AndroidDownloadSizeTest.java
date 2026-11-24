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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import java.io.IOException;
import java.util.List;
import java.util.Map;

import org.junit.Test;

import com.fasterxml.jackson.databind.ObjectMapper;

public class AndroidDownloadSizeTest {

    // Output of `bundletool get-size total --dimensions=ABI`
    private static final String TWO_ABIS_OUTPUT = String.join("\n",
            "ABI,MIN,MAX",
            "arm64-v8a,25009448,25009450",
            "armeabi-v7a,18208826,18908828",
            "");

    private static final String ONE_ABI_OUTPUT = String.join("\n",
            "ABI,MIN,MAX",
            "armeabi-v7a,3008826,3008828",
            "");

    // Verifies per-ABI rows are parsed and the total spans the smallest min and largest max,
    // guarding against a headline range that ignores one of the architectures.
    @Test
    public void testParseTwoAbis() throws IOException {
        AndroidDownloadSize size = AndroidDownloadSize.parse(TWO_ABIS_OUTPUT);

        assertEquals(new AndroidDownloadSize.Range(18208826, 25009450), size.getTotal());
        assertEquals(List.of("arm64-v8a", "armeabi-v7a"), List.copyOf(size.getAbis().keySet()));
        assertEquals(new AndroidDownloadSize.Range(25009448, 25009450), size.getAbis().get("arm64-v8a"));
        assertEquals(new AndroidDownloadSize.Range(18208826, 18908828), size.getAbis().get("armeabi-v7a"));
    }

    // Verifies the report JSON keeps exact byte counts for the total and each ABI, guarding
    // against CI size tracking reading rounded or missing values.
    @Test
    public void testJson() throws IOException {
        String json = AndroidDownloadSize.parse(TWO_ABIS_OUTPUT).toJson();

        Map<?, ?> expected = Map.of(
                "min", 18208826,
                "max", 25009450,
                "abis", Map.of(
                        "arm64-v8a", Map.of("min", 25009448, "max", 25009450),
                        "armeabi-v7a", Map.of("min", 18208826, "max", 18908828)));
        assertEquals(expected, new ObjectMapper().readValue(json, Map.class));
    }

    // Verifies the log line shows decimal MB with a per-ABI breakdown and collapses ranges
    // that round to the same value.
    @Test
    public void testLogMessageTwoAbis() throws IOException {
        assertEquals("Estimated Play Store download size: 18.2 MB - 25.0 MB (arm64-v8a: 25.0 MB, armeabi-v7a: 18.2 MB - 18.9 MB)",
                AndroidDownloadSize.parse(TWO_ABIS_OUTPUT).toLogMessage());
    }

    // Verifies a single-architecture bundle logs one number without a redundant breakdown.
    @Test
    public void testLogMessageOneAbi() throws IOException {
        assertEquals("Estimated Play Store download size: 3.0 MB",
                AndroidDownloadSize.parse(ONE_ABI_OUTPUT).toLogMessage());
    }

    // Verifies output without size rows is an error, guarding against writing an empty
    // report when bundletool changes its output format.
    @Test
    public void testParseFailsWithoutSizes() {
        try {
            AndroidDownloadSize.parse("ABI,MIN,MAX\n");
            fail("Expected parsing to fail when bundletool reported no sizes");
        } catch (IOException e) {
        }
    }
}
