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

package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

import org.junit.Assert;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Progress;
import com.dynamo.bob.Task;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gamesys.proto.Gui.NodeDesc;
import com.dynamo.gamesys.proto.Gui.NodeDescOrBuilder;
import com.dynamo.gamesys.proto.Gui.Property;
import com.dynamo.gamesys.proto.Gui.Property.PropertyType;
import com.dynamo.gamesys.proto.Gui.SceneDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.LayoutDesc;
import com.dynamo.gamesys.proto.Gui.SceneDesc.ResourceDesc;
import com.google.protobuf.Descriptors.FieldDescriptor;

public class GuiCustomNodeTest extends AbstractProtoBuilderTest {
    @GuiCustomNode(type = "LifecycleTest")
    public static class LifecycleNode implements IGuiCustomNode {
        private final Map<String, NodeDesc> createdNodes = new HashMap<>();
        private final Map<String, IResource> compiledDependencies = new HashMap<>();
        private GuiBuildContext buildContext;

        public static void registerProperties(IGuiCustomType type) {
            type.addProperty("animation", "idle", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_DEFAULT);
            type.addProperty("scene", "", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_RESOURCE);
            type.addProperty("dependency", "", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_DEFAULT);
            type.addProperty("compiled_dependency", "", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_DEFAULT);
        }

        private String nodeKey(NodeDescOrBuilder node, String layoutName) {
            return layoutName + ":" + node.getId();
        }

        @Override
        public void create(GuiBuildContext context, Task.TaskBuilder<?> taskBuilder, NodeDesc node, String layoutName) throws CompileExceptionError {
            if (buildContext == null) {
                buildContext = context;
            }
            Assert.assertSame("All callbacks in this GUI task must share their context", buildContext, context);
            Assert.assertEquals(taskBuilder.firstInput(), context.getResource());
            Assert.assertNull("create must run once per resolved node", createdNodes.put(nodeKey(node, layoutName), node));
            if (GuiBuildContext.getProperty(node, "animation").getString().equals("invalid-create")) {
                throw context.error(node, layoutName, "Invalid animation during create");
            }
            String dependency = GuiBuildContext.getProperty(node, "dependency").getString();
            if (!dependency.isEmpty()) {
                taskBuilder.addInput(context.getProject().getResource(dependency));
            }
            String compiledDependency = GuiBuildContext.getProperty(node, "compiled_dependency").getString();
            if (!compiledDependency.isEmpty()) {
                Task task = context.getProject().createTask(context.getProject().getResource(compiledDependency));
                taskBuilder.addInputsFromOutputs(task);
                compiledDependencies.put(nodeKey(node, layoutName), task.output(0));
            }
            taskBuilder.addExtraCacheKey(context.getProject().option("custom-node-version", "v1"));
        }

        @Override
        public void build(GuiBuildContext context, Task task, NodeDesc.Builder node, String layoutName) throws IOException, CompileExceptionError {
            Assert.assertSame(buildContext, context);
            NodeDesc created = createdNodes.remove(nodeKey(node, layoutName));
            Assert.assertNotNull("build must use the instance that ran create", created);
            Assert.assertEquals(created, node.build());
            Assert.assertEquals(context.getResource(), task.firstInput());

            String animation = GuiBuildContext.getProperty(node, "animation").getString();
            if (animation.equals("invalid-build")) {
                throw context.error(node, layoutName, "Invalid animation during build");
            }
            node.setText(animation);
            node.addCustomProperties(Property.newBuilder().setId("compiled_animation")
                    .setType(PropertyType.TYPE_HASH).setString(animation));
            IResource scene = context.getResources().get(GuiBuildContext.getProperty(node, "scene").getString());
            if (scene != null) {
                node.addCustomProperties(stringProperty("source_path", scene.getPath()));
            }
            String dependency = GuiBuildContext.getProperty(node, "dependency").getString();
            if (!dependency.isEmpty()) {
                String content = new String(context.getProject().getResource(dependency).getContent(), StandardCharsets.UTF_8);
                if (content.equals("invalid")) {
                    throw context.error(node, layoutName, "Invalid dependency");
                }
                node.addCustomProperties(stringProperty("dependency_text", content));
            }
            IResource compiledDependency = compiledDependencies.get(nodeKey(node, layoutName));
            if (compiledDependency != null) {
                SceneDesc compiled = SceneDesc.parseFrom(compiledDependency.getContent());
                node.addCustomProperties(stringProperty("compiled_text", compiled.getNodes(0).getText()));
            }
        }
    }

