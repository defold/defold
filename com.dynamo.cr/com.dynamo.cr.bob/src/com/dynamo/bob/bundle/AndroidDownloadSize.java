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

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.TreeMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;

/**
 * Estimated Google Play download size, parsed from the output of
 * `bundletool get-size total --dimensions=ABI`.
 */
public class AndroidDownloadSize {

    public record Range(long min, long max) {}

    private static final Pattern ROW_PATTERN = Pattern.compile("^([^,]*),(\\d+),(\\d+)$");

    private final Range total;
    private final Map<String, Range> abis;

    private AndroidDownloadSize(Range total, Map<String, Range> abis) {
        this.total = total;
        this.abis = Collections.unmodifiableMap(abis);
    }

    // The output also contains whatever bundletool and the JVM print to stderr, so lines
    // that aren't size rows are skipped
    public static AndroidDownloadSize parse(String getSizeOutput) throws IOException {
        Map<String, Range> abis = new TreeMap<>();
        boolean inTable = false;
        for (String line : getSizeOutput.split("\\R")) {
            line = line.trim();
            if (line.equals("ABI,MIN,MAX")) {
                inTable = true;
                continue;
            }
            Matcher m = ROW_PATTERN.matcher(line);
            if (inTable && m.matches()) {
                abis.put(m.group(1), new Range(Long.parseLong(m.group(2)), Long.parseLong(m.group(3))));
            }
        }
        if (abis.isEmpty()) {
            throw new IOException("No download sizes found in bundletool output: " + getSizeOutput);
        }

        long min = Long.MAX_VALUE;
        long max = 0;
        for (Range range : abis.values()) {
            min = Math.min(min, range.min());
            max = Math.max(max, range.max());
        }
        return new AndroidDownloadSize(new Range(min, max), abis);
    }

    public Range getTotal() {
        return total;
    }

    public Map<String, Range> getAbis() {
        return abis;
    }

    public String toJson() throws IOException {
        Map<String, Object> abiMap = new LinkedHashMap<>();
        for (Map.Entry<String, Range> entry : abis.entrySet()) {
            abiMap.put(entry.getKey(), rangeToMap(entry.getValue()));
        }
        Map<String, Object> root = rangeToMap(total);
        root.put("abis", abiMap);
        return new ObjectMapper().enable(SerializationFeature.INDENT_OUTPUT).writeValueAsString(root);
    }

    public String toLogMessage() {
        String message = "Estimated Play Store download size: " + formatRange(total);
        if (abis.size() > 1) {
            List<String> parts = new ArrayList<>();
            for (Map.Entry<String, Range> entry : abis.entrySet()) {
                parts.add(entry.getKey() + ": " + formatRange(entry.getValue()));
            }
            message += " (" + String.join(", ", parts) + ")";
        }
        return message;
    }

    private static Map<String, Object> rangeToMap(Range range) {
        Map<String, Object> map = new LinkedHashMap<>();
        map.put("min", range.min());
        map.put("max", range.max());
        return map;
    }

    // Decimal megabytes, as reported by the Play Console
    private static String formatMegabytes(long bytes) {
        return String.format(Locale.ROOT, "%.1f MB", bytes / 1_000_000.0);
    }

    private static String formatRange(Range range) {
        String min = formatMegabytes(range.min());
        String max = formatMegabytes(range.max());
        return min.equals(max) ? min : min + " - " + max;
    }
}
