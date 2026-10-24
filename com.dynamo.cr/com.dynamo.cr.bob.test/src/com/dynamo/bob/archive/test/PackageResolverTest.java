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

package com.dynamo.bob.archive.test;

import com.dynamo.bob.archive.liveupdate.PackageResolver;
import com.dynamo.bob.archive.liveupdate.PackageResolver.Loading;
import com.dynamo.bob.archive.liveupdate.PackageResolver.Resource;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.ManifestDesc;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.PackageDesc;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.RuleDesc;
import com.dynamo.liveupdate.proto.LiveUpdateManifest.RuleOperation;
import org.junit.Test;
import java.util.List;
import java.util.Map;
import java.util.Set;
import static org.junit.Assert.*;

public class PackageResolverTest {
    private static RuleDesc path(String name, RuleOperation op, String regex) {
        return RuleDesc.newBuilder().setName(name).setOperation(op).setPath(regex).build();
    }
    private static RuleDesc reference(String name, String target) {
        return RuleDesc.newBuilder().setName(name).setPackage(target).build();
    }
    private static PackageDesc pkg(String name, RuleDesc... rules) {
        return PackageDesc.newBuilder().setName(name).addAllRules(List.of(rules)).build();
    }
    private static ManifestDesc manifest(PackageDesc... packages) {
        return ManifestDesc.newBuilder().addAllPackages(List.of(packages)).build();
    }
    private static Resource leaf() { return new Resource(Map.of()); }

    // Migration retains mixed targets and shared content reached through unchecked proxies in the base.
    @Test
    public void migrationPreservesIncludedCollections() {
        var manifest = PackageResolver.migrationManifest("legacy.zip", "zip",
                List.of("/mixed+[1].collection", "/excluded.collection"),
                List.of("/mixed+[1].collection", "/included.collection"));
        var plan = PackageResolver.resolve(manifest,
                Map.of("/mixed+[1].collection", List.of("/mixed.collectionc"),
                       "/included.collection", List.of("/included.collectionc"),
                       "/excluded.collection", List.of("/excluded.collectionc")),
                Map.of("/main.collectionc", new Resource(Map.of("/mixed.collectionc", Loading.DEFERRED,
                                "/included.collectionc", Loading.DEFERRED, "/excluded.collectionc", Loading.DEFERRED)),
                       "/mixed.collectionc", leaf(),
                       "/included.collectionc", new Resource(Map.of("/shared.scriptc", Loading.EAGER)),
                       "/excluded.collectionc", new Resource(Map.of("/shared.scriptc", Loading.EAGER)),
                       "/shared.scriptc", leaf()),
                List.of("/main.collectionc"), List.of(), true);
        assertFalse(plan.hasErrors());
        assertEquals(Set.of("/excluded.collectionc"), plan.packages().get("legacy.zip").resources());
        assertEquals(Set.of("/main.collectionc", "/mixed.collectionc", "/included.collectionc", "/shared.scriptc"), plan.base());
        assertEquals(List.of(RuleOperation.ADD, RuleOperation.ADD, RuleOperation.REMOVE, RuleOperation.REMOVE),
                manifest.getPackages(0).getRulesList().stream().map(RuleDesc::getOperation).toList());
    }

    // Ordered removal discards previous provenance and a subsequent add restores membership with its own rule.
    @Test
    public void orderedRulesAndProvenance() {
        ManifestDesc manifest = manifest(pkg("episode",
                path("initial", RuleOperation.ADD, "/episode[.]collection"),
                path("remove", RuleOperation.REMOVE, "/shared[.]png"),
                path("restore", RuleOperation.ADD, "/shared[.]png")));
        var plan = PackageResolver.resolve(manifest,
                Map.of("/episode.collection", List.of("/episode.collectionc"), "/shared.png", List.of("/shared.texturec")),
                Map.of("/episode.collectionc", new Resource(Map.of("/shared.texturec", Loading.EAGER)), "/shared.texturec", leaf()),
                List.of(), List.of(), true);
        assertFalse(plan.hasErrors());
        assertEquals(Set.of("/episode.collectionc", "/shared.texturec"), plan.packages().get("episode").resources());
        var provenance = plan.packages().get("episode").provenance().get("/shared.texturec");
        assertEquals(1, provenance.size());
        assertEquals(List.of(new PackageResolver.RuleId("episode", 2)), provenance.get(0).rules());
        assertEquals("/shared.png", provenance.get(0).source());
    }

