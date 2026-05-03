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

import com.dynamo.bob.IProgress;
import com.dynamo.bob.archive.EngineVersion;
import org.apache.commons.codec.binary.Hex;
import org.apache.commons.io.FilenameUtils;

import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.ParseException;
import java.time.Duration;
import java.util.*;
import java.util.concurrent.*;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/// Shared bob/editor library dependency API.
///
/// This class owns the common cache-scan and fetch surface for dependency
/// archives stored under `.internal/lib`. Callers consume returned [Result]
/// values and keep mount/reload policy outside this class.
public final class Library {
    private static final Logger logger = Logger.getLogger(Library.class.getName());
    private static final int FETCHES_PER_HOST = 4;
    private static final HttpClient HTTP_CLIENT = HttpClient.newBuilder()
            .connectTimeout(Duration.ofMillis(2000))
            .followRedirects(HttpClient.Redirect.NORMAL)
            .build();

    private Library() {
    }

    /// Returns the dependency state currently available in the local library
    /// cache without performing any network access.
    ///
    /// @param uris   dependency URIs to inspect
    /// @param libDir library cache directory
    /// @return dependency results corresponding to the supplied URIs
    public static List<Result> cached(Collection<URI> uris, Path libDir) {
        Objects.requireNonNull(uris, "uris");
        Objects.requireNonNull(libDir, "libDir");
        var taggedResults = scanInstalledArchives(uniqueUris(uris), libDir);
        var results = new ArrayList<Result>(taggedResults.size());
        for (var taggedResult : taggedResults) {
            results.add(taggedResult.result());
        }
        return results;
    }

    /// Returns dependency state after attempting to refresh the supplied URIs.
    ///
    /// The shared implementation may use cached archives, perform conditional
    /// HTTP fetches, and report progress as each URI check starts.
    ///
    /// @param uris     dependency URIs to inspect
    /// @param libDir   library cache directory
    /// @param email    optional shared auth email/header value
    /// @param auth     optional shared auth token/header value
    /// @param progress progress reporter for download tasks
    /// @return dependency results corresponding to the supplied URIs
    public static List<Result> fetch(Collection<URI> uris, Path libDir, String email, String auth, IProgress progress) {
        try (progress) {
            Objects.requireNonNull(uris, "uris");
            Objects.requireNonNull(libDir, "libDir");
            Objects.requireNonNull(progress, "progress");

            var uniqueUris = uniqueUris(uris);
            progress.message(new IProgress.Message.DownloadingArchives(uniqueUris.size()));
            var split = progress.split(uniqueUris.size());

            var cachedResults = scanInstalledArchives(uniqueUris, libDir);
            var cachedByUri = new HashMap<URI, TaggedResult>(cachedResults.size());
            for (var result : cachedResults) {
                cachedByUri.put(result.result().uri(), result);
            }
            var executor = Executors.newVirtualThreadPerTaskExecutor();
            try {
                Files.createDirectories(libDir);
                var tasks = new ArrayList<Future<Result>>(uniqueUris.size());
                var downloadTasksByHost = new HashMap<URI, List<Future<Result>>>();
                var hostLimits = new ConcurrentHashMap<URI, Semaphore>();
                for (var uri : uniqueUris) {
                    var cachedResult = cachedByUri.get(uri);
                    var task = executor.submit(() -> fetchTask(uri, cachedResult, libDir, email, auth, split.subtask(), hostLimits));
                    tasks.add(task);
                    var host = hostKey(uri);
                    if (host != null) {
                        downloadTasksByHost.computeIfAbsent(host, _ -> new ArrayList<>()).add(task);
                    }
                }

                downloadTasksByHost.forEach((host, downloadTasks) -> executor.submit(() -> hostProbeTask(host, downloadTasks, executor)));

                var results = new ArrayList<Result>(tasks.size());
                for (var i = 0; i < tasks.size(); i++) {
                    try {
                        results.add(tasks.get(i).get());
                    } catch (CancellationException e) {
                        var uri = uniqueUris.get(i);
                        results.add(new Result(uri, cachedByUri.get(uri).result().archive(), new Problem.FetchFailed()));
                    }
                }
                return results;
            } catch (InterruptedException | ExecutionException | IOException e) {
                if (e instanceof InterruptedException) {
                    Thread.currentThread().interrupt();
                } else {
                    logger.log(Level.WARNING, "Unexpected library fetch failure", e);
                }
                var results = new ArrayList<Result>(uniqueUris.size());
                for (var uri : uniqueUris) {
                    results.add(new Result(uri, cachedByUri.get(uri).result().archive(), new Problem.FetchFailed()));
                }
                return results;
            } finally {
                executor.shutdownNow();
            }
        }
    }