    @GuiCustomNode(type = "BuildOnlyTest")
    public static class BuildOnlyNode implements IGuiCustomNode {
        @Override
        public void build(GuiBuildContext context, Task task, NodeDesc.Builder node, String layoutName) {
            node.setText("built");
        }
    }

    @GuiCustomNode(type = "CreateOnlyTest")
    public static class CreateOnlyNode implements IGuiCustomNode {
        @Override
        public void create(GuiBuildContext context, Task.TaskBuilder<?> taskBuilder, NodeDesc node, String layoutName) {
            taskBuilder.addInput(context.getProject().getResource("/create-only.data"));
        }
    }

    @GuiCustomNode(type = "ContextTest")
    public static class ContextNode implements IGuiCustomNode {
        private static final Map<String, GuiBuildContext> contexts = new ConcurrentHashMap<>();
        private static final Map<GuiBuildContext, Set<String>> invocations = new ConcurrentHashMap<>();

        private static void record(GuiBuildContext context, NodeDescOrBuilder node, String layoutName, String phase) {
            GuiBuildContext previous = contexts.putIfAbsent(context.getResource().getPath(), context);
            if (previous != null) {
                Assert.assertSame("All custom types, nodes, layouts and phases must share one GUI context", previous, context);
            }
            Assert.assertTrue(invocations.computeIfAbsent(context, key -> ConcurrentHashMap.newKeySet())
                    .add(phase + ":" + layoutName + ":" + node.getId()));
        }

        @Override
        public void create(GuiBuildContext context, Task.TaskBuilder<?> taskBuilder, NodeDesc node, String layoutName) {
            Assert.assertEquals(taskBuilder.firstInput(), context.getResource());
            record(context, node, layoutName, "create");
        }

        @Override
        public void build(GuiBuildContext context, Task task, NodeDesc.Builder node, String layoutName) {
            Assert.assertEquals(task.firstInput(), context.getResource());
            record(context, node, layoutName, "build");
        }
    }

    @GuiCustomNode(type = "OtherContextTest")
    public static class OtherContextNode extends ContextNode {
    }

    @GuiCustomNode(type = "LegacyTest")
    public static class LegacyNode implements IGuiCustomNode {
        private LegacyNode() {
            Assert.fail("Static-only plugins must not be instantiated");
        }

        public static void registerProperties(IGuiCustomType type) {
            type.addProperty("value", "legacy", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_DEFAULT);
        }
    }

    @GuiCustomNode(type = "LegacyTest")
    public static class UpgradedLegacyNode extends BuildOnlyNode {
        public static void registerProperties(IGuiCustomType type) {
            LegacyNode.registerProperties(type);
        }
    }

    // The registration-only API used by Spine 4.9.4, before source migration callbacks.
    @GuiCustomNode(type = "Spine")
    public static class OldSpineNode implements IGuiCustomNode {
        public static void registerProperties(IGuiCustomType type) {
            type.addProperty("spine_scene", "", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_RESOURCE);
            type.addProperty("spine_default_animation", "", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_DEFAULT);
            type.addProperty("spine_skin", "", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_DEFAULT);
            type.addProperty("spine_create_bones", false, PropertyType.TYPE_BOOLEAN, IGuiCustomType.EDIT_TYPE_DEFAULT);
        }

        public static void migrateProperties(Map<String, Object> properties) {
            properties.remove("spine_node_child");
        }
    }

    @GuiCustomNode(type = "SourceMigration")
    public static class SourceMigrationNode implements IGuiCustomNode {
        private final Set<String> migratedSources = new HashSet<>();
        private static String migrationVersion = "v1";

        private void migrateNode(NodeDesc.Builder node) {
            if (node.getCustomTypeName().equals("OldLifecycleTest")) {
                node.setCustomTypeName("LifecycleTest");
            }
            if (node.getText().startsWith("legacy:")) {
                Assert.assertEquals("Migration must run before defaults and inheritance", 0, node.getCustomPropertiesCount());
                node.addCustomProperties(stringProperty("animation", node.getText().substring("legacy:".length())));
                node.clearText();
                List<Integer> fields = node.getOverriddenFieldsList().stream()
                        .filter(field -> field != NodeDesc.TEXT_FIELD_NUMBER).toList();
                node.clearOverriddenFields().addAllOverriddenFields(fields);
            }
        }