    // References use final contents, allow forward references, and retain both the original and referring rule.
    @Test
    public void forwardReferencesAndOverlap() {
        var plan = PackageResolver.resolve(manifest(
                pkg("first", reference("include", "later")),
                pkg("later", path("all", RuleOperation.ADD, "/.*"), path("remove", RuleOperation.REMOVE, "/removed"))),
                Map.of("/kept", List.of("/kept"), "/removed", List.of("/removed")),
                Map.of("/kept", leaf(), "/removed", leaf()), List.of(), List.of(), true);
        assertFalse(plan.hasErrors());
        assertEquals(Set.of("/kept"), plan.packages().get("first").resources());
        assertEquals(plan.packages().get("later").resources(), plan.packages().get("first").resources());
        assertEquals(List.of(new PackageResolver.RuleId("later", 0), new PackageResolver.RuleId("first", 0)),
                plan.packages().get("first").provenance().get("/kept").get(0).rules());
        assertTrue(plan.diagnostics().stream().anyMatch(d -> d.resource().equals("/removed")));
    }

    // Regexes match the complete normalized path; diagnostics identify the rule, pattern, and one-based error column.
    @Test
    public void regexDiagnostics() {
        var selection = PackageResolver.select(manifest(pkg("pack",
                path("valid", RuleOperation.ADD, "/episodes/0[0-9]/.*[.]collection"),
                path("partial", RuleOperation.ADD, "episodes"),
                path("bad", RuleOperation.ADD, "["),
                path("bad later", RuleOperation.ADD, "/episodes/["),
                path("unfinished", RuleOperation.ADD, "/episodes/("))), List.of("episodes/01/main.collection", "/episodes/10/main.collection"));
        assertEquals(List.of("/episodes/01/main.collection"), selection.matches().get(new PackageResolver.RuleId("pack", 0)));
        assertEquals(4, selection.diagnostics().size());
        assertEquals(PackageResolver.Severity.WARNING, selection.diagnostics().get(0).severity());
        assertEquals("Rule 'partial' in package 'pack' matches no source files for Path 'episodes'. "
                        + "Patterns match the entire source path, including its leading '/'.",
                selection.diagnostics().get(0).message());
        var errors = selection.diagnostics().subList(1, 4);
        assertTrue(errors.stream().allMatch(d -> d.severity() == PackageResolver.Severity.ERROR));
        assertEquals(List.of(
                "Invalid regex in rule 'bad' of package 'pack' at column 1: Unclosed character class. Path: [",
                "Invalid regex in rule 'bad later' of package 'pack' at column 11: Unclosed character class. Path: /episodes/[",
                "Invalid regex in rule 'unfinished' of package 'pack' at column 12: Unclosed group. Path: /episodes/("),
                errors.stream().map(PackageResolver.Diagnostic::message).toList());
    }

    // Choices exclude the current package and direct/transitive referrers, including Remove rules and forward references.
    @Test
    public void packageReferenceChoices() {
        var manifest = manifest(pkg("a", reference("include", "b")),
                pkg("b", reference("subtract", "c").toBuilder().setOperation(RuleOperation.REMOVE).build()),
                pkg("c"), pkg("independent"));
        var references = PackageResolver.packageReferences(manifest);
        assertTrue(references.diagnostics().isEmpty());
        assertEquals(Map.of("a", List.of("b", "c", "independent"),
                            "b", List.of("c", "independent"),
                            "c", List.of("independent"),
                            "independent", List.of("a", "b", "c")), references.targets());

        // Removing an edge makes its previous referrers selectable again.
        var edited = manifest.toBuilder().setPackages(1, pkg("b")).build();
        assertEquals(List.of("a", "b", "independent"), PackageResolver.packageReferences(edited).targets().get("c"));
        assertEquals(List.of(), PackageResolver.packageReferences(manifest(pkg("only"))).targets().get("only"));
    }

    // Selection rejects missing, empty, self, and transitive cyclic references before building any resources.
    @Test
    public void invalidReferences() {
        var manifest = manifest(pkg("a", reference("include", "b")),
                pkg("b", reference("include", "c")),
                pkg("c", reference("subtract", "a").toBuilder().setOperation(RuleOperation.REMOVE).build(),
                        reference("missing", "absent"), reference("empty", "")),
                pkg("self", reference("self", "self")), pkg("independent"));
        var selection = PackageResolver.select(manifest, List.of());
        assertEquals(List.of(
                new PackageResolver.Diagnostic(PackageResolver.Severity.ERROR, "a", 0, "", "Package reference cycle in rule 'include' of package 'a': a -> b -> c -> a"),
                new PackageResolver.Diagnostic(PackageResolver.Severity.ERROR, "b", 0, "", "Package reference cycle in rule 'include' of package 'b': b -> c -> a -> b"),
                new PackageResolver.Diagnostic(PackageResolver.Severity.ERROR, "c", 0, "", "Package reference cycle in rule 'subtract' of package 'c': c -> a -> b -> c"),
                new PackageResolver.Diagnostic(PackageResolver.Severity.ERROR, "c", 1, "", "Unknown package 'absent' in rule 'missing' of package 'c'"),
                new PackageResolver.Diagnostic(PackageResolver.Severity.ERROR, "c", 2, "", "Select a package in rule 'empty' of package 'c'"),
                new PackageResolver.Diagnostic(PackageResolver.Severity.ERROR, "self", 0, "", "Package reference cycle in rule 'self' of package 'self': self -> self")),
                selection.diagnostics());
        var plan = PackageResolver.resolve(manifest, Map.of(), Map.of(), List.of(), List.of(), true);
        assertTrue(plan.hasErrors());
        assertEquals(selection.diagnostics(), plan.diagnostics());
        assertFalse(PackageResolver.packageReferences(manifest).targets().get("a").contains("c"));

        var repaired = manifest.toBuilder().setPackages(2, pkg("c", reference("subtract", "independent")))
                .setPackages(3, pkg("self")).build();
        assertTrue(PackageResolver.select(repaired, List.of()).diagnostics().isEmpty());
    }

