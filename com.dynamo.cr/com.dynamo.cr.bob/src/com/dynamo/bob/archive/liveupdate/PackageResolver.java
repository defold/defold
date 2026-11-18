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

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

import com.dynamo.liveupdate.proto.LiveUpdateManifest.ContainerPreset;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.FileCompression;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.FileCompressionPreset;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.ManifestDesc;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.NoMatchPolicy;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.PackageDesc;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.RuleDesc;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.RuleOperation;

/** Pure package resolution, shared by archive creation and editor previews. */
public final class PackageResolver {

    /** Startup traversal follows EAGER edges, retains only the target of DESCRIPTOR edges, and skips DEFERRED edges. */
    public enum Loading {
        EAGER,
        DESCRIPTOR,
        DEFERRED
    }

    public enum Severity {
        WARNING,
        ERROR
    }

    /** Identifies a rule by package name and zero-based position in its authored rule list. */
    public record RuleId(String packageName, int index) {}

    /** Rule diagnostics include zero-based package and rule indices; absent indices are -1. */
    public record Diagnostic(Severity severity, String packageName, int packageIndex, int rule, String resource, Problem problem) {
        public Diagnostic(Severity severity, String packageName, int rule, String resource, Problem problem) {
            this(severity, packageName, -1, rule, resource, problem);
        }

        public String message() {
            return problemMessage(problem);
        }
    }

    /** Structured validation details shared by Bob and the editor. */
    public sealed interface Problem {
        record InvalidPackageName(String name) implements Problem {}
        record DuplicatePackage(String name) implements Problem {}
        record MissingPackageTarget(String packageName, String ruleName) implements Problem {}
        record UnknownPackageTarget(String packageName, String ruleName, String target) implements Problem {}
        record PackageReferenceCycle(String packageName, String ruleName, List<String> cycle) implements Problem {
            public PackageReferenceCycle {
                cycle = List.copyOf(cycle);
            }
        }
        record InvalidPreset(String name, Problem cause) implements Problem {}
        record RequiredPresetName(boolean container) implements Problem {}
        record DuplicatePresetName(String name, boolean container) implements Problem {}
        record RequiredContainerProvider() implements Problem {}
        record CompressionLevelRange(String type, int min, int max) implements Problem {}
        record UnknownFilePreset(String name) implements Problem {}
        record UnknownContainerPreset(String name) implements Problem {}
        record InvalidRuleTarget() implements Problem {}
        record BuildConfigurationMatch(String packageName, String ruleName, String path) implements Problem {}
        record NoSourceMatch(String packageName, String ruleName, String pattern) implements Problem {}
        /** Column is one-based or zero when Java does not report a position. */
        record InvalidRegex(String packageName, String ruleName, String pattern, String reason, int column) implements Problem {}
        record UnassignedResource(String path) implements Problem {}
        record MissingCompiledResource(String path) implements Problem {}
        record MissingDependency(String path, String requiredBy) implements Problem {}
    }