        @Override
        public void migrateScene(GuiSceneMigrationContext context) throws CompileExceptionError {
            Assert.assertTrue("Migrate each source once per GUI task", migratedSources.add(context.getResource().getPath()));
            context.addExtraCacheKey("source-migration-test-" + migrationVersion);
            SceneDesc.Builder scene = context.getSceneBuilder();
            if (scene.getScript().equals("migration-error")) {
                throw context.error("Invalid legacy source");
            }
            if (scene.getScript().startsWith("legacy-resource:")) {
                scene.addResources(resource("scene", scene.getScript().substring("legacy-resource:".length())));
                scene.clearScript();
            }
            for (NodeDesc.Builder node : scene.getNodesBuilderList()) {
                migrateNode(node);
            }
            for (LayoutDesc.Builder layout : scene.getLayoutsBuilderList()) {
                for (NodeDesc.Builder node : layout.getNodesBuilderList()) {
                    migrateNode(node);
                }
            }
        }
    }

    @GuiCustomNode(type = "MigratingLifecycleTest")
    public static class MigratingLifecycleNode extends LifecycleNode {
        private final Set<String> sourcePaths = new HashSet<>();

        @Override
        public void migrateScene(GuiSceneMigrationContext context) {
            sourcePaths.add(context.getResource().getPath());
        }

        @Override
        public void build(GuiBuildContext context, Task task, NodeDesc.Builder node, String layoutName) throws IOException, CompileExceptionError {
            Assert.assertTrue("Migration and node callbacks must use the same plugin instance",
                    sourcePaths.contains(context.getResource().getPath()));
            super.build(context, task, node, layoutName);
        }
    }

    public GuiCustomNodeTest() {
        getProject().getGuiCustomTypeRegistry().register(LifecycleNode.class);
        getProject().getGuiCustomTypeRegistry().register(BuildOnlyNode.class);
        getProject().getGuiCustomTypeRegistry().register(CreateOnlyNode.class);
        getProject().getGuiCustomTypeRegistry().register(LegacyNode.class);
        getProject().getGuiCustomTypeRegistry().register(SourceMigrationNode.class);
        getProject().getGuiCustomTypeRegistry().register(MigratingLifecycleNode.class);
    }

    private static SceneDesc.Builder scene() {
        return SceneDesc.newBuilder().setMaterial("");
    }

    private static NodeDesc.Builder node(String id, String type) {
        return NodeDesc.newBuilder().setId(id).setType(NodeDesc.Type.TYPE_CUSTOM).setCustomTypeName(type);
    }

    private static Property stringProperty(String name, String value) {
        return Property.newBuilder().setId(name).setType(PropertyType.TYPE_STRING).setString(value).build();
    }

    private static Property property(NodeDesc node, String name) {
        return node.getCustomPropertiesList().stream()
                .filter(property -> property.getIdHash() == MurmurHash.hash64(name)).findFirst().orElseThrow();
    }

    private static NodeDesc findNode(List<NodeDesc> nodes, String id) {
        return nodes.stream().filter(node -> node.getId().equals(id)).findFirst().orElseThrow();
    }

    private void addScene(String path, SceneDesc.Builder scene) {
        addFile(path, scene.build().toString());
    }

    private SceneDesc buildScene(SceneDesc.Builder scene) throws Exception {
        return getMessage(build("/test.gui", scene.build().toString()), SceneDesc.class);
    }

    private static NodeDesc.Builder template(String id, String path) {
        return NodeDesc.newBuilder().setId(id).setType(NodeDesc.Type.TYPE_TEMPLATE).setTemplate(path);
    }

    private static ResourceDesc resource(String name, String path) {
        return ResourceDesc.newBuilder().setName(name).setPath(path).build();
    }

    private void assertMigrationError(SceneDesc.Builder source, String resourcePath, String location, String field) throws Exception {
        addScene("/test.gui", source);
        GuiBuilder builder = new GuiBuilder();
        builder.setProject(getProject());
        try {
            builder.create(getProject().getResource("/test.gui"));
            Assert.fail("Unmigrated fields must fail during task creation, even when a cached output exists");
        } catch (CompileExceptionError e) {
            Assert.assertEquals(resourcePath, e.getResource().getPath());
            Assert.assertEquals(location + "Legacy GUI field '" + field
                    + "' was not migrated. Install or update the GUI plugin that handles this field.", e.getMessage());
        }
    }

    // New Bob with a registration-only Spine plugin must reject legacy fields instead of emitting empty defaults.
    @Test
    public void testOldPluginRejectsUnmigratedCustomNodeFields() throws Exception {
        getProject().getGuiCustomTypeRegistry().register(OldSpineNode.class);
        for (boolean named : List.of(true, false)) {
            for (FieldDescriptor field : NodeDesc.getDescriptor().getFields()) {
                if (!field.getOptions().getDeprecated()) {
                    continue;
                }
                NodeDesc.Builder node = node("actor", "Spine");
                if (!named) {
                    node.clearCustomTypeName().setCustomType(MurmurHash.hash32("Spine"));
                }
                // Explicit empty/false values still need migration, particularly in overrides.
                node.setField(field, field.getDefaultValue());
                assertMigrationError(scene().addNodes(node), "test.gui", "Node 'actor': ", field.getName());
            }
        }
    }

