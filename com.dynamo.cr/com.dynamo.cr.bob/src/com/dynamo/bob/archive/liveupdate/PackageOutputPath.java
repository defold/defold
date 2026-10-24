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

package com.dynamo.bob.archive.liveupdate;

import java.nio.file.Path;
import java.util.function.Function;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Resolves authored output directories for both Bob and the editor. Does not create files. */
public final class PackageOutputPath {
    private static final Pattern VARIABLE = Pattern.compile("\\$\\{([A-Za-z_][A-Za-z0-9_]*)\\}|\\$([A-Za-z_][A-Za-z0-9_]*)|%([A-Za-z_][A-Za-z0-9_]*)%");

    private PackageOutputPath() {}

    public static Path resolve(String directory, Path projectRoot, Function<String, String> environment) {
        Matcher matcher = VARIABLE.matcher(directory);
        StringBuilder expanded = new StringBuilder();
        while (matcher.find()) {
            String name = matcher.group(1) != null ? matcher.group(1) : matcher.group(2) != null ? matcher.group(2) : matcher.group(3);
            String value = environment.apply(name);
            if (value == null) throw new IllegalArgumentException("Undefined environment variable '" + name + "' in Live Update output directory");
            matcher.appendReplacement(expanded, Matcher.quoteReplacement(value));
        }
        matcher.appendTail(expanded);
        if (expanded.toString().isBlank()) throw new IllegalArgumentException("Live Update output directory must not be empty");
        return projectRoot.resolve(expanded.toString()).toAbsolutePath().normalize();
    }
}