    /// Helper function to parse and validate project dependency ZIP
    ///
    /// @param archivePath path to dependency ZIP file
    public static Archive readArchive(Path archivePath) throws IOException {
        var inspection = inspectArchive(archivePath);
        if (inspection.problem() != null) {
            throw new IOException("Failed to inspect archive " + archivePath + ": " + inspection.problem());
        }
        return inspection.archive();
    }

    private static void hostProbeTask(URI host, List<Future<Result>> downloadTasks, ExecutorService executor) {
        try {
            var request = HttpRequest.newBuilder(host).method("HEAD", HttpRequest.BodyPublishers.noBody()).timeout(Duration.ofSeconds(5)).build();
            // Unfortunately, 5 second timeout on an HTTP request does not mean that send will abort after
            // 5 seconds, so we separately do a hard timeout
            executor.submit(() -> HTTP_CLIENT.send(request, HttpResponse.BodyHandlers.discarding())).get(5, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        } catch (ExecutionException | TimeoutException e) {
            for (var downloadTask : downloadTasks) {
                downloadTask.cancel(true);
            }
        }
    }

    private static Result fetchTask(URI uri, TaggedResult cachedResult, Path libDir, String email, String auth, IProgress progress,
                                    ConcurrentHashMap<URI, Semaphore> hostLimits) {
        try (progress) {
            URI sanitizedUri;
            try {
                sanitizedUri = new URI(uri.getScheme(), null, uri.getHost(), uri.getPort(), uri.getPath(), null, uri.getFragment());
            } catch (URISyntaxException e) {
                sanitizedUri = URI.create(uri.getScheme() + "://" + uri.getHost() + uri.getPath());
            }
            progress.message(new IProgress.Message.DownloadingArchive(sanitizedUri));
            var host = hostKey(uri);
            if (host == null) {
                throw new IllegalArgumentException("Invalid host URI " + uri);
            }
            var permit = hostLimits.computeIfAbsent(host, _ -> new Semaphore(FETCHES_PER_HOST));
            permit.acquire();
            try {
                var cachedArchive = cachedResult.result().archive();
                var request = HttpRequest.newBuilder(uri).GET().header("Accept", "application/zip");
                var userInfo = uri.getUserInfo();
                if (userInfo != null) {
                    var parts = userInfo.split(":", 2);
                    var user = replaceUserInfoEnv(parts[0]);
                    var resolvedUserInfo = parts.length == 1 ? user : user + ':' + replaceUserInfoEnv(parts[1]);
                    request.header("Authorization", "Basic " + Base64.getEncoder().encodeToString(resolvedUserInfo.getBytes(StandardCharsets.UTF_8)));
                } else if (email != null && auth != null) {
                    request.header("X-Email", email);
                    request.header("X-Auth", auth);
                }
                if (cachedResult.tag() != null && !cachedResult.tag().isEmpty()) {
                    request.header("If-None-Match", cachedResult.tag());
                }
                var stagedPath = Files.createTempFile(libDir, cacheKey(uri) + '-', ".zip.tmp");
                try {
                    var response = HTTP_CLIENT.send(request.build(), HttpResponse.BodyHandlers.ofFile(stagedPath));
                    var code = response.statusCode();
                    if (code == 304) {
                        return cachedResult.result();
                    }
                    if (code >= 400) {
                        return new Result(uri, cachedArchive, new Problem.FetchFailed());
                    }
                    // Validate and inspect the downloaded archive before replacing the
                    // installed copy in the shared cache.
                    var stagedInspection = inspectArchive(stagedPath);
                    if (stagedInspection.problem() != null) {
                        return new Result(uri, cachedArchive, stagedInspection.problem());
                    }

                    var tag = response.headers().firstValue("ETag").orElse("");
                    return install(uri, libDir, stagedPath, stagedInspection.archive(), tag, cachedArchive);
                } finally {
                    try {
                        Files.deleteIfExists(stagedPath);
                    } catch (IOException e) {
                        logger.log(Level.FINE, "Failed to clean staged library " + stagedPath, e);
                    }
                }
            } finally {
                permit.release();
            }
        } catch (Exception e) {
            if (e instanceof InterruptedException) {
                Thread.currentThread().interrupt();
            }
            return new Result(uri, cachedResult.result().archive(), new Problem.FetchFailed());
        }
    }

    private static Result install(URI uri, Path libDir, Path stagedPath, Archive stagedArchive, String tag, Archive previousArchive) {
        var fileName = cacheKey(uri) + '-' + Base64.getUrlEncoder().encodeToString(tag.getBytes(StandardCharsets.UTF_8)) + ".zip";
        var targetPath = libDir.resolve(fileName);
        try {
            try {
                Files.move(stagedPath, targetPath, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
            } catch (AtomicMoveNotSupportedException e) {
                Files.move(stagedPath, targetPath, StandardCopyOption.REPLACE_EXISTING);
            }
            var installedArchive = new Archive(targetPath, stagedArchive.baseDir(), stagedArchive.includeDirs());
            purgeOldVersions(uri, targetPath);
            return new Result(uri, installedArchive, null);
        } catch (IOException e) {
            if (previousArchive != null) {
                return new Result(uri, previousArchive, new Problem.InstallFailed());
            }
            logger.log(Level.FINE, "Failed to install library " + uri, e);
            return new Result(uri, null, new Problem.InstallFailed());
        }
    }

    private static void purgeOldVersions(URI uri, Path keep) {
        try (DirectoryStream<Path> stream = Files.newDirectoryStream(keep.getParent())) {
            var prefix = cacheKey(uri) + '-';
            for (var path : stream) {
                if (Files.isRegularFile(path) && !path.equals(keep) && path.getFileName().toString().startsWith(prefix) && path.getFileName().toString().endsWith(".zip")) {
                    try {
                        Files.deleteIfExists(path);
                    } catch (IOException e) {
                        logger.log(Level.FINE, "Failed to purge old library " + path, e);
                    }
                }
            }
        } catch (IOException e) {
            logger.log(Level.FINE, "Failed to list old library versions for " + uri, e);
        }
    }

    private static ArchiveInspection inspectArchive(Path archivePath) {
        try (ZipFile archive = new ZipFile(archivePath.toFile())) {
            var baseDir = findBaseDir(archive);
            if (baseDir == null) {
                return new ArchiveInspection(null, new Problem.InvalidArchive());
            }
            var projectEntry = archive.getEntry(baseDir.isEmpty() ? "game.project" : baseDir + "/game.project");
            if (projectEntry == null) {
                return new ArchiveInspection(null, new Problem.InvalidArchive());
            }
            var includeDirs = new TreeSet<String>();
            try (InputStream input = archive.getInputStream(projectEntry)) {
                var properties = new BobProjectProperties();
                properties.load(input);
                var minVersion = properties.getStringValue("library", "defold_min_version", "");
                if (minVersion != null && !minVersion.isEmpty() && compareVersions(EngineVersion.version, minVersion) < 0) {
                    return new ArchiveInspection(null, new Problem.DefoldMinVersion(minVersion));
                }

                var includeDirsValue = properties.getStringValue("library", "include_dirs", "");
                for (var part : includeDirsValue.split("[,\\s]")) {
                    if (!part.isEmpty()) {
                        includeDirs.add(part);
                    }
                }
            } catch (ParseException e) {
                return new ArchiveInspection(null, new Problem.InvalidArchive());
            }
            return new ArchiveInspection(new Archive(archivePath, baseDir, Collections.unmodifiableSet(includeDirs)), null);
        } catch (IOException e) {
            return new ArchiveInspection(null, new Problem.InvalidArchive());
        }
    }

    private static String findBaseDir(ZipFile archive) {
        Enumeration<? extends ZipEntry> entries = archive.entries();
        while (entries.hasMoreElements()) {
            var entryName = entries.nextElement().getName();
            if (!FilenameUtils.getName(entryName).equals("game.project")) {
                continue;
            }
            var baseDir = FilenameUtils.getPathNoEndSeparator(entryName);
            return baseDir != null ? baseDir : "";
        }
        return null;
    }

    private static List<URI> uniqueUris(Collection<URI> uris) {
        var uniqueUris = new ArrayList<URI>();
        var seenUris = new HashSet<URI>();
        for (var uri : uris) {
            Objects.requireNonNull(uri, "uri");
            if (seenUris.add(uri)) {
                uniqueUris.add(uri);
            }
        }
        return uniqueUris;
    }

    private static URI hostKey(URI uri) {
        try {
            return new URI(uri.getScheme(), null, uri.getHost(), uri.getPort(), null, null, null);
        } catch (URISyntaxException e) {
            return null;
        }
    }

    private static List<TaggedResult> scanInstalledArchives(Collection<URI> uniqueUris, Path libDir) {
        var archives = new HashMap<URI, TaggedPath>();
        if (Files.isDirectory(libDir)) {
            var wantedKeys = new HashMap<String, URI>();
            for (var uri : uniqueUris) {
                wantedKeys.put(cacheKey(uri), uri);
            }
            try (DirectoryStream<Path> stream = Files.newDirectoryStream(libDir)) {
                for (var path : stream) {
                    if (!Files.isRegularFile(path)) {
                        continue;
                    }
                    var name = path.getFileName().toString();
                    var dash = name.indexOf('-');
                    if (dash <= 0 || !name.endsWith(".zip")) {
                        continue;
                    }
                    var uri = wantedKeys.get(name.substring(0, dash));
                    if (uri == null || archives.containsKey(uri)) {
                        continue;
                    }
                    var encodedTag = name.substring(dash + 1, name.length() - 4);
                    try {
                        var tag = new String(Base64.getUrlDecoder().decode(encodedTag), StandardCharsets.UTF_8);
                        archives.put(uri, new TaggedPath(path, tag));
                    } catch (IllegalArgumentException ignored) {
                    }
                }
            } catch (IOException e) {
                logger.log(Level.FINE, "Failed to scan library directory " + libDir, e);
            }
        }

        var results = new ArrayList<TaggedResult>(uniqueUris.size());
        for (var uri : uniqueUris) {
            var installed = archives.get(uri);
            if (installed == null) {
                results.add(new TaggedResult(new Result(uri, null, new Problem.Missing()), null));
                continue;
            }
            var inspected = inspectArchive(installed.path());
            results.add(new TaggedResult(new Result(uri, inspected.archive(), inspected.problem()), installed.tag()));
        }
        return results;
    }

    private static String cacheKey(URI uri) {
        try {
            var sha1 = MessageDigest.getInstance("SHA1");
            sha1.update(uri.toString().getBytes(StandardCharsets.UTF_8));
            return Hex.encodeHexString(sha1.digest());
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }
    }

    private static String replaceUserInfoEnv(String value) {
        if (value != null && value.startsWith("__") && value.endsWith("__") && value.length() > 4) {
            var replacement = System.getenv(value.substring(2, value.length() - 2));
            if (replacement != null) {
                return replacement;
            }
        }
        return value;
    }

    private static int compareVersions(String left, String right) {
        var a = left.split("\\.");
        var b = right.split("\\.");
        var length = Math.max(a.length, b.length);
        for (var i = 0; i < length; ++i) {
            var leftPart = i < a.length ? parseLeadingInt(a[i]) : 0;
            var rightPart = i < b.length ? parseLeadingInt(b[i]) : 0;
            if (leftPart != rightPart) {
                return leftPart - rightPart;
            }
        }
        return 0;
    }

    private static int parseLeadingInt(String value) {
        var result = 0;
        var found = false;
        for (var i = 0; i < value.length(); ++i) {
            var c = value.charAt(i);
            if (Character.isDigit(c)) {
                found = true;
                result = result * 10 + (c - '0');
            } else {
                break;
            }
        }
        return found ? result : 0;
    }

    private record TaggedPath(Path path, String tag) {
    }

    private record TaggedResult(Result result, String tag) {
    }

    private record ArchiveInspection(Archive archive, Problem problem) {
    }

    /// Shared dependency result.
    ///
    /// @param uri     dependency identity
    /// @param archive installed archive to use, or `null` when unavailable
    /// @param problem structured problem associated with this dependency, or `null` when no problem is being reported
    public record Result(URI uri, Archive archive, Problem problem) {
        public Result {
            Objects.requireNonNull(uri, "uri");
            if (archive == null && problem == null) {
                throw new IllegalArgumentException("archive == null requires problem != null");
            }
        }
    }

    /// Metadata for an installed archive that callers can consume without
    /// reopening and reparsing the zip.
    ///
    /// @param path        installed archive path
    /// @param baseDir     archive-relative directory containing `game.project`
    /// @param includeDirs archive-relative include directories
    public record Archive(Path path, String baseDir, Set<String> includeDirs) {
    }

    /// Structured dependency problem.
    public sealed interface Problem permits Problem.Missing, Problem.FetchFailed, Problem.InvalidArchive, Problem.DefoldMinVersion, Problem.InstallFailed {
        /// No usable cached archive is currently available for the dependency.
        record Missing() implements Problem {
        }

        /// Refreshing the dependency failed during transport or HTTP fetch.
        record FetchFailed() implements Problem {
        }

        /// The archive was present but unusable, for example due to invalid zip contents or missing required project metadata.
        record InvalidArchive() implements Problem {
        }

        /// The dependency requires a newer Defold version than the current runtime.
        ///
        /// @param required minimum compatible Defold version
        record DefoldMinVersion(String required) implements Problem {
        }

        /// The dependency was fetched and validated, but committing it into the installed cache failed.
        record InstallFailed() implements Problem {
        }
    }
}