    // An anonymous template/layout override may contain only a legacy field marker selecting its default.
    @Test
    public void testUnmigratedSparseOverridesFailBeforeInheritance() throws Exception {
        NodeDesc.Builder override = NodeDesc.newBuilder().setId("template/actor").setType(NodeDesc.Type.TYPE_CUSTOM)
                .setTemplateNodeChild(true).addOverriddenFields(NodeDesc.SPINE_CREATE_BONES_FIELD_NUMBER);
        assertMigrationError(scene().addNodes(override), "test.gui", "Node 'template/actor': ", "spine_create_bones");
        assertMigrationError(scene().addLayouts(LayoutDesc.newBuilder().setName("Portrait").addNodes(override)),
                "test.gui", "Node 'template/actor', layout 'Portrait': ", "spine_create_bones");
    }

    // Legacy template fields identify their declaring source, and legacy resource lists fail even without nodes.
    @Test
    public void testUnmigratedTemplateFieldsAndResourceLists() throws Exception {
        addScene("/template.gui", scene().addNodes(node("actor", "Spine").setSpineScene("scene")));
        assertMigrationError(scene().addNodes(template("template", "/template.gui")),
                "template.gui", "Node 'actor': ", "spine_scene");
        assertMigrationError(scene().addSpineScenes(SceneDesc.SpineSceneDesc.newBuilder()
                .setName("scene").setSpineScene("/unused.spinescene")), "test.gui", "", "spine_scenes");
    }

    // Reuse one context across custom types, nodes, layouts and phases without sharing it between GUI tasks.
    @Test
    public void testContextSharedWithinGuiTask() throws Exception {
        getProject().getGuiCustomTypeRegistry().register(ContextNode.class);
        getProject().getGuiCustomTypeRegistry().register(OtherContextNode.class);
        ContextNode.contexts.clear();
        ContextNode.invocations.clear();
        try {
            addScene("/child.gui", scene().addNodes(node("child", "ContextTest")));
            buildScene(scene().addResources(resource("child", "/child.gui"))
                    .addNodes(node("first", "ContextTest"))
                    .addNodes(node("second", "OtherContextTest"))
                    .addLayouts(LayoutDesc.newBuilder().setName("Landscape")
                            .addNodes(node("first", "ContextTest"))
                            .addNodes(node("second", "OtherContextTest"))));
            Assert.assertEquals(2, ContextNode.contexts.size());
            GuiBuildContext parent = ContextNode.contexts.get("test.gui");
            GuiBuildContext child = ContextNode.contexts.get("child.gui");
            Assert.assertNotSame(parent, child);
            Assert.assertEquals(8, ContextNode.invocations.get(parent).size());
            Assert.assertEquals(2, ContextNode.invocations.get(child).size());
        } finally {
            ContextNode.contexts.clear();
            ContextNode.invocations.clear();
        }
    }

    // Cached migrations must include later registrations in name order and replace re-registered types.
    @Test
    public void testSceneMigrationRegistryUpdatesAfterRegistration() {
        GuiCustomTypeRegistry registry = new GuiCustomTypeRegistry();
        Assert.assertTrue(registry.getSceneMigrations().isEmpty());
        registry.register(LifecycleNode.class);
        Assert.assertTrue(registry.getSceneMigrations().isEmpty());

        registry.register(SourceMigrationNode.class);
        List<GuiCustomTypeRegistry.Type> firstMigrations = registry.getSceneMigrations();
        Assert.assertEquals(List.of(registry.getByName("SourceMigration")), firstMigrations);

        registry.register(MigratingLifecycleNode.class);
        Assert.assertEquals(List.of(registry.getByName("MigratingLifecycleTest"), registry.getByName("SourceMigration")),
                registry.getSceneMigrations());

        registry.register(SourceMigrationNode.class);
        Assert.assertEquals(List.of(registry.getByName("MigratingLifecycleTest"), registry.getByName("SourceMigration")),
                registry.getSceneMigrations());
        Assert.assertNotSame(firstMigrations.get(0), registry.getSceneMigrations().get(1));
    }