    private static String problemMessage(Problem problem) {
        return switch (problem) {
            case Problem.InvalidPackageName(var name) -> "Invalid or reserved package name: " + name;
            case Problem.DuplicatePackage(var name) -> "Duplicate package: " + name;
            case Problem.MissingPackageTarget(var owner, var rule) ->
                    "Select a package in rule '" + rule + "' of package '" + owner + "'";
            case Problem.UnknownPackageTarget(var owner, var rule, var target) ->
                    "Unknown package '" + target + "' in rule '" + rule + "' of package '" + owner + "'";
            case Problem.PackageReferenceCycle(var owner, var rule, var cycle) ->
                    "Package reference cycle in rule '" + rule + "' of package '" + owner + "': " + String.join(" -> ", cycle);
            case Problem.InvalidPreset(var name, var cause) -> "Compression preset '" + name + "': " + problemMessage(cause);
            case Problem.RequiredPresetName(var container) ->
                    container ? "A container preset name is required" : "A compression preset name is required";
            case Problem.DuplicatePresetName(var name, var container) ->
                    (container ? "Duplicate container preset: " : "Duplicate compression preset: ") + name;
            case Problem.RequiredContainerProvider _ -> "A container provider is required";
            case Problem.CompressionLevelRange(var type, var min, var max) ->
                    type + " compression level must be between " + min + " and " + max;
            case Problem.UnknownFilePreset(var name) -> "Unknown file compression preset: " + name;
            case Problem.UnknownContainerPreset(var name) -> "Unknown container preset: " + name;
            case Problem.InvalidRuleTarget _ -> "A rule must select either a path regex or a package";
            case Problem.BuildConfigurationMatch(_, _, var path) -> "Package rule selects build configuration: " + path;
            case Problem.NoSourceMatch(var owner, var rule, var pattern) ->
                    "Rule '" + rule + "' in package '" + owner + "' matches no source files for Path '" + pattern
                            + "'. Patterns match the entire source path, including its leading '/'.";
            case Problem.InvalidRegex(var owner, var rule, var pattern, var reason, var column) ->
                    "Invalid regex in rule '" + rule + "' of package '" + owner + "'"
                            + (column == 0 ? "" : " at column " + column) + ": " + reason + ". Path: " + pattern;
            case Problem.UnassignedResource(var path) -> "Unassigned resource: " + path;
            case Problem.MissingCompiledResource(var path) -> "Missing compiled resource: " + path;
            case Problem.MissingDependency(var path, var requiredBy) ->
                    "Resource " + path + ", required by " + requiredBy + ", is absent from the base and all packages.";
        };
    }

    public record PackageReferences(Map<String, List<String>> targets, List<Diagnostic> diagnostics) {
        public PackageReferences {
            targets = immutableLists(targets);
            diagnostics = List.copyOf(diagnostics);
        }
    }

    /** The contributing rule chain, selected source, and runtime dependency path leading to one package member. */
    public record Contribution(List<RuleId> rules, String source, List<String> dependencyPath) {
        public Contribution {
            rules = List.copyOf(rules);
            dependencyPath = List.copyOf(dependencyPath);
        }
    }

    /** Runtime dependency paths and their startup loading behavior; keys use normalized archive paths. */
    public record Resource(Map<String, Loading> dependencies) {
        public Resource {
            dependencies = immutableMap(dependencies);
        }
    }

    /** Eligible paths by rule and diagnostics for validation problems, including ineligible matches. */
    public record Selection(Map<RuleId, List<String>> matches, List<Diagnostic> diagnostics) {
        public Selection {
            matches = immutableLists(matches);
            diagnostics = List.copyOf(diagnostics);
        }
    }

    public record Package(PackageDesc description, Set<String> resources, Map<String, List<Contribution>> provenance) {
        public Package {
            resources = immutableSet(resources);
            provenance = immutableLists(provenance);
        }
    }

    /**
     * Immutable package membership and dependency information shared by packaging and preview consumers.
     * Locks map startup-required output paths to their loading routes; requirements map each package's external
     * resource paths to alternative supplying packages, excluding resources already provided by the base.
     */
    public record Plan(Map<String, Package> packages,
            Set<String> base,
            Map<String, List<String>> locks,
            Map<String, List<String>> sourceOutputs,
            Map<String, String> primarySources,
            Map<String, Resource> resources,
            List<Diagnostic> diagnostics,
            Map<String, Map<String, List<String>>> requirements) {
        public Plan {
            packages = immutableMap(packages);
            base = immutableSet(base);
            locks = immutableLists(locks);
            sourceOutputs = immutableLists(sourceOutputs);
            primarySources = immutableMap(primarySources);
            resources = immutableMap(resources);
            diagnostics = List.copyOf(diagnostics);

            Map<String, Map<String, List<String>>> frozen = new LinkedHashMap<>();
            for (Map.Entry<String, Map<String, List<String>>> entry : requirements.entrySet()) {
                frozen.put(entry.getKey(), immutableLists(entry.getValue()));
            }
            requirements = immutableMap(frozen);
        }

        /** Reports whether diagnostics contain an error that prevents packaging. */
        public boolean hasErrors() {
            return diagnostics.stream().anyMatch(diagnostic -> diagnostic.severity() == Severity.ERROR);
        }

        /** Returns the sorted union of base and package paths, counting overlapping resources once. */
        public Set<String> emittedResources() {
            Set<String> emitted = new TreeSet<>(base);
            for (Package pkg : packages.values()) {
                emitted.addAll(pkg.resources());
            }
            return immutableSet(emitted);
        }
    }

