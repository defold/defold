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

    /** A validation message; rule is -1 and packageName or resource may be empty when no specific location applies. */
    public record Diagnostic(Severity severity, String packageName, int rule, String resource, String message) {}

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
                diagnostics.add(new Diagnostic(Severity.ERROR, name, -1, "", "Invalid or reserved package name: " + name));
            }
            if (referrers.containsKey(name)) {
                diagnostics.add(new Diagnostic(Severity.ERROR, name, -1, "", "Duplicate package: " + name));
            }
            referrers.computeIfAbsent(pkg.getName(), k -> new LinkedHashSet<>());
        }
        for (PackageDesc pkg : manifest.getPackagesList()) {
            for (RuleDesc rule : pkg.getRulesList()) {
                if (rule.hasPackage() && referrers.containsKey(rule.getPackage())) {
                    referrers.get(rule.getPackage()).add(pkg.getName());
                }
            }
        }

        Map<String, List<String>> targets = new LinkedHashMap<>();
        for (PackageDesc pkg : manifest.getPackagesList()) {
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
                    .filter(name -> !ancestors.containsKey(name))
                    .toList();
            targets.put(owner, availableTargets);

            for (int i = 0; i < pkg.getRulesCount(); ++i) {
                RuleDesc rule = pkg.getRules(i);
                if (!rule.hasPackage()) {
                    continue;
                }
                String target = rule.getPackage();
                String context = " in rule '" + rule.getName() + "' of package '" + owner + "'";
                if (!referrers.containsKey(target)) {
                    String message = target.isEmpty() ? "Select a package" : "Unknown package '" + target + "'";
                    diagnostics.add(new Diagnostic(Severity.ERROR, owner, i, "", message + context));
                } else if (ancestors.containsKey(target)) {
                    List<String> cycle = new ArrayList<>();
                    cycle.add(owner);
                    for (String current = target; current != null; current = ancestors.get(current)) {
                        cycle.add(current);
                    }
                    diagnostics.add(new Diagnostic(Severity.ERROR, owner, i, "",
                            "Package reference cycle" + context + ": " + String.join(" -> ", cycle)));
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
        if (name.isEmpty()) {
            return FileCompressionPreset.getDefaultInstance();
        }
        for (FileCompressionPreset preset : manifest.getFileCompressionPresetsList()) {
            if (preset.getName().equals(name)) {
                return preset;
            }
        }
        throw new IllegalArgumentException("Unknown file compression preset: " + name);
    }

    /** Returns property errors keyed by preset name for the editor and build validation. */
    public static Map<String, Map<String, String>> fileCompressionPresetErrors(ManifestDesc manifest) {
        Map<String, Map<String, String>> errors = new LinkedHashMap<>();
        Set<String> names = new LinkedHashSet<>();
        for (FileCompressionPreset preset : manifest.getFileCompressionPresetsList()) {
            Map<String, String> properties = new LinkedHashMap<>();
            if (preset.getName().isBlank()) {
                properties.put("name", "A compression preset name is required");
            } else if (!names.add(preset.getName())) {
                properties.put("name", "Duplicate compression preset: " + preset.getName());
            }
            if (preset.getFileCompression() == FileCompression.LZ4
                    && (preset.getLz4Level() < 1 || preset.getLz4Level() > 12)) {
                properties.put("lz4-level", "LZ4 compression level must be between 1 and 12");
            }
            if (!properties.isEmpty()) {
                errors.put(preset.getName(), Map.copyOf(properties));
            }
        }
        return Map.copyOf(errors);
    }

    /** Resolves a container preset; an omitted choice uses ZIP at its default level. */
    public static ContainerPreset containerPreset(ManifestDesc manifest, String name) {
        if (name.isEmpty()) {
            return ContainerPreset.getDefaultInstance();
        }
        for (ContainerPreset preset : manifest.getContainerPresetsList()) {
            if (preset.getName().equals(name)) {
                return preset;
            }
        }
        throw new IllegalArgumentException("Unknown container preset: " + name);
    }

    /** Returns container property errors shared by editor properties and build validation. */
    public static Map<String, Map<String, String>> containerPresetErrors(ManifestDesc manifest) {
        Map<String, Map<String, String>> errors = new LinkedHashMap<>();
        Set<String> names = new LinkedHashSet<>();
        for (ContainerPreset preset : manifest.getContainerPresetsList()) {
            Map<String, String> properties = new LinkedHashMap<>();
            if (preset.getName().isBlank()) {
                properties.put("name", "A container preset name is required");
            } else if (!names.add(preset.getName())) {
                properties.put("name", "Duplicate container preset: " + preset.getName());
            }
            if (preset.getProvider().isBlank()) {
                properties.put("provider", "A container provider is required");
            }
            if (preset.getProvider().equals("zip") && (preset.getZipLevel() < -1 || preset.getZipLevel() > 9)) {
                properties.put("zip-level", "ZIP compression level must be between -1 and 9");
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
                for (String message : preset.getValue().values()) {
                    diagnostics.add(new Diagnostic(Severity.ERROR, "", -1, "", "Compression preset '" + preset.getKey() + "': " + message));
                }
            }
        }
        Map<String, String> choices = new LinkedHashMap<>();
        choices.put("base", manifest.getBaseFileCompressionPreset());
        for (PackageDesc pkg : manifest.getPackagesList()) {
            choices.put(pkg.getName(), pkg.getFileCompressionPreset());
        }
        for (var choice : choices.entrySet()) {
            try {
                fileCompressionPreset(manifest, choice.getValue());
            } catch (IllegalArgumentException e) {
                diagnostics.add(new Diagnostic(Severity.ERROR, choice.getKey(), -1, "", e.getMessage()));
            }
        }
        Map<RuleId, List<String>> matches = new LinkedHashMap<>();
        Set<String> inventory = new TreeSet<>();
        for (String source : sourcePaths) {
            inventory.add(normalize(source));
        }

        for (PackageDesc pkg : manifest.getPackagesList()) {
            String name = pkg.getName();
            try {
                containerPreset(manifest, pkg.getContainerPreset());
            } catch (IllegalArgumentException e) {
                diagnostics.add(new Diagnostic(Severity.ERROR, name, -1, "", e.getMessage()));
            }

            for (int i = 0; i < pkg.getRulesCount(); ++i) {
                RuleDesc rule = pkg.getRules(i);
                RuleId id = new RuleId(name, i);
                if (rule.hasPath() == rule.hasPackage()) {
                    diagnostics.add(new Diagnostic(Severity.ERROR, name, i, "",
                            "A rule must select either a path regex or a package"));
                } else if (rule.hasPath()) {
                    try {
                        Pattern pattern = Pattern.compile(rule.getPath());
                        if (!matchSources) {
                            continue;
                        }
                        List<String> selected = inventory.stream()
                                .filter(path -> pattern.matcher(path).matches())
                                .toList();
                        matches.put(id, selected);
                        if (selected.isEmpty() && rule.getNoMatch() != NoMatchPolicy.IGNORE) {
                            Severity severity = rule.getNoMatch() == NoMatchPolicy.ERROR ? Severity.ERROR : Severity.WARNING;
                            diagnostics.add(new Diagnostic(severity, name, i, "",
                                    "Rule '" + rule.getName() + "' in package '" + name
                                            + "' matches no source files for Path '" + rule.getPath()
                                            + "'. Patterns match the entire source path, including its leading '/'."));
                        }
                    } catch (PatternSyntaxException e) {
                        String location = e.getIndex() < 0 ? "" : " at column " + (e.getIndex() + 1);
                        diagnostics.add(new Diagnostic(Severity.ERROR, name, i, "",
                                "Invalid regex in rule '" + rule.getName() + "' of package '" + name + "'" + location + ": "
                                        + e.getDescription() + ". Path: " + rule.getPath()));
                    }
                }
            }
        }

        diagnostics.addAll(packageReferences(manifest).diagnostics());
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
            } else if (rule.hasPackage()) {
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

    /**
     * Converts a resolved legacy selection to ordered source rules without moving shared base content.
     * Returns no rules when nothing was excluded; does not write or activate a manifest.
     *
     * @throws IllegalArgumentException if source outputs or cycles prevent rules from preserving the exact selection
     */
    public static List<RuleDesc> migrateLegacy(Plan plan) {
        Set<String> excluded = new TreeSet<>();
        for (Package pkg : plan.packages().values()) {
            excluded.addAll(pkg.resources());
        }
        if (excluded.isEmpty()) {
            return List.of();
        }

        PackageResolver resolver = new PackageResolver(ManifestDesc.getDefaultInstance(), plan.sourceOutputs(), plan.resources());
        Set<String> affected = resolver.closure(excluded, false).keySet();
        Map<String, Set<String>> candidates = new LinkedHashMap<>();
        for (String source : new TreeSet<>(plan.primarySources().values())) {
            List<String> outputs = resolver.sourceOutputs.get(source);
            if (!Collections.disjoint(outputs, affected)) {
                Map<String, List<String>> dependencies = resolver.sourceClosures.computeIfAbsent(
                        outputs, roots -> resolver.closure(roots, false));
                candidates.put(source, dependencies.keySet());
            }
        }

        // Work backwards: the last rule affecting a resource must agree with
        // its legacy membership. Once assigned, earlier rules may overlap it.
        Set<String> assigned = new LinkedHashSet<>();
        List<RuleDesc> reversed = new ArrayList<>();
        while (!assigned.containsAll(excluded)) {
            boolean progressed = false;
            for (var iterator = candidates.entrySet().iterator(); iterator.hasNext();) {
                Map.Entry<String, Set<String>> candidate = iterator.next();
                Set<String> remaining = new LinkedHashSet<>(candidate.getValue());
                remaining.removeAll(assigned);
                boolean add = excluded.containsAll(remaining);
                if (!add && !Collections.disjoint(excluded, remaining)) {
                    continue;
                }

                iterator.remove();
                if (remaining.isEmpty()) {
                    continue;
                }
                RuleDesc rule = RuleDesc.newBuilder()
                        .setName(candidate.getKey())
                        .setPath(Pattern.quote(candidate.getKey()))
                        .setOperation(add ? RuleOperation.ADD : RuleOperation.REMOVE)
                        .build();
                reversed.add(rule);
                assigned.addAll(remaining);
                progressed = true;
                if (assigned.containsAll(excluded)) {
                    break;
                }
            }
            if (!progressed) {
                Set<String> unresolved = new TreeSet<>(excluded);
                unresolved.removeAll(assigned);
                throw new IllegalArgumentException("Cannot preserve the legacy Live Update selection for "
                        + unresolved.iterator().next()
                        + ": its source outputs or dependency cycle also include resources kept in Base.");
            }
        }

        Collections.reverse(reversed);
        Set<String> selected = new LinkedHashSet<>();
        List<RuleDesc> rules = new ArrayList<>();
        for (RuleDesc rule : reversed) {
            List<String> outputs = resolver.sourceOutputs.get(rule.getName());
            Set<String> closure = resolver.sourceClosures.get(outputs).keySet();
            boolean changed;
            if (rule.getOperation() == RuleOperation.ADD) {
                changed = selected.addAll(closure);
            } else {
                changed = selected.removeAll(closure);
            }
            if (changed) {
                rules.add(rule);
            }
        }
        return List.copyOf(rules);
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
                resolver.diagnostics.add(new Diagnostic(Severity.WARNING, "", -1, path, "Unassigned resource: " + path));
            }
        }

        for (String path : emitted) {
            Resource node = resources.get(path);
            if (node == null) {
                resolver.diagnostics.add(new Diagnostic(Severity.ERROR, "", -1, path, "Missing compiled resource: " + path));
            } else {
                for (String dependency : node.dependencies().keySet()) {
                    if (!emitted.contains(dependency)) {
                        resolver.diagnostics.add(new Diagnostic(Severity.ERROR, "", -1, dependency,
                                "Required by " + path + " but absent from the base and all packages"));
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