    // Migrate raw templates and sparse layouts once, before resolving type aliases, defaults and overrides.
    @Test
    public void testSourceMigrationBeforeInheritance() throws Exception {
        addScene("/template.gui", scene().addNodes(node("actor", "OldLifecycleTest").setText("legacy:base")));
        SceneDesc result = buildScene(scene().addNodes(template("first", "/template.gui"))
                .addNodes(template("second", "/template.gui"))
                .addNodes(node("first/actor", "OldLifecycleTest").setTemplateNodeChild(true).setText("legacy:parent")
                        .addOverriddenFields(NodeDesc.TEXT_FIELD_NUMBER))
                .addLayouts(LayoutDesc.newBuilder().setName("Landscape")
                        .addNodes(node("first/actor", "OldLifecycleTest").setTemplateNodeChild(true).setText("legacy:layout")
                                .addOverriddenFields(NodeDesc.TEXT_FIELD_NUMBER))));
        Assert.assertEquals("parent", findNode(result.getNodesList(), "first/actor").getText());
        Assert.assertEquals("base", findNode(result.getNodesList(), "second/actor").getText());
        Assert.assertEquals("layout", findNode(result.getLayouts(0).getNodesList(), "first/actor").getText());
    }

    // Migration state remains available to create/build on the same plugin instance for the owning GUI task.
    @Test
    public void testMigrationSharesNodeCallbackInstance() throws Exception {
        SceneDesc result = buildScene(scene().addNodes(node("actor", "MigratingLifecycleTest")));
        Assert.assertEquals("idle", result.getNodes(0).getText());
    }

    // Migration-created resources must become task dependencies even when no custom nodes use callbacks.
    @Test
    public void testSourceMigrationBeforeDependencies() throws Exception {
        addScene("/resource.gui", scene().addNodes(NodeDesc.newBuilder().setId("box").setType(NodeDesc.Type.TYPE_BOX)));
        addScene("/test.gui", scene().setScript("legacy-resource:/resource.gui"));
        TaskResult result = rebuild();
        Assert.assertEquals(result.getMessage(), TaskResult.Result.SUCCESS, result.getResult());
        Assert.assertTrue(result.getTask().getInputs().contains(getProject().getResource("/resource.gui").changeExt(".guic")));
        SceneDesc compiled = SceneDesc.parseFrom(result.getTask().output(0).getContent());
        Assert.assertEquals("/resource.guic", compiled.getResources(0).getPath());
        Assert.assertTrue(compiled.getScript().isEmpty());
    }

    // A migration version change invalidates otherwise identical output, including migration-only GUIs.
    @Test
    public void testSourceMigrationCacheInvalidation() throws Exception {
        addScene("/test.gui", scene());
        Assert.assertEquals(TaskResult.Result.SUCCESS, rebuild().getResult());
        Assert.assertEquals(TaskResult.Result.SKIPPED, rebuild().getResult());
        try {
            SourceMigrationNode.migrationVersion = "v2";
            Assert.assertEquals(TaskResult.Result.SUCCESS, rebuild().getResult());
            Assert.assertEquals(TaskResult.Result.SKIPPED, rebuild().getResult());
        } finally {
            SourceMigrationNode.migrationVersion = "v1";
        }
    }

    // Migration errors identify the template source and occur before Bob scans its legacy resource paths.
    @Test
    public void testSourceMigrationErrorLocation() throws Exception {
        addScene("/template.gui", scene().setScript("migration-error"));
        try {
            buildScene(scene().addNodes(template("template", "/template.gui")));
            Assert.fail("Invalid legacy sources must fail during migration");
        } catch (CompileExceptionError e) {
            Assert.assertEquals("template.gui", e.getResource().getPath());
            Assert.assertEquals("Invalid legacy source", e.getMessage());
        }
    }

    // Named resource checks must see parent overrides and resources supplied by the containing GUI.
    @Test
    public void testResolvedCustomResourceValidation() throws Exception {
        addScene("/resource.gui", scene());
        addScene("/template.gui", scene().addNodes(node("actor", "LifecycleTest")
                .addCustomProperties(stringProperty("scene", "missing"))));
        SceneDesc.Builder root = scene().addResources(resource("scene", "/resource.gui"))
                .addNodes(template("template", "/template.gui"))
                .addNodes(node("template/actor", "LifecycleTest").setTemplateNodeChild(true)
                        .addCustomProperties(stringProperty("scene", "scene")));
        Assert.assertEquals("resource.gui", property(buildScene(root).getNodes(0), "source_path").getString());

        root.addLayouts(LayoutDesc.newBuilder().setName("Landscape")
                .addNodes(node("template/actor", "LifecycleTest").setTemplateNodeChild(true)
                        .addCustomProperties(stringProperty("scene", "still-missing"))));
        try {
            buildScene(root);
            Assert.fail("Unresolved final resources must fail the build");
        } catch (CompileExceptionError e) {
            Assert.assertEquals("Node 'template/actor', layout 'Landscape': the resource 'still-missing' can't be found", e.getMessage());
        }
    }