    /** Copies a map and its lists, preserving map iteration order while preventing later caller mutation. */
    private static <K, V> Map<K, List<V>> immutableLists(Map<K, List<V>> values) {
        Map<K, List<V>> result = new LinkedHashMap<>();
        for (Map.Entry<K, List<V>> entry : values.entrySet()) {
            result.put(entry.getKey(), List.copyOf(entry.getValue()));
        }
        return immutableMap(result);
    }

    /** Takes a read-only map snapshot while preserving iteration order. */
    private static <K, V> Map<K, V> immutableMap(Map<K, V> values) {
        return Collections.unmodifiableMap(new LinkedHashMap<>(values));
    }

    /** Takes a sorted, read-only snapshot of resource paths. */
    private static Set<String> immutableSet(Collection<String> values) {
        return Collections.unmodifiableSet(new TreeSet<>(values));
    }

    /** Uses forward slashes and ensures a leading slash; does not resolve dot segments or change case. */
    public static String normalize(String path) {
        String normalized = path.replace('\\', '/');
        return normalized.startsWith("/") ? normalized : "/" + normalized;
    }

    /** Validates package names and Add/Remove references, and supplies cycle-free choices in authoring order. */
    public static PackageReferences packageReferences(ManifestDesc manifest) {
        Map<String, Set<String>> referrers = new LinkedHashMap<>();
        List<Diagnostic> diagnostics = new ArrayList<>();
        for (PackageDesc pkg : manifest.getPackagesList()) {
            String name = pkg.getName();
            if (!name.matches("[a-zA-Z0-9][a-zA-Z0-9._-]*")
                    || name.equalsIgnoreCase("base")
                    || name.matches("(?i)(con|prn|aux|nul|com[1-9]|lpt[1-9])([.].*)?")) {
                diagnostics.add(new Diagnostic(Severity.ERROR, name, -1, "", new Problem.InvalidPackageName(name)));
            }
            if (referrers.containsKey(name)) {
                diagnostics.add(new Diagnostic(Severity.ERROR, name, -1, "", new Problem.DuplicatePackage(name)));
            }
            referrers.computeIfAbsent(pkg.getName(), k -> new LinkedHashSet<>());
        }
        for (PackageDesc pkg : manifest.getPackagesList()) {
            for (RuleDesc rule : pkg.getRulesList()) {
                String target = rule.getPackage();
                if (rule.hasPackage() && !target.isEmpty() && referrers.containsKey(target)) {
                    referrers.get(target).add(pkg.getName());
                }
            }
        }

        Map<String, List<String>> targets = new LinkedHashMap<>();
        for (int packageIndex = 0; packageIndex < manifest.getPackagesCount(); ++packageIndex) {
            PackageDesc pkg = manifest.getPackages(packageIndex);
            String owner = pkg.getName();

            // Walk incoming references. Each link points back toward the owner,
            // so it also gives a concrete cycle when an outgoing rule closes it.
            Map<String, String> ancestors = new LinkedHashMap<>();
            ancestors.put(owner, null);
            ArrayDeque<String> pending = new ArrayDeque<>();
            pending.add(owner);
            while (!pending.isEmpty()) {
                String current = pending.remove();
                for (String referrer : referrers.get(current)) {
                    if (!ancestors.containsKey(referrer)) {
                        ancestors.put(referrer, current);
                        pending.add(referrer);
                    }
                }
            }

            List<String> availableTargets = referrers.keySet().stream()
                    .filter(name -> !name.isEmpty() && !ancestors.containsKey(name))
                    .toList();
            targets.put(owner, availableTargets);

            for (int i = 0; i < pkg.getRulesCount(); ++i) {
                RuleDesc rule = pkg.getRules(i);
                if (!rule.hasPackage()) {
                    continue;
                }
                String target = rule.getPackage();
                if (target.isEmpty() || !referrers.containsKey(target)) {
                    Problem problem = target.isEmpty()
                            ? new Problem.MissingPackageTarget(owner, rule.getName())
                            : new Problem.UnknownPackageTarget(owner, rule.getName(), target);
                    diagnostics.add(new Diagnostic(Severity.ERROR, owner, packageIndex, i, "", problem));
                } else if (ancestors.containsKey(target)) {
                    List<String> cycle = new ArrayList<>();
                    cycle.add(owner);
                    for (String current = target; current != null; current = ancestors.get(current)) {
                        cycle.add(current);
                    }
                    diagnostics.add(new Diagnostic(Severity.ERROR, owner, packageIndex, i, "",
                            new Problem.PackageReferenceCycle(owner, rule.getName(), cycle)));
                }
            }
        }
        return new PackageReferences(targets, diagnostics);
    }

