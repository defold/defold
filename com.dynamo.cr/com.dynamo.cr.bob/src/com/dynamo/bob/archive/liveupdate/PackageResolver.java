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

import com.dynamo.liveupdate.proto.LiveUpdateManifest.ManifestDesc;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.PackageDesc;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.RuleDesc;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.RuleOperation;

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

/** Pure package resolution, shared by archive creation and editor previews. */
public final class PackageResolver {
    public enum Loading { EAGER, DESCRIPTOR, DEFERRED }
    public enum Severity { WARNING, ERROR }

    public record RuleId(String packageName, int index) {}
    public record Diagnostic(Severity severity, String packageName, int rule, String resource, String message) {}
    public record PackageReferences(Map<String, List<String>> targets, List<Diagnostic> diagnostics) {
        public PackageReferences {
            targets = immutableLists(targets);
            diagnostics = List.copyOf(diagnostics);
        }
    }
    public record Contribution(List<RuleId> rules, String source, List<String> dependencyPath) {
        public Contribution {
            rules = List.copyOf(rules);
            dependencyPath = List.copyOf(dependencyPath);
        }
    }
    public record Resource(Map<String, Loading> dependencies) {
        public Resource { dependencies = immutableMap(dependencies); }
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
    public record Plan(Map<String, Package> packages, Set<String> base,
                       Map<String, List<String>> locks, Map<String, List<String>> sourceOutputs,
                       Map<String, String> primarySources,
                       Map<String, Resource> resources, List<Diagnostic> diagnostics,
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
            requirements.forEach((name, values) -> frozen.put(name, immutableLists(values)));
            requirements = immutableMap(frozen);
        }
        public boolean hasErrors() {
            return diagnostics.stream().anyMatch(d -> d.severity() == Severity.ERROR);
        }
        public Set<String> emittedResources() {
            Set<String> emitted = new TreeSet<>(base);
            for (Package pkg : packages.values()) emitted.addAll(pkg.resources());
            return immutableSet(emitted);
        }
    }

    private static <K, V> Map<K, List<V>> immutableLists(Map<K, List<V>> values) {
        Map<K, List<V>> result = new LinkedHashMap<>();
        values.forEach((key, list) -> result.put(key, List.copyOf(list)));
        return immutableMap(result);
    }

    private static <K, V> Map<K, V> immutableMap(Map<K, V> values) {
        return Collections.unmodifiableMap(new LinkedHashMap<>(values));
    }
    private static <T> Set<T> immutableSet(Collection<T> values) {
        return Collections.unmodifiableSet(new LinkedHashSet<>(values));
    }
    public static String normalize(String path) {
        String normalized = path.replace('\\', '/');
        return normalized.startsWith("/") ? normalized : "/" + normalized;
    }

    /**
     * Creates editable rules for legacy proxy exclusions. Subtract unchecked
     * collections last so mixed proxy routes and shared dependencies stay in
     * the base. Their complete closures are retained conservatively, including
     * any nested proxies, without retaining Exclude as a packaging control.
     */
    public static ManifestDesc migrationManifest(String name, String provider,
                                                 Collection<String> excluded, Collection<String> included) {
        PackageDesc.Builder pkg = PackageDesc.newBuilder().setName(name).setProvider(provider);
        for (String path : new TreeSet<>(excluded)) {
            pkg.addRules(RuleDesc.newBuilder().setName(path).setPath(Pattern.quote(normalize(path))));
        }
        for (String path : new TreeSet<>(included)) {
            pkg.addRules(RuleDesc.newBuilder().setName(path).setOperation(RuleOperation.REMOVE)
                    .setPath(Pattern.quote(normalize(path))));
        }
        ManifestDesc manifest = ManifestDesc.newBuilder().addPackages(pkg).build();
        Set<String> paths = new TreeSet<>(excluded);
        paths.addAll(included);
        if (select(manifest, paths).diagnostics().stream().anyMatch(d -> d.severity() == Severity.ERROR)) {
            manifest = manifest.toBuilder().setPackages(0, pkg.setName("liveupdate")).build();
        }
        return manifest;
    }