    // Both callbacks see defaults, and properties added by build are compiled for the runtime.
    @Test
    public void testDefaultsAndOutputTransformation() throws Exception {
        SceneDesc result = buildScene(scene().addNodes(node("actor", "LifecycleTest")));
        NodeDesc node = result.getNodes(0);
        Assert.assertEquals("idle", node.getText());
        Assert.assertEquals(MurmurHash.hash64("idle"), property(node, "compiled_animation").getHash());
        Assert.assertFalse(property(node, "compiled_animation").hasId());
        Assert.assertFalse(node.hasCustomTypeName());
    }

    // Nested templates and layouts must resolve before create, with root resource names taking precedence.
    @Test
    public void testResolvedTemplatesLayoutsAndSourceResources() throws Exception {
        addScene("/template-resource.gui", scene());
        addScene("/root-resource.gui", scene());
        addScene("/inherited.gui", scene());
        addScene("/template.gui", scene()
                .addResources(resource("scene", "/template-resource.gui"))
                .addResources(resource("inherited", "/inherited.gui"))
                .addNodes(node("actor", "LifecycleTest").addCustomProperties(stringProperty("scene", "scene"))
                        .addCustomProperties(stringProperty("animation", "template")))
                .addNodes(node("inherited", "LifecycleTest").addCustomProperties(stringProperty("scene", "inherited"))));
        addScene("/middle.gui", scene().addNodes(template("inner", "/template.gui")));
        SceneDesc.Builder source = scene()
                .addResources(resource("scene", "/root-resource.gui"))
                .addNodes(template("outer", "/middle.gui"))
                .addNodes(node("outer/inner/actor", "LifecycleTest").setTemplateNodeChild(true)
                        .addCustomProperties(stringProperty("animation", "root")))
                .addLayouts(LayoutDesc.newBuilder().setName("Landscape")
                        .addNodes(node("outer/inner/actor", "LifecycleTest").setTemplateNodeChild(true)
                                .addCustomProperties(stringProperty("animation", "layout"))));

        SceneDesc result = buildScene(source);
        NodeDesc defaultNode = findNode(result.getNodesList(), "outer/inner/actor");
        NodeDesc layoutNode = findNode(result.getLayouts(0).getNodesList(), "outer/inner/actor");
        Assert.assertEquals("root", defaultNode.getText());
        Assert.assertEquals("layout", layoutNode.getText());
        Assert.assertEquals("root-resource.gui", property(defaultNode, "source_path").getString());
        Assert.assertEquals("root-resource.gui", property(layoutNode, "source_path").getString());
        Assert.assertEquals("inherited.gui", property(findNode(result.getNodesList(), "outer/inner/inherited"), "source_path").getString());
        Assert.assertEquals("/root-resource.guic", result.getResources(0).getPath());
    }

    // Sibling and repeated templates must give callbacks the same winning resources that the runtime GUI uses.
    @Test
    public void testSourceAndCompiledResourcePrecedenceMatch() throws Exception {
        for (String name : List.of("leaf", "first", "second", "root")) {
            addScene("/" + name + "-resource.gui", scene());
        }
        addScene("/leaf.gui", scene().addResources(resource("scene", "/leaf-resource.gui"))
                .addNodes(node("actor", "LifecycleTest").addCustomProperties(stringProperty("scene", "scene"))));
        addScene("/first.gui", scene().addResources(resource("scene", "/first-resource.gui"))
                .addNodes(template("leaf", "/leaf.gui")));
        addScene("/second.gui", scene().addResources(resource("scene", "/second-resource.gui"))
                .addNodes(template("leaf", "/leaf.gui")));

        for (boolean reverse : List.of(false, true)) {
            for (boolean local : List.of(false, true)) {
                SceneDesc.Builder source = scene()
                        .addNodes(template("first", reverse ? "/second.gui" : "/first.gui"))
                        .addNodes(template("second", reverse ? "/first.gui" : "/second.gui"))
                        .addNodes(template("again", "/leaf.gui"))
                        .addLayouts(LayoutDesc.newBuilder().setName("Landscape"));
                if (local) {
                    source.addResources(resource("scene", "/root-resource.gui"));
                }
                String winner = (local ? "root" : reverse ? "second" : "first") + "-resource.gui";
                SceneDesc result = buildScene(source);
                Assert.assertEquals(1, result.getResourcesCount());
                Assert.assertEquals("/" + winner + "c", result.getResources(0).getPath());
                for (List<NodeDesc> nodes : List.of(result.getNodesList(), result.getLayouts(0).getNodesList())) {
                    Assert.assertEquals(3, nodes.size());
                    for (NodeDesc node : nodes) {
                        Assert.assertEquals(winner, property(node, "source_path").getString());
                    }
                }
            }
        }
    }