    /**
     * Matches Java regexes against entire normalized source paths, including resources outside the bootstrap closure.
     * Validates names, rule targets, references, and per-rule no-match policies without expanding dependencies.
     */
    public static Selection select(ManifestDesc manifest, Collection<String> sourcePaths) {
        return select(manifest, sourcePaths, true);
    }

    /** Validates authoring syntax and package references without matching a file inventory. */
    public static List<Diagnostic> validate(ManifestDesc manifest) {
        return select(manifest, List.of(), false).diagnostics();
    }

    /** Resolves file compression; an omitted choice uses LZ4 at its default level. */
    public static FileCompressionPreset fileCompressionPreset(ManifestDesc manifest, String name) {
        var preset = findFileCompressionPreset(manifest, name);
        if (preset == null) {
            throw new IllegalArgumentException(problemMessage(new Problem.UnknownFilePreset(name)));
        }
        return preset;
    }

    private static FileCompressionPreset findFileCompressionPreset(ManifestDesc manifest, String name) {
        if (name.isEmpty()) {
            return FileCompressionPreset.getDefaultInstance();
        }
        for (FileCompressionPreset preset : manifest.getFileCompressionPresetsList()) {
            if (preset.getName().equals(name)) {
                return preset;
            }
        }
        return null;
    }

    /** Returns property errors keyed by preset name for the editor and build validation. */
    public static Map<String, Map<String, Problem>> fileCompressionPresetErrors(ManifestDesc manifest) {
        Map<String, Map<String, Problem>> errors = new LinkedHashMap<>();
        Set<String> names = new LinkedHashSet<>();
        for (FileCompressionPreset preset : manifest.getFileCompressionPresetsList()) {
            Map<String, Problem> properties = new LinkedHashMap<>();
            if (preset.getName().isBlank()) {
                properties.put("name", new Problem.RequiredPresetName(false));
            } else if (!names.add(preset.getName())) {
                properties.put("name", new Problem.DuplicatePresetName(preset.getName(), false));
            }
            if (preset.getFileCompression() == FileCompression.LZ4
                    && (preset.getLz4Level() < 1 || preset.getLz4Level() > 12)) {
                properties.put("lz4-level", new Problem.CompressionLevelRange("LZ4", 1, 12));
            }
            if (!properties.isEmpty()) {
                errors.put(preset.getName(), Map.copyOf(properties));
            }
        }
        return Map.copyOf(errors);
    }

