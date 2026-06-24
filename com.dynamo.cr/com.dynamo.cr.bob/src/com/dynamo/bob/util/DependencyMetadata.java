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

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;

import javax.xml.bind.DatatypeConverter;
import java.io.FileWriter;
import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;

public final class DependencyMetadata {
    public static final String DATA_FILE_NAME = "dependencies.json";
    public static final String PROJECT_PATH = ".internal/lib/" + DATA_FILE_NAME;
    public static final String OUTPUT_PATH = ".internal/" + DATA_FILE_NAME;
    private static final Pattern SHA1_PATTERN = Pattern.compile("(?i)(?<![0-9a-f])[0-9a-f]{40}(?![0-9a-f])");

    private DependencyMetadata() {
    }

    public static void saveAsJson(Path libDir, Collection<Library.Result> dependencies) {
        try {
            Files.createDirectories(libDir);
            try (FileWriter writer = new FileWriter(metadataPath(libDir).toFile())) {
                ObjectMapper mapper = new ObjectMapper().enable(SerializationFeature.INDENT_OUTPUT);
                mapper.writeValue(writer, collect(dependencies));
            }
        } catch (IOException e) {
            throw new RuntimeException("Failed to write dependencies JSON", e);
        }
    }

    public static void deleteJson(Path libDir) {
        try {
            Files.deleteIfExists(metadataPath(libDir));
        } catch (IOException e) {
            throw new RuntimeException("Failed to remove dependencies JSON", e);
        }
    }

    public static Path metadataPath(Path libDir) {
        return libDir.resolve(DATA_FILE_NAME);
    }

    public static List<Map<String, String>> collect(Collection<Library.Result> dependencies) {
        var dependencyEntries = new ArrayList<Map<String, String>>();
        if (dependencies == null) {
            return dependencyEntries;
        }

        for (var dependency : dependencies) {
            var dependencyEntry = new LinkedHashMap<String, String>();
            dependencyEntry.put("url", anonymizedUrl(dependency.uri()));

            var version = releaseVersion(dependency.uri());
            dependencyEntry.put("version", version == null ? "" : version);

            var archive = dependency.archive();
            var sha1 = archive == null ? null : firstSha1(archive.zipComment());
            dependencyEntry.put("commit-sha1", sha1 == null ? "" : sha1);

            if (archive != null) {
                var md5 = fileMd5(archive.path());
                if (md5 != null) {
                    dependencyEntry.put("payload-md5", md5);
                }
            }

            if (dependency.problem() != null) {
                dependencyEntry.put("problem", problemName(dependency.problem()));
            }

            dependencyEntries.add(dependencyEntry);
        }
        return dependencyEntries;
    }

    static String anonymizedUrl(URI uri) {
        try {
            return new URI(uri.getScheme(), null, uri.getHost(), uri.getPort(), uri.getPath(), null, uri.getFragment()).toString();
        } catch (URISyntaxException | IllegalArgumentException e) {
            var scheme = uri.getScheme();
            var host = uri.getHost();
            var path = uri.getPath();
            if (scheme != null && host != null) {
                return scheme + "://" + host + (path == null ? "" : path);
            }
            return uri.toString();
        }
    }

    static String releaseVersion(URI uri) {
        var path = uri.getPath();
        if (path == null || path.isBlank()) {
            return null;
        }

        var segments = path.split("/");
        for (var i = 0; i < segments.length; ++i) {
            if (segments[i].equals("releases")) {
                if (i + 2 < segments.length && segments[i + 1].equals("download")) {
                    return releaseVersionSegment(segments[i + 2]);
                }
                if (i + 1 < segments.length) {
                    return releaseVersionSegment(segments[i + 1]);
                }
            }
            if (segments[i].equals("archive")) {
                if (i + 3 < segments.length && segments[i + 1].equals("refs") && segments[i + 2].equals("tags")) {
                    return releaseVersionSegment(segments[i + 3]);
                }
                if (i + 1 < segments.length) {
                    return releaseVersionSegment(segments[i + 1]);
                }
            }
        }
        return null;
    }

    static String firstSha1(String text) {
        if (text == null) {
            return null;
        }
        var matcher = SHA1_PATTERN.matcher(text);
        return matcher.find() ? matcher.group().toLowerCase() : null;
    }

    private static String versionSegment(String segment) {
        if (segment == null || segment.isBlank()) {
            return null;
        }
        return stripArchiveExtension(segment);
    }

    private static String releaseVersionSegment(String segment) {
        var version = versionSegment(segment);
        return looksLikeVersion(version) ? version : null;
    }

    private static String stripArchiveExtension(String segment) {
        if (segment.endsWith(".tar.gz")) {
            return segment.substring(0, segment.length() - ".tar.gz".length());
        }
        if (segment.endsWith(".tgz")) {
            return segment.substring(0, segment.length() - ".tgz".length());
        }
        if (segment.endsWith(".zip")) {
            return segment.substring(0, segment.length() - ".zip".length());
        }
        return segment;
    }

    private static boolean looksLikeVersion(String version) {
        return version != null && version.matches(".*\\d.*") && !version.matches("(?i)[0-9a-f]{40}");
    }

    private static String fileMd5(Path path) {
        try {
            byte[] fileBytes = Files.readAllBytes(path);
            MessageDigest md = MessageDigest.getInstance("MD5");
            md.update(fileBytes);
            return DatatypeConverter.printHexBinary(md.digest()).toUpperCase();
        } catch (Exception e) {
            throw new RuntimeException("Failed to generate dependencies info", e);
        }
    }

    private static String problemName(Library.Problem problem) {
        return switch (problem) {
            case Library.Problem.Missing _ -> "missing";
            case Library.Problem.FetchFailed _ -> "fetch_failed";
            case Library.Problem.FailedHTTPRequest _ -> "failed_http_request";
            case Library.Problem.HttpConnectTimeout _ -> "http_connect_timeout";
            case Library.Problem.InvalidArchive _ -> "invalid_archive";
            case Library.Problem.DefoldMinVersion _ -> "defold_min_version";
            case Library.Problem.InstallFailed _ -> "install_failed";
        };
    }
}
