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

package com.dynamo.bob.util;

import java.util.regex.Pattern;

import org.apache.commons.io.FilenameUtils;

public class PathUtil {

    /**
     * Matches paths according to the following rules (from Ant):
     * 1) * matches zero or more characters within a path name
     * 2) ? matches exactly one character within a path name
     * 3) ** matches zero or more path names
     * The path is normalized with unix separators before matching.
     * @param path
     * @param pattern
     * @return
     */
    public static boolean wildcardMatch(String path, String pattern) {
        if (pattern == null) {
            return true;
        }
        // normalize with unix separators
        path = FilenameUtils.normalize(path, true);
        StringBuffer regex = new StringBuffer();
        while (!pattern.isEmpty()) {
            int offset = 1;
            if (pattern.startsWith(".")) {
                // escape the dot character
                regex.append("\\.");
            } else if (pattern.startsWith("**")) {
                // ** matches anything
                regex.append(".*");
                offset = 2;
                // consume following / as well
                if (pattern.startsWith("**/")) {
                    offset = 3;
                }
            } else if (pattern.startsWith("*")) {
                // * matches anything except /
                regex.append("[^/]*");
            } else if (pattern.startsWith("?")) {
                // ? matches anything once, except /
                regex.append("[^/]");
            } else {
                regex.append(pattern.charAt(0));
            }
            pattern = pattern.substring(offset);
        }
        return Pattern.matches(regex.toString(), path);
    }
}