    /** Resolves a container preset; an omitted choice uses ZIP at its default level. */
    public static ContainerPreset containerPreset(ManifestDesc manifest, String name) {
        var preset = findContainerPreset(manifest, name);
        if (preset == null) {
            throw new IllegalArgumentException(problemMessage(new Problem.UnknownContainerPreset(name)));
        }
        return preset;
    }

    private static ContainerPreset findContainerPreset(ManifestDesc manifest, String name) {
        if (name.isEmpty()) {
            return ContainerPreset.getDefaultInstance();
        }
        for (ContainerPreset preset : manifest.getContainerPresetsList()) {
            if (preset.getName().equals(name)) {
                return preset;
            }
        }
        return null;
    }

    /** Returns container property errors shared by editor properties and build validation. */
    public static Map<String, Map<String, Problem>> containerPresetErrors(ManifestDesc manifest) {
        Map<String, Map<String, Problem>> errors = new LinkedHashMap<>();
        Set<String> names = new LinkedHashSet<>();
        for (ContainerPreset preset : manifest.getContainerPresetsList()) {
            Map<String, Problem> properties = new LinkedHashMap<>();
            if (preset.getName().isBlank()) {
                properties.put("name", new Problem.RequiredPresetName(true));
            } else if (!names.add(preset.getName())) {
                properties.put("name", new Problem.DuplicatePresetName(preset.getName(), true));
            }
            if (preset.getProvider().isBlank()) {
                properties.put("provider", new Problem.RequiredContainerProvider());
            }
            if (preset.getProvider().equals("zip") && (preset.getZipLevel() < -1 || preset.getZipLevel() > 9)) {
                properties.put("zip-level", new Problem.CompressionLevelRange("ZIP", -1, 9));
            }
            if (!properties.isEmpty()) {
                errors.put(preset.getName(), Map.copyOf(properties));
            }
        }
        return Map.copyOf(errors);
    }