    // A template value may be invalid until its parent overrides it; only the resolved node is validated.
    @Test
    public void testParentOverridesInvalidTemplateValues() throws Exception {
        addScene("/template.gui", scene().addNodes(node("actor", "LifecycleTest")
                .addCustomProperties(stringProperty("animation", "invalid-create"))));
        SceneDesc result = buildScene(scene().addNodes(template("template", "/template.gui"))
                .addNodes(node("template/actor", "LifecycleTest").setTemplateNodeChild(true)
                        .addCustomProperties(stringProperty("animation", "idle"))));
        Assert.assertEquals("idle", result.getNodes(0).getText());
    }

    // Flattened template sources remain direct inputs so changing a template revalidates the parent GUI.
    @Test
    public void testTemplateChangesInvalidateParent() throws Exception {
        addScene("/template.gui", scene().addNodes(node("actor", "LifecycleTest")));
        addScene("/test.gui", scene().addNodes(template("template", "/template.gui")));
        Assert.assertEquals(TaskResult.Result.SUCCESS, rebuild().getResult());
        Assert.assertEquals(TaskResult.Result.SKIPPED, rebuild().getResult());

        addScene("/template.gui", scene().addNodes(node("actor", "LifecycleTest")
                .addCustomProperties(stringProperty("animation", "invalid-build"))));
        TaskResult result = rebuild();
        Assert.assertEquals(TaskResult.Result.FAILED, result.getResult());
        Assert.assertEquals("Node 'template/actor': Invalid animation during build", result.getMessage());
    }

    // Detect recursive templates even though they no longer create independent GUI compilation tasks.
    @Test
    public void testCircularTemplatesFailBuild() throws Exception {
        addScene("/template.gui", scene().addNodes(template("loop", "/test.gui")));
        try {
            buildScene(scene().addNodes(template("template", "/template.gui")));
            Assert.fail("Circular templates must fail the build");
        } catch (CompileExceptionError e) {
            Assert.assertTrue(e.getMessage(), e.getMessage().contains("Circular GUI template dependency"));
        }
    }

    private TaskResult rebuild() throws Exception {
        getProject().setInputs(List.of("/test.gui"));
        List<TaskResult> results = getProject().build(Progress.discarding(), "build");
        return results.stream().filter(result -> result.getTask().firstInput().getPath().equals("test.gui")).findFirst().orElseThrow();
    }

    // Completed GUI tasks must stop retaining source scenes, resolved scenes and plugin instances, even when skipped.
    @Test
    public void testCompletedTasksReleaseGuiState() throws Exception {
        addScene("/template.gui", scene().addNodes(node("actor", "MigratingLifecycleTest")));
        addScene("/test.gui", scene().addNodes(template("template", "/template.gui")));
        for (TaskResult.Result expected : List.of(TaskResult.Result.SUCCESS, TaskResult.Result.SKIPPED)) {
            TaskResult result = rebuild();
            Assert.assertEquals(result.getMessage(), expected, result.getResult());
            GuiBuilder builder = (GuiBuilder) result.getTask().getBuilder();
            for (Field field : GuiBuilder.class.getDeclaredFields()) {
                if (Modifier.isStatic(field.getModifiers())) {
                    continue;
                }
                field.setAccessible(true);
                Object value = field.get(builder);
                if (value instanceof Map<?, ?> map) {
                    Assert.assertTrue(field.getName(), map.isEmpty());
                } else if (value instanceof Set<?> set) {
                    Assert.assertTrue(field.getName(), set.isEmpty());
                } else if (value instanceof Boolean flag) {
                    Assert.assertFalse(field.getName(), flag);
                } else {
                    Assert.assertNull(field.getName(), value);
                }
            }
        }
    }