    /** Validates Add/Remove references and supplies cycle-free choices for each package in authoring order. */
    public static PackageReferences packageReferences(ManifestDesc manifest) {
        Map<String, Set<String>> referrers = new LinkedHashMap<>();
        for (PackageDesc pkg : manifest.getPackagesList()) referrers.computeIfAbsent(pkg.getName(), k -> new LinkedHashSet<>());
        for (PackageDesc pkg : manifest.getPackagesList()) {
            for (RuleDesc rule : pkg.getRulesList()) {
                if (rule.hasPackage() && referrers.containsKey(rule.getPackage())) referrers.get(rule.getPackage()).add(pkg.getName());
            }
        }
        Map<String, List<String>> targets = new LinkedHashMap<>();
        List<Diagnostic> diagnostics = new ArrayList<>();
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
            targets.put(owner, referrers.keySet().stream().filter(name -> !ancestors.containsKey(name)).toList());
            for (int i = 0; i < pkg.getRulesCount(); ++i) {
                RuleDesc rule = pkg.getRules(i);
                if (!rule.hasPackage()) continue;
                String target = rule.getPackage();
                String context = " in rule '" + rule.getName() + "' of package '" + owner + "'";
                if (!referrers.containsKey(target)) {
                    diagnostics.add(new Diagnostic(Severity.ERROR, owner, i, "",
                            target.isEmpty() ? "Select a package" + context : "Unknown package '" + target + "'" + context));
                } else if (ancestors.containsKey(target)) {
                    List<String> cycle = new ArrayList<>();
                    cycle.add(owner);
                    for (String current = target; current != null; current = ancestors.get(current)) cycle.add(current);
                    diagnostics.add(new Diagnostic(Severity.ERROR, owner, i, "",
                            "Package reference cycle" + context + ": " + String.join(" -> ", cycle)));
                }
            }
        }
        return new PackageReferences(targets, diagnostics);
    }

    /** Matches the complete source inventory, including resources outside the bootstrap closure. */
    public static Selection select(ManifestDesc manifest, Collection<String> sourcePaths) {
        List<Diagnostic> diagnostics = new ArrayList<>();
        Map<RuleId, List<String>> matches = new LinkedHashMap<>();
        Set<String> names = new LinkedHashSet<>();
        Set<String> inventory = new TreeSet<>();
        for (String source : sourcePaths) inventory.add(normalize(source));
        for (PackageDesc pkg : manifest.getPackagesList()) {
            String name = pkg.getName();
            if (!name.matches("[a-zA-Z0-9][a-zA-Z0-9._-]*") || name.equalsIgnoreCase("base") ||
                    name.matches("(?i)(con|prn|aux|nul|com[1-9]|lpt[1-9])([.].*)?")) {
                diagnostics.add(new Diagnostic(Severity.ERROR, name, -1, "", "Invalid or reserved package name: " + name));
            }
            if (!names.add(name)) diagnostics.add(new Diagnostic(Severity.ERROR, name, -1, "", "Duplicate package: " + name));
            if (pkg.getProvider().isBlank()) diagnostics.add(new Diagnostic(Severity.ERROR, name, -1, "", "A provider is required"));
            for (int i = 0; i < pkg.getRulesCount(); ++i) {
                RuleDesc rule = pkg.getRules(i);
                RuleId id = new RuleId(name, i);
                if (rule.hasPath() == rule.hasPackage()) {
                    diagnostics.add(new Diagnostic(Severity.ERROR, name, i, "", "A rule must select either a path regex or a package"));
                } else if (rule.hasPath()) {
                    try {
                        Pattern pattern = Pattern.compile(rule.getPath());
                        List<String> selected = inventory.stream().filter(p -> pattern.matcher(p).matches()).toList();
                        matches.put(id, selected);
                        if (selected.isEmpty()) diagnostics.add(new Diagnostic(Severity.WARNING, name, i, "",
                                "Rule '" + rule.getName() + "' in package '" + name + "' matches no source files for Path '" + rule.getPath()
                                        + "'. Patterns match the entire source path, including its leading '/'."));
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
    private final Selection selection;
    private final List<Diagnostic> diagnostics;
    private final Set<String> resolving = new LinkedHashSet<>();

    private PackageResolver(ManifestDesc manifest, Map<String, List<String>> sourceOutputs, Map<String, Resource> resources) {
        for (PackageDesc pkg : manifest.getPackagesList()) descriptions.put(pkg.getName(), pkg);
        this.sourceOutputs = new LinkedHashMap<>();
        sourceOutputs.forEach((source, outputs) -> this.sourceOutputs.put(normalize(source), outputs.stream().map(PackageResolver::normalize).distinct().sorted().toList()));
        this.resources = immutableMap(resources);
        selection = select(manifest, this.sourceOutputs.keySet());
        diagnostics = new ArrayList<>(selection.diagnostics());
    }

    private Map<String, List<String>> closure(Collection<String> roots) {
        Map<String, List<String>> paths = new LinkedHashMap<>();
        ArrayDeque<List<String>> pending = new ArrayDeque<>();
        for (String root : roots) pending.add(List.of(root));
        while (!pending.isEmpty()) {
            List<String> path = pending.remove();
            String resource = path.get(path.size() - 1);
            if (paths.putIfAbsent(resource, path) != null) continue;
            Resource node = resources.get(resource);
            if (node != null) {
                for (String dependency : node.dependencies().keySet()) {
                    List<String> childPath = new ArrayList<>(path);
                    childPath.add(dependency);
                    pending.add(List.copyOf(childPath));
                }
            }
        }
        return paths;
    }

    private Package resolvePackage(String name) {
        Package existing = resolved.get(name);
        if (existing != null) return existing;
        PackageDesc description = descriptions.get(name);
        if (description == null) return null;
        if (!resolving.add(name)) {
            // Selection has already reported the offending reference rules.
            return null;
        }
        Map<String, LinkedHashSet<Contribution>> members = new LinkedHashMap<>();
        for (int i = 0; i < description.getRulesCount(); ++i) {
            RuleDesc rule = description.getRules(i);
            RuleId id = new RuleId(name, i);
            Map<String, List<Contribution>> selected = new LinkedHashMap<>();
            if (rule.hasPath()) {
                for (String source : selection.matches().getOrDefault(id, List.of())) {
                    closure(sourceOutputs.get(source)).forEach((output, path) ->
                            selected.computeIfAbsent(output, k -> new ArrayList<>()).add(new Contribution(List.of(id), source, path)));
                }
            } else if (rule.hasPackage()) {
                Package referenced = resolvePackage(rule.getPackage());
                if (referenced != null) {
                    referenced.provenance().forEach((output, contributions) -> {
                        List<Contribution> inherited = new ArrayList<>();
                        for (Contribution contribution : contributions) {
                            List<RuleId> rules = new ArrayList<>(contribution.rules());
                            rules.add(id);
                            inherited.add(new Contribution(rules, contribution.source(), contribution.dependencyPath()));
                        }
                        selected.put(output, inherited);
                    });
                }
            }
            if (rule.getOperation() == RuleOperation.REMOVE) {
                selected.keySet().forEach(members::remove);
            } else {
                selected.forEach((output, contributions) -> members.computeIfAbsent(output, k -> new LinkedHashSet<>()).addAll(contributions));
            }
        }
        Map<String, List<Contribution>> provenance = new LinkedHashMap<>();
        members.forEach((output, contributions) -> provenance.put(output, List.copyOf(contributions)));
        Package result = new Package(description, members.keySet(), provenance);
        resolving.remove(name);
        resolved.put(name, result);
        return result;
    }

    private Map<String, List<String>> startupLocks(Collection<String> roots) {
        Map<String, List<String>> locks = new LinkedHashMap<>();
        Set<String> expanded = new LinkedHashSet<>();
        ArrayDeque<List<String>> pending = new ArrayDeque<>();
        for (String root : roots) pending.add(List.of(root));
        while (!pending.isEmpty()) {
            List<String> path = pending.remove();
            String resource = path.get(path.size() - 1);
            locks.putIfAbsent(resource, path);
            if (!expanded.add(resource)) continue;
            Resource node = resources.get(resource);
            if (node == null) continue;
            node.dependencies().forEach((dependency, loading) -> {
                if (loading == Loading.DEFERRED) return;
                List<String> childPath = new ArrayList<>(path);
                childPath.add(dependency);
                List<String> frozen = List.copyOf(childPath);
                locks.putIfAbsent(dependency, frozen);
                if (loading == Loading.EAGER) pending.add(frozen);
            });
        }
        return locks;
    }

    public static Plan resolve(ManifestDesc manifest, Map<String, List<String>> sourceOutputs,
                               Map<String, Resource> resources, Collection<String> bootstrapRoots,
                               Collection<String> customRoots, boolean export) {
        return resolve(manifest, sourceOutputs, Map.of(), resources, bootstrapRoots, customRoots, export);
    }

    public static Plan resolve(ManifestDesc manifest, Map<String, List<String>> sourceOutputs,
                               Map<String, String> primarySources, Map<String, Resource> resources,
                               Collection<String> bootstrapRoots, Collection<String> customRoots, boolean export) {
        PackageResolver resolver = new PackageResolver(manifest, sourceOutputs, resources);
        Map<String, Package> packages = new LinkedHashMap<>();
        for (PackageDesc pkg : manifest.getPackagesList()) packages.put(pkg.getName(), resolver.resolvePackage(pkg.getName()));
        Set<String> roots = new LinkedHashSet<>(bootstrapRoots);
        roots.addAll(customRoots);
        Set<String> base = new TreeSet<>(resolver.closure(roots).keySet());
        Map<String, List<String>> locks = resolver.startupLocks(bootstrapRoots);
        Set<String> assigned = new TreeSet<>();
        for (Package pkg : packages.values()) assigned.addAll(pkg.resources());
        if (export) base.removeAll(assigned);
        else base.addAll(assigned);
        base.addAll(locks.keySet());
        Set<String> emitted = new TreeSet<>(base);
        emitted.addAll(assigned);
        for (String path : resources.keySet()) {
            if (!emitted.contains(path)) resolver.diagnostics.add(new Diagnostic(Severity.WARNING, "", -1, path, "Unassigned resource: " + path));
        }
        for (String path : emitted) {
            Resource node = resources.get(path);
            if (node == null) {
                resolver.diagnostics.add(new Diagnostic(Severity.ERROR, "", -1, path, "Missing compiled resource: " + path));
            } else {
                for (String dependency : node.dependencies().keySet()) {
                    if (!emitted.contains(dependency)) resolver.diagnostics.add(new Diagnostic(Severity.ERROR, "", -1, dependency, "Required by " + path + " but absent from the base and all packages"));
                }
            }
        }
        Map<String, Map<String, List<String>>> requirements = new LinkedHashMap<>();
        for (Package pkg : packages.values()) {
            Map<String, List<String>> external = new LinkedHashMap<>();
            for (String dependency : resolver.closure(pkg.resources()).keySet()) {
                if (base.contains(dependency) || pkg.resources().contains(dependency)) continue;
                List<String> alternatives = new ArrayList<>();
                for (Package supplier : packages.values()) {
                    if (supplier.resources().contains(dependency)) alternatives.add(supplier.description().getName());
                }
                external.put(dependency, List.copyOf(alternatives));
            }
            requirements.put(pkg.description().getName(), immutableMap(external));
        }
        return new Plan(packages, base, locks, resolver.sourceOutputs, primarySources, resources, resolver.diagnostics, requirements);
    }
}