    /** Shares authoring checks between immediate editor validation and full resource selection. */
    private static Selection select(ManifestDesc manifest, Collection<String> sourcePaths, boolean matchSources) {
        List<Diagnostic> diagnostics = new ArrayList<>();
        for (var errors : List.of(containerPresetErrors(manifest), fileCompressionPresetErrors(manifest))) {
            for (var preset : errors.entrySet()) {
                for (Problem problem : preset.getValue().values()) {
                    diagnostics.add(new Diagnostic(Severity.ERROR, "", -1, "",
                            new Problem.InvalidPreset(preset.getKey(), problem)));
                }
            }
        }
        Map<String, String> choices = new LinkedHashMap<>();
        choices.put("base", manifest.getBaseFileCompressionPreset());
        for (PackageDesc pkg : manifest.getPackagesList()) {
            choices.put(pkg.getName(), pkg.getFileCompressionPreset());
        }
        for (var choice : choices.entrySet()) {
            if (findFileCompressionPreset(manifest, choice.getValue()) == null) {
                diagnostics.add(new Diagnostic(Severity.ERROR, choice.getKey(), -1, "",
                        new Problem.UnknownFilePreset(choice.getValue())));
            }
        }
        Map<RuleId, List<String>> matches = new LinkedHashMap<>();
        List<Diagnostic> buildConfigurationProblems = new ArrayList<>();
        Set<String> inventory = new TreeSet<>();
        for (String source : sourcePaths) {
            inventory.add(normalize(source));
        }

        for (int packageIndex = 0; packageIndex < manifest.getPackagesCount(); ++packageIndex) {
            PackageDesc pkg = manifest.getPackages(packageIndex);
            String name = pkg.getName();
            if (findContainerPreset(manifest, pkg.getContainerPreset()) == null) {
                diagnostics.add(new Diagnostic(Severity.ERROR, name, -1, "",
                        new Problem.UnknownContainerPreset(pkg.getContainerPreset())));
            }

            for (int i = 0; i < pkg.getRulesCount(); ++i) {
                RuleDesc rule = pkg.getRules(i);
                RuleId id = new RuleId(name, i);
                if (rule.hasPath() == rule.hasPackage()) {
                    diagnostics.add(new Diagnostic(Severity.ERROR, name, packageIndex, i, "",
                            new Problem.InvalidRuleTarget()));
                } else if (rule.hasPath()) {
                    try {
                        Pattern pattern = Pattern.compile(rule.getPath());
                        if (!matchSources) {
                            continue;
                        }
                        List<String> selected = new ArrayList<>();
                        boolean matched = false;
                        for (String source : inventory) {
                            if (!pattern.matcher(source).matches()) {
                                continue;
                            }
                            matched = true;
                            if (source.equals("/game.project") || source.endsWith(".archivemanifest")) {
                                buildConfigurationProblems.add(new Diagnostic(Severity.ERROR, name, packageIndex, i, source,
                                        new Problem.BuildConfigurationMatch(name, rule.getName(), source)));
                            } else {
                                selected.add(source);
                            }
                        }
                        matches.put(id, selected);
                        if (!matched && rule.getNoMatch() != NoMatchPolicy.IGNORE) {
                            Severity severity = rule.getNoMatch() == NoMatchPolicy.ERROR ? Severity.ERROR : Severity.WARNING;
                            diagnostics.add(new Diagnostic(severity, name, packageIndex, i, "",
                                    new Problem.NoSourceMatch(name, rule.getName(), rule.getPath())));
                        }
                    } catch (PatternSyntaxException e) {
                        diagnostics.add(new Diagnostic(Severity.ERROR, name, packageIndex, i, "",
                                new Problem.InvalidRegex(name, rule.getName(), rule.getPath(),
                                        e.getDescription(), Math.max(0, e.getIndex() + 1))));
                    }
                }
            }
        }

        diagnostics.addAll(packageReferences(manifest).diagnostics());
        // Bob previously checked these paths after all other selection errors.
        diagnostics.addAll(buildConfigurationProblems);
        return new Selection(matches, diagnostics);
    }

    private final Map<String, PackageDesc> descriptions = new LinkedHashMap<>();
    private final Map<String, Package> resolved = new LinkedHashMap<>();
    private final Map<String, List<String>> sourceOutputs;
    private final Map<String, Resource> resources;
    private final Map<List<String>, Map<String, List<String>>> sourceClosures = new LinkedHashMap<>();
    private final Selection selection;
    private final List<Diagnostic> diagnostics;
    private final Set<String> resolving = new LinkedHashSet<>();

    /** Captures declarations and normalized source mappings, and validates rules against that source inventory. */
    private PackageResolver(ManifestDesc manifest, Map<String, List<String>> sourceOutputs, Map<String, Resource> resources) {
        for (PackageDesc pkg : manifest.getPackagesList()) {
            descriptions.put(pkg.getName(), pkg);
        }
        this.sourceOutputs = new LinkedHashMap<>();
        for (Map.Entry<String, List<String>> entry : sourceOutputs.entrySet()) {
            List<String> outputs = entry.getValue().stream()
                    .map(PackageResolver::normalize)
                    .distinct()
                    .sorted()
                    .toList();
            this.sourceOutputs.put(normalize(entry.getKey()), outputs);
        }
        this.resources = immutableMap(resources);
        selection = select(manifest, this.sourceOutputs.keySet());
        diagnostics = new ArrayList<>(selection.diagnostics());
    }