    // Raw inputs and plugin version keys invalidate cached output; compiled inputs finish before build runs.
    @Test
    public void testDependenciesAndCacheInvalidation() throws Exception {
        addFile("/animation.data", "idle");
        addScene("/generated.gui", scene().addNodes(NodeDesc.newBuilder().setId("box")
                .setType(NodeDesc.Type.TYPE_BOX).setText("compiled dependency")));
        addScene("/test.gui", scene().addNodes(node("actor", "LifecycleTest")
                .addCustomProperties(stringProperty("dependency", "/animation.data"))
                .addCustomProperties(stringProperty("compiled_dependency", "/generated.gui"))));

        TaskResult first = rebuild();
        Assert.assertEquals(first.getMessage(), TaskResult.Result.SUCCESS, first.getResult());
        NodeDesc firstNode = SceneDesc.parseFrom(first.getTask().output(0).getContent()).getNodes(0);
        Assert.assertEquals("idle", property(firstNode, "dependency_text").getString());
        Assert.assertEquals("compiled dependency", property(firstNode, "compiled_text").getString());
        Assert.assertEquals(TaskResult.Result.SKIPPED, rebuild().getResult());

        addFile("/animation.data", "walk");
        TaskResult changed = rebuild();
        Assert.assertEquals(changed.getMessage(), TaskResult.Result.SUCCESS, changed.getResult());
        NodeDesc changedNode = SceneDesc.parseFrom(changed.getTask().output(0).getContent()).getNodes(0);
        Assert.assertEquals("walk", property(changedNode, "dependency_text").getString());

        getProject().setOption("custom-node-version", "v2");
        Assert.assertEquals(TaskResult.Result.SUCCESS, rebuild().getResult());
        Assert.assertEquals(TaskResult.Result.SKIPPED, rebuild().getResult());

        addFile("/animation.data", "invalid");
        TaskResult invalid = rebuild();
        Assert.assertEquals(TaskResult.Result.FAILED, invalid.getResult());
        Assert.assertEquals("Node 'actor': Invalid dependency", invalid.getMessage());
    }

    // Adding callbacks changes the task signature so output built without lifecycle hooks cannot be reused.
    @Test
    public void testAddingCallbacksChangesTaskSignature() throws Exception {
        addScene("/test.gui", scene().addNodes(node("legacy", "LegacyTest")));
        getProject().getGuiCustomTypeRegistry().register(LegacyNode.class);
        GuiBuilder legacyBuilder = new GuiBuilder();
        legacyBuilder.setProject(getProject());
        Task legacy = legacyBuilder.create(getProject().getResource("/test.gui"));

        getProject().getGuiCustomTypeRegistry().register(UpgradedLegacyNode.class);
        GuiBuilder upgradedBuilder = new GuiBuilder();
        upgradedBuilder.setProject(getProject());
        Task upgraded = upgradedBuilder.create(getProject().getResource("/test.gui"));
        Assert.assertFalse(Arrays.equals(legacy.calculateSignature(), upgraded.calculateSignature()));
    }

    private void assertLayoutError(String animation, String message) throws Exception {
        SceneDesc.Builder source = scene().addNodes(node("actor", "LifecycleTest"))
                .addLayouts(LayoutDesc.newBuilder().setName("Landscape")
                        .addNodes(node("actor", "LifecycleTest").addCustomProperties(stringProperty("animation", animation))));
        try {
            buildScene(source);
            Assert.fail("The plugin error must fail the GUI build");
        } catch (CompileExceptionError e) {
            Assert.assertEquals("test.gui", e.getResource().getPath());
            Assert.assertEquals("Node 'actor', layout 'Landscape': " + message, e.getMessage());
        }
    }

    // Errors during dependency declaration must retain the GUI, node and layout location.
    @Test
    public void testCreateValidationErrors() throws Exception {
        assertLayoutError("invalid-create", "Invalid animation during create");
    }

    // Validation in build must fail Bob with the same source location as create.
    @Test
    public void testBuildValidationErrors() throws Exception {
        assertLayoutError("invalid-build", "Invalid animation during build");
    }

    // Either callback can be omitted, and existing static-only plugins need no public constructor.
    @Test
    public void testOptionalCallbacksAndLegacyPlugin() throws Exception {
        addFile("/create-only.data", "data");
        addScene("/test.gui", scene().addNodes(node("build-only", "BuildOnlyTest"))
                .addNodes(node("legacy", "LegacyTest"))
                .addNodes(node("create-only", "CreateOnlyTest").setText("unchanged")));
        TaskResult taskResult = rebuild();
        Assert.assertEquals(taskResult.getMessage(), TaskResult.Result.SUCCESS, taskResult.getResult());
        Assert.assertTrue(taskResult.getTask().getInputs().contains(getProject().getResource("/create-only.data")));
        SceneDesc result = SceneDesc.parseFrom(taskResult.getTask().output(0).getContent());
        Assert.assertEquals("built", result.getNodes(0).getText());
        Assert.assertEquals("legacy", property(result.getNodes(1), "value").getString());
        Assert.assertEquals("unchanged", result.getNodes(2).getText());
    }
}
