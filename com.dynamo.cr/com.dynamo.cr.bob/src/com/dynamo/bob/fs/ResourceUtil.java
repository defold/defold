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

package com.dynamo.bob.fs;

import com.dynamo.bob.util.MurmurHash;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Collections;

public class ResourceUtil {

    protected static HashMap<String, String> extensionMapping = new HashMap<>();
    protected static HashMap<String, String> cachedPaths = new HashMap<>();
    // A set of the minified results, so we don't accidentally minify them again
    protected static HashSet<String>         minifiedPaths = new HashSet<>();
    protected static HashSet<String>         minifySupport = new HashSet<>();

    private static boolean minifyPathEnabled = false;

    protected static String buildDirectory = null;

    public static void registerMapping(String inExt, String outExt) {
        extensionMapping.put(inExt, outExt);
        minifySupport.add(outExt);
    }

    public static void setBuildDirectory(String buildDirectory) {
        ResourceUtil.buildDirectory = buildDirectory;
    }

    /**
     * Change extension of filename
     * @param fileName file-name to change extension for
     * @param ext new extension including preceding dot
     * @return new file-name
     */
    public static String changeExt(String fileName, String ext) {
        int i = fileName.lastIndexOf(".");
        if (i == -1) {
            throw new IllegalArgumentException(String.format("Missing extension in name '%s'", fileName));
        }
        fileName = fileName.substring(0, i);
        return fileName + ext;
    }

    /**
     * Optionally change suffix of filename if the requested input suffix matches
     * @param path path to change suffix for
     * @param from input suffix
     * @param to output suffix
     * @return modified path if input suffix matched, otherwise the original input path
     */
    public static String replaceExt(String path, String from, String to) {
        if (path.endsWith(from)) {
            return path.substring(0, path.lastIndexOf(from)).concat(to);
        }
        return path;
    }

   /**
    * Get the output suffix from an input siffix
    * @param inExt the input file suffix (including the '.')
    * @return the output suffix (including the '.')
    */
    public static String getOutputExt(String inExt) {
        String outExt = extensionMapping.get(inExt); // Get the output ext, or use the inExt as default
        if (outExt != null)
            return outExt;
        return inExt;
    }

    // returns suffix with ".suffix"
    public static String getSuffix(String path) {
        int index = path.lastIndexOf(".");
        if (index < 0)
            return null;
        return path.substring(index);
    }

    public static void enableMinification(boolean enable) {
        minifyPathEnabled = enable;
    }

    public static boolean isMinificationEnabled() {
        return minifyPathEnabled;
    }

    public static String minifyPathAndReplaceExt(String path, String from, String to) {
        path = replaceExt(path, from, to);
        return minifyPath(path);
    }

    public static String minifyPathAndChangeExt(String path, String to) {
        path = changeExt(path, to);
        return minifyPath(path);
    }

    public static void disableMinify(String ext) {
        minifySupport.remove(ext);
    }

    /**
     * Minify a resource path into a hashed bucket layout.
     *
     * Return value expectations:
     * - If minification is disabled, the input path is returned unchanged.
     * - If the path suffix is not in the minify support set, the input path is returned unchanged.
     * - If minification is enabled and the suffix is supported, the path is replaced with a hashed bucket path
     *   that preserves the original suffix (e.g. ".spc", ".materialc", ".texturesetc").
     *
     * Leading "/" expectations:
     * - For non-build paths (not starting with buildDirectory), the returned minified path is forced to start with "/".
     * - For build output paths (starting with buildDirectory), the returned minified path starts with buildDirectory
     *   and does not include a leading "/".
     * - If minification is disabled or unsupported for the suffix, non-build paths still get a leading "/".
     *   Build output paths are returned unchanged.
     *
     */
    public static String minifyPath(String path) {
        if (path == null || path.length() == 0) {
            return path;
        }
        boolean isBuildPath = buildDirectory != null && buildDirectory.length() > 0 && path.startsWith(buildDirectory);
        if (!minifyPathEnabled) {
            if (!isBuildPath && path.charAt(0) != '/') {
                return "/" + path;
            }
            return path;
        }

        String suffix = getSuffix(path);
        if (!minifySupport.contains(suffix)) {
            if (!isBuildPath && path.charAt(0) != '/') {
                return "/" + path;
            }
            return path;
        }

        if (isBuildPath) {
            path = path.substring(buildDirectory.length());
        }

        if (path.charAt(0) != '/') {
            path = "/" + path;
        }

        if (minifiedPaths.contains(path)) {
            return path; // Already minified
        }

        String cachedPath = cachedPaths.getOrDefault(path, null);
        if (cachedPath != null) {
            return cachedPath;
        }

        long hash = MurmurHash.hash64(path);
        String hex = Long.toHexString(hash);

        // Normally I wouldn't put 65k files into the same folder, but here we are also
        // relying on the fact that the hash has a good distribution, and thus getting us an "equal" spread
        // over the different buckets. // MAWE
        int bucket0 = (int)((hash >> 0 ) & 0xFFFFL);
        int bucket1 = (int)((hash >> 16) & 0xFFFFL);
        int bucket2 = (int)((hash >> 32) & 0xFFFFL);
        int bucket3 = (int)((hash >> 48) & 0xFFFFL);

        String prefix = isBuildPath ? buildDirectory : "";
        String minifiedPath = String.format("%s/%s/%s/%s/%s%s", prefix,
                Long.toHexString(bucket0), Long.toHexString(bucket1), Long.toHexString(bucket2), Long.toHexString(bucket3), suffix);

        cachedPaths.put(path, minifiedPath);
        minifiedPaths.add(minifiedPath);
        return minifiedPath;
    }

    public static Map<String, String> snapshotMinifiedPaths() {
        return Collections.unmodifiableMap(new HashMap<>(cachedPaths));
    }

    public static String getBuildDirectory() {
        return buildDirectory;
    }

}