    /**
     * Traverses dependencies breadth-first, retaining one route to each reachable output.
     * Startup traversal respects loading boundaries; ordinary package expansion follows every dependency edge.
     */
    private Map<String, List<String>> closure(Collection<String> roots, boolean startup) {
        Map<String, List<String>> paths = new LinkedHashMap<>();
        Set<String> queued = new LinkedHashSet<>();
        ArrayDeque<List<String>> pending = new ArrayDeque<>();
        for (String root : roots) {
            if (queued.add(root)) {
                pending.add(List.of(root));
            }
        }

        while (!pending.isEmpty()) {
            List<String> path = pending.remove();
            String resource = path.getLast();
            paths.putIfAbsent(resource, path);
            Resource node = resources.get(resource);
            if (node != null) {
                for (Map.Entry<String, Loading> edge : node.dependencies().entrySet()) {
                    Loading loading = startup ? edge.getValue() : Loading.EAGER;
                    if (loading == Loading.DEFERRED) {
                        continue;
                    }

                    // A descriptor-only visit must not prevent a later eager route from expanding it.
                    boolean expand = loading == Loading.EAGER && queued.add(edge.getKey());
                    if (!expand && (!startup || paths.containsKey(edge.getKey()))) {
                        continue;
                    }

                    List<String> childPath = new ArrayList<>(path);
                    childPath.add(edge.getKey());
                    List<String> child = List.copyOf(childPath);
                    if (startup) {
                        paths.putIfAbsent(edge.getKey(), child);
                    }
                    if (expand) {
                        pending.add(child);
                    }
                }
            }
        }
        return paths;
    }

    /**
     * Applies ordered Add/Remove rules and retains their provenance, caching each package's final contents.
     * References resolve the target's final contents, including forward references; invalid or cyclic targets return null.
     */
    private Package resolvePackage(String name) {
        Package existing = resolved.get(name);
        if (existing != null) {
            return existing;
        }
        PackageDesc description = descriptions.get(name);
        if (description == null) {
            return null;
        }
        if (!resolving.add(name)) {
            // Selection has already reported the offending reference rules.
            return null;
        }

        Map<String, List<Contribution>> members = new LinkedHashMap<>();
        for (int i = 0; i < description.getRulesCount(); ++i) {
            RuleDesc rule = description.getRules(i);
            RuleId id = new RuleId(name, i);
            boolean remove = rule.getOperation() == RuleOperation.REMOVE;
            if (rule.hasPath()) {
                for (String source : selection.matches().getOrDefault(id, List.of())) {
                    // Build inputs can share the same compiled outputs (e.g. atlas images).
                    // Reuse their traversal, while retaining each source and rule in provenance.
                    Map<String, List<String>> dependencies = sourceClosures.computeIfAbsent(
                            sourceOutputs.get(source), roots -> closure(roots, false));
                    for (Map.Entry<String, List<String>> dependency : dependencies.entrySet()) {
                        String output = dependency.getKey();
                        if (remove) {
                            members.remove(output);
                        } else {
                            List<Contribution> contributions = members.computeIfAbsent(output, k -> new ArrayList<>());
                            contributions.add(new Contribution(List.of(id), source, dependency.getValue()));
                        }
                    }
                }
            } else if (rule.hasPackage() && !rule.getPackage().isEmpty()) {
                Package referenced = resolvePackage(rule.getPackage());
                if (referenced != null) {
                    for (Map.Entry<String, List<Contribution>> entry : referenced.provenance().entrySet()) {
                        String output = entry.getKey();
                        if (remove) {
                            members.remove(output);
                            continue;
                        }

                        List<Contribution> inherited = members.computeIfAbsent(output, k -> new ArrayList<>());
                        for (Contribution contribution : entry.getValue()) {
                            List<RuleId> rules = new ArrayList<>(contribution.rules());
                            rules.add(id);
                            inherited.add(new Contribution(rules, contribution.source(), contribution.dependencyPath()));
                        }
                    }
                }
            }
        }

        Package result = new Package(description, members.keySet(), members);
        resolving.remove(name);
        resolved.put(name, result);
        return result;
    }