    // Startup traversal distinguishes deferred prototypes from descriptor-only loads and restores eagerly shared resources.
    @Test
    public void startupLocks() {
        Map<String, Resource> graph = Map.of(
                "/main", new Resource(Map.of("/proxy", Loading.EAGER, "/factory", Loading.EAGER, "/dynamic", Loading.EAGER, "/cf", Loading.EAGER, "/shared", Loading.EAGER)),
                "/proxy", new Resource(Map.of("/level", Loading.DEFERRED)),
                "/factory", new Resource(Map.of("/static-go", Loading.EAGER)),
                "/dynamic", new Resource(Map.of("/dynamic-go", Loading.DEFERRED)),
                "/cf", new Resource(Map.of("/collection", Loading.DESCRIPTOR)),
                "/collection", new Resource(Map.of("/dynamic-go", Loading.EAGER)),
                "/level", new Resource(Map.of("/shared", Loading.EAGER)),
                "/shared", leaf(), "/static-go", leaf(), "/dynamic-go", leaf());
        var plan = PackageResolver.resolve(manifest(pkg("pack", path("all", RuleOperation.ADD, "/all"))),
                Map.of("/all", List.copyOf(graph.keySet())), graph, List.of("/main"), List.of(), true);
        assertEquals(Set.of("/main", "/proxy", "/factory", "/dynamic", "/cf", "/collection", "/static-go", "/shared"), plan.base());
        assertFalse(plan.locks().containsKey("/dynamic-go"));
        assertEquals(List.of("/main", "/shared"), plan.locks().get("/shared"));
        var eager = PackageResolver.resolve(manifest(pkg("pack", path("all", RuleOperation.ADD, "/all"))),
                Map.of("/all", List.copyOf(graph.keySet())), graph, List.of("/main", "/collection"), List.of(), true);
        assertTrue(eager.locks().containsKey("/dynamic-go"));
    }

    // Package requirements retain alternative suppliers and exclude resources already provided by the base.
    @Test
    public void dependencyAlternativesAndMissingContent() {
        var descriptions = manifest(pkg("level", path("level", RuleOperation.ADD, "/level"), path("separate", RuleOperation.REMOVE, "/shared")),
                pkg("common", path("shared", RuleOperation.ADD, "/shared")), pkg("backup", reference("copy", "common")));
        var mappings = Map.of("/level", List.of("/levelc"), "/shared", List.of("/sharedc"));
        var graph = Map.of("/levelc", new Resource(Map.of("/sharedc", Loading.EAGER)), "/sharedc", leaf());
        var plan = PackageResolver.resolve(descriptions, mappings, graph, List.of(), List.of(), true);
        assertEquals(List.of("common", "backup"), plan.requirements().get("level").get("/sharedc"));
        assertFalse(plan.hasErrors());
        var baseSupplies = PackageResolver.resolve(descriptions, mappings, graph, List.of("/sharedc"), List.of(), true);
        assertTrue(baseSupplies.requirements().get("level").isEmpty());
        var missing = PackageResolver.resolve(manifest(descriptions.getPackages(0)), mappings, graph, List.of(), List.of(), true);
        assertTrue(missing.hasErrors());
        assertTrue(missing.diagnostics().stream().anyMatch(d -> d.message().contains("Required by /levelc")));
    }

    // Actual source mappings can name several generated outputs and opaque custom files without extension substitution.
    @Test
    public void multipleOutputsAndDisabledExport() {
        var plan = PackageResolver.resolve(manifest(pkg("content", path("all", RuleOperation.ADD, "/.*"))),
                Map.of("/atlas.atlas", List.of("/atlas.a.texturesetc", "/generated.texturec"), "/custom.json", List.of("/custom.json")),
                Map.of("/atlas.a.texturesetc", new Resource(Map.of("/generated.texturec", Loading.EAGER)), "/generated.texturec", leaf(), "/custom.json", leaf()),
                List.of(), List.of(), false);
        assertEquals(Set.of("/atlas.a.texturesetc", "/generated.texturec", "/custom.json"), plan.base());
        assertEquals(plan.base(), plan.packages().get("content").resources());
        assertEquals(List.of("/generated.texturec"), plan.packages().get("content").provenance().get("/generated.texturec").get(0).dependencyPath());
    }
}
