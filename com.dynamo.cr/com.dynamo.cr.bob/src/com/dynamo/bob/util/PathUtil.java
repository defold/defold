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

import java.util.ArrayList;
import java.util.List;
import java.util.function.Predicate;
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

    /**
     * Creates a predicate that tests project paths against a list of patterns
     * of the kind found in `.defignore` and `.defunload` files. Shared between
     * Bob and the editor, so the two always agree on what a pattern matches.
     *
     * Both patterns and tested paths are project paths: they start with `/`
     * and have no trailing `/`. Matching is exact and case-sensitive regardless
     * of the file system. A pattern matches a path if it matches the whole path
     * or one of its parent directories, so `/dir` matches `/dir` and
     * `/dir/entry` but not `/dire`.
     *
     * Patterns may contain wildcards:
     * 1) * matches zero or more characters except /
     * 2) ? matches exactly one character except /
     * 3) **&#47; matches zero or more whole path segments
     * 4) a trailing /** matches the directory itself and everything below it
     * Everything else is matched literally.
     */
    public static Predicate<String> makeProjPathPredicate(Iterable<String> patterns) {
        List<String> literals = new ArrayList<>();
        List<Pattern> globs = new ArrayList<>();
        for (String pattern : patterns) {
            if (pattern.indexOf('*') >= 0 || pattern.indexOf('?') >= 0) {
                globs.add(projPathGlobToRegex(pattern));
            } else {
                literals.add(pattern);
            }
        }
        return projPath -> {
            for (String literal : literals) {
                if (projPath.startsWith(literal)
                        && (projPath.length() == literal.length() || projPath.charAt(literal.length()) == '/')) {
                    return true;
                }
            }
            for (Pattern glob : globs) {
                if (glob.matcher(projPath).matches()) {
                    return true;
                }
            }
            return false;
        };
    }

    private static Pattern projPathGlobToRegex(String glob) {
        if (glob.endsWith("/**")) {
            glob = glob.substring(0, glob.length() - 3);
        }
        StringBuilder regex = new StringBuilder("^");
        StringBuilder literal = new StringBuilder();
        int n = glob.length();
        for (int i = 0; i < n; ++i) {
            char c = glob.charAt(i);
            if (c != '*' && c != '?') {
                literal.append(c);
                continue;
            }
            if (literal.length() > 0) {
                regex.append(Pattern.quote(literal.toString()));
                literal.setLength(0);
            }
            if (c == '?') {
                regex.append("[^/]");
            } else if (glob.startsWith("**/", i)) {
                regex.append("(?:.*/)?");
                i += 2;
            } else if (glob.startsWith("**", i)) {
                regex.append(".*");
                i += 1;
            } else {
                regex.append("[^/]*");
            }
        }
        if (literal.length() > 0) {
            regex.append(Pattern.quote(literal.toString()));
        }
        regex.append("(?:/.*)?$");
        return Pattern.compile(regex.toString());
    }
}