    /** Resolves a plan without primary-source attribution; otherwise follows the full resolve overload. */
    public static Plan resolve(ManifestDesc manifest, Map<String, List<String>> sourceOutputs,
            Map<String, Resource> resources, Collection<String> bootstrapRoots,
            Collection<String> customRoots, boolean export) {
        return resolve(manifest, sourceOutputs, Map.of(), resources, bootstrapRoots, customRoots, export);
    }

    /**
     * Resolves ordered rules, overlapping packages, base locks, provenance, and external dependency requirements.
     * This operation performs no filesystem I/O. Invalid input and unassigned or missing resources are reported in the plan.
     *
     * @param sourceOutputs source inventory mapped to actual runtime outputs; an empty list means no runtime output
     * @param primarySources runtime output to originating source, used for navigation and legacy conversion
     * @param resources runtime graph keyed by normalized output paths with leading slashes
     * @param bootstrapRoots normalized runtime paths whose startup closure must remain in the base
     * @param customRoots normalized custom-resource output paths whose closure initially belongs to the base
     * @param export true subtracts package membership from the base before restoring startup locks;
     *               false includes all selected content in the base
     * @return immutable resolution result; check {@link Plan#hasErrors()} before packaging
     */
    public static Plan resolve(ManifestDesc manifest, Map<String, List<String>> sourceOutputs,
            Map<String, String> primarySources, Map<String, Resource> resources,
            Collection<String> bootstrapRoots, Collection<String> customRoots, boolean export) {
        PackageResolver resolver = new PackageResolver(manifest, sourceOutputs, resources);
        Map<String, Package> packages = new LinkedHashMap<>();
        for (PackageDesc pkg : manifest.getPackagesList()) {
            packages.put(pkg.getName(), resolver.resolvePackage(pkg.getName()));
        }

        Set<String> roots = new LinkedHashSet<>(bootstrapRoots);
        roots.addAll(customRoots);
        Set<String> base = new TreeSet<>(resolver.closure(roots, false).keySet());
        Map<String, List<String>> locks = resolver.closure(bootstrapRoots, true);

        Set<String> assigned = new TreeSet<>();
        for (Package pkg : packages.values()) {
            assigned.addAll(pkg.resources());
        }
        if (export) {
            base.removeAll(assigned);
        } else {
            base.addAll(assigned);
        }
        base.addAll(locks.keySet());

        Set<String> emitted = new TreeSet<>(base);
        emitted.addAll(assigned);
        for (String path : resources.keySet()) {
            if (!emitted.contains(path)) {
                resolver.diagnostics.add(new Diagnostic(Severity.WARNING, "", -1, path, new Problem.UnassignedResource(path)));
            }
        }

        for (String path : emitted) {
            Resource node = resources.get(path);
            if (node == null) {
                resolver.diagnostics.add(new Diagnostic(Severity.ERROR, "", -1, path, new Problem.MissingCompiledResource(path)));
            } else {
                for (String dependency : node.dependencies().keySet()) {
                    if (!emitted.contains(dependency)) {
                        resolver.diagnostics.add(new Diagnostic(Severity.ERROR, "", -1, dependency,
                                new Problem.MissingDependency(dependency, path)));
                    }
                }
            }
        }

        Map<String, Map<String, List<String>>> requirements = new LinkedHashMap<>();
        for (Package pkg : packages.values()) {
            Map<String, List<String>> external = new LinkedHashMap<>();
            for (String dependency : resolver.closure(pkg.resources(), false).keySet()) {
                if (base.contains(dependency) || pkg.resources().contains(dependency)) {
                    continue;
                }
                List<String> alternatives = new ArrayList<>();
                for (Package supplier : packages.values()) {
                    if (supplier.resources().contains(dependency)) {
                        alternatives.add(supplier.description().getName());
                    }
                }
                external.put(dependency, List.copyOf(alternatives));
            }
            requirements.put(pkg.description().getName(), immutableMap(external));
        }

        return new Plan(packages, base, locks, resolver.sourceOutputs, primarySources, resources, resolver.diagnostics, requirements);
    }
}
