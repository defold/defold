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
import java.util.*;

import org.junit.Test;
import org.junit.Assert;

import com.dynamo.gamesys.proto.Gui;
import com.dynamo.gamesys.proto.Gui.Property.PropertyType;
import com.dynamo.gamesys.proto.Gui.SceneDesc.LayoutDesc;
import com.dynamo.gamesys.proto.Gui.NodeDesc;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.proto.DdfMath.Vector4;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.FileSystemMountPoint;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.bob.test.util.MockFileSystem;
import com.dynamo.bob.util.MurmurHash;

public class GuiBuilderTest extends AbstractProtoBuilderTest {

    private static final float EPSILON = 0.00001f;
    private ClassLoaderScanner scanner = null;

    @ProtoParams(srcClass = Gui.SceneDesc.class, messageClass = Gui.SceneDesc.class)
    @BuilderParams(name = "GuiTestResource", inExts = ".guiresource", outExt = ".guic")
    public static class TestGuiResourceBuilder extends ProtoBuilder<Gui.SceneDesc.Builder> {
        @Override
        protected Gui.SceneDesc.Builder transform(Task task, IResource input, Gui.SceneDesc.Builder scene) {
            for (Gui.SceneDesc.ResourceDesc.Builder resource : scene.getResourcesBuilderList()) {
                String suffix = ResourceUtil.getSuffix(resource.getPath());
                resource.setPath(ResourceUtil.minifyPathAndReplaceExt(resource.getPath(), suffix, ResourceUtil.getOutputExt(suffix)));
            }
            return scene;
        }
    }

    @GuiCustomNode(type = "Spine")
    private static class TestSpineGuiNode implements IGuiCustomNode {
        public static void registerProperties(IGuiCustomType type) {
            type.addProperty("spine_scene", "", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_RESOURCE_REQUIRED);
            type.addProperty("spine_default_animation", "", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_DEFAULT);
            type.addProperty("spine_skin", "", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_DEFAULT);
            type.addProperty("spine_create_bones", false, PropertyType.TYPE_BOOLEAN, IGuiCustomType.EDIT_TYPE_DEFAULT);
        }

        public static void migrateProperties(Map<String, Object> properties) {
            properties.remove("spine_node_child");
        }
    }

    @GuiCustomNode(type = "OptionalResource")
    private static class TestOptionalResourceGuiNode implements IGuiCustomNode {
        public static void registerProperties(IGuiCustomType type) {
            type.addProperty("resource", "", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_RESOURCE);
        }
    }

    @GuiCustomNode(type = "Validated")
    private static class TestValidatedGuiNode implements IGuiCustomNode {
        public static void registerProperties(IGuiCustomType type) {
            type.addProperty("selection", "valid", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_DEFAULT);
        }

        public static void validateNodes(Project project, IResource resource, List<NodeDesc> nodes, Map<String, IResource> resources) throws Exception {
            project.setOption("gui-validation-calls", Integer.toString(Integer.parseInt(project.option("gui-validation-calls", "0")) + 1));
            project.setOption("gui-validation-node-count", Integer.toString(nodes.size()));
            IResource asset = resources.get("asset");
            Assert.assertNotNull(asset);
            Assert.assertTrue(asset.isOutput());
            Gui.SceneDesc assetScene = Gui.SceneDesc.parseFrom(asset.getContent());
            String validSelection = "valid";
            if (assetScene.getResourcesCount() != 0) {
                do {
                    IResource data = project.getResource(assetScene.getResources(0).getPath()).output();
                    assetScene = Gui.SceneDesc.parseFrom(data.getContent());
                } while (assetScene.getResourcesCount() != 0);
                validSelection = assetScene.getNodes(0).getText();
            }
            for (NodeDesc node : nodes) {
                Assert.assertEquals(MurmurHash.hash32("Validated"), node.getCustomType());
                String selection = findCustomProperty(node, "selection").getString();
                if (selection.equals("exception")) {
                    throw new IllegalArgumentException("Invalid plugin input");
                }
                if (!selection.equals(validSelection)) {
                    throw new CompileExceptionError(resource, 17, "GUI node '" + node.getId() + "': invalid selection");
                }
            }
        }
    }

    @GuiCustomNode(type = "LayoutValidated")
    private static class TestLayoutValidatedGuiNode implements IGuiCustomNode {
        public static void registerProperties(IGuiCustomType type) {
            type.addProperty("selection", "valid", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_DEFAULT);
        }

        public static void validateNodes(Project project, IResource resource, List<NodeDesc> nodes, Map<String, IResource> resources) {
            Assert.fail("The layout overload must take precedence over the legacy list overload");
        }

        public static void validateNodes(Project project, IResource resource, Map<String, List<NodeDesc>> nodesByLayout, Map<String, IResource> resources) throws CompileExceptionError {
            project.setOption("gui-layout-validation-calls", Integer.toString(Integer.parseInt(project.option("gui-layout-validation-calls", "0")) + 1));
            project.setOption("gui-validated-layouts", String.join(",", nodesByLayout.keySet()));
            for (Map.Entry<String, List<NodeDesc>> layout : nodesByLayout.entrySet()) {
                for (NodeDesc node : layout.getValue()) {
                    Assert.assertEquals(MurmurHash.hash32("LayoutValidated"), node.getCustomType());
                    if (!findCustomProperty(node, "selection").getString().equals("valid")) {
                        throw new CompileExceptionError(resource, 17,
                                "GUI node '" + node.getId() + "' in layout '" + layout.getKey() + "': invalid selection");
                    }
                }
            }
        }
    }

    @GuiCustomNode(type = "Typed")
    private static class TestTypedGuiNode implements IGuiCustomNode {
        public static void registerProperties(IGuiCustomType type) {
            type.addProperty("number", 1.5f, PropertyType.TYPE_NUMBER, IGuiCustomType.EDIT_TYPE_DEFAULT);
            type.addProperty("boolean", true, PropertyType.TYPE_BOOLEAN, IGuiCustomType.EDIT_TYPE_DEFAULT);
            type.addProperty("hash", "hash_value", PropertyType.TYPE_HASH, IGuiCustomType.EDIT_TYPE_DEFAULT);
            type.addProperty("string", "text", PropertyType.TYPE_STRING, IGuiCustomType.EDIT_TYPE_DEFAULT);
            type.addProperty("vector3", Vector3.newBuilder().setX(1.0f).setY(2.0f).setZ(3.0f).build(), PropertyType.TYPE_VECTOR3, IGuiCustomType.EDIT_TYPE_DEFAULT);
            type.addProperty("vector4", Vector4.newBuilder().setX(4.0f).setY(5.0f).setZ(6.0f).setW(7.0f).build(), PropertyType.TYPE_VECTOR4, IGuiCustomType.EDIT_TYPE_DEFAULT);
            type.addProperty("quat", Quat.newBuilder().setX(8.0f).setY(9.0f).setZ(10.0f).setW(11.0f).build(), PropertyType.TYPE_QUAT, IGuiCustomType.EDIT_TYPE_DEFAULT);
        }
    }

    public GuiBuilderTest() throws IOException {
        this.scanner = new ClassLoaderScanner(this.getClass().getClassLoader());
        registerProtoBuilderNames();
        getProject().getGuiCustomTypeRegistry().register(TestSpineGuiNode.class);
        getProject().getGuiCustomTypeRegistry().register(TestOptionalResourceGuiNode.class);
        getProject().getGuiCustomTypeRegistry().register(TestTypedGuiNode.class);
        getProject().getGuiCustomTypeRegistry().register(TestValidatedGuiNode.class);
        getProject().getGuiCustomTypeRegistry().register(TestLayoutValidatedGuiNode.class);
    }

    private boolean nodeExists(Gui.SceneDesc scene, String nodeId)
    {
        for (int n = 0; n < scene.getNodesCount(); n++) {
            Gui.NodeDesc node = scene.getNodes(n);
            if (node.getId().equals(nodeId)) {
                return true;
            }
        }

        return false;
    }

    private void registerProtoBuilderNames() {
        Set<String> classNames = this.scanner.scan("com.dynamo.bob.pipeline");

        for (String className : classNames) {
            // Ignore TexcLibrary to avoid it being loaded and initialized
            boolean skip = className.startsWith("com.dynamo.bob.TexcLibrary");
            if (!skip) {
                try {
                    Class<?> klass = Class.forName(className, true, this.scanner.getClassLoader());
                    BuilderParams builderParams = klass.getAnnotation(BuilderParams.class);
                    if (builderParams != null) {
                        ProtoParams protoParams = klass.getAnnotation(ProtoParams.class);
                        if (protoParams != null) {
                            ProtoBuilder.addMessageClass(builderParams.outExt(), protoParams.messageClass());

                            for (String ext : builderParams.inExts()) {
                                Class<?> inputKlass = protoParams.srcClass();
                                if (inputKlass != null) {
                                    ProtoBuilder.addMessageClass(ext, protoParams.srcClass());
                                }
                            }
                        }
                    }

                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }
    }

    private static StringBuilder createGui() {
        StringBuilder src = new StringBuilder();
        src.append("material: \"\"");
        return src;
    }

    private Gui.SceneDesc buildGui(StringBuilder src, String path) throws Exception {
        return getMessage(build(path, src.toString()), Gui.SceneDesc.class);
    }

    private static void startBoxNode(StringBuilder src, String id, String parent) {
        src.append("nodes {\n");
        src.append("  type: TYPE_BOX\n");
        src.append("  id: \"").append(id).append("\"\n");
        src.append("  parent: \"").append(parent).append("\"\n");
    }

    private static void addBoxNode(StringBuilder src, String id, String parent) {
        startBoxNode(src, id, parent);
        finishNode(src);
    }

    private static void addTextNode(StringBuilder src, String id, String parent, String text) {
        src.append("nodes {\n");
        src.append("  type: TYPE_TEXT\n");
        src.append("  id: \"").append(id).append("\"\n");
        src.append("  parent: \"").append(parent).append("\"\n");
        src.append("  text: \"").append(text).append("\"\n");
        src.append("}\n");
    }

    private void addSpineResource(StringBuilder src, String name) {
        src.append("\nresources {\n");
        src.append("  name: \"").append(name).append("\"\n");
        src.append("  path: \"/assets/").append(name).append(".gui\"\n");
        src.append("}\n");
        addFile("/assets/"+name+".gui", createGui().toString());
    }

    private void addGuiResource(StringBuilder src, String name) {
        addGuiResource(src, name, "gui");
    }

    private void addGuiResource(StringBuilder src, String name, String extension) {
        src.append("\nresources {\n");
        src.append("  name: \"").append(name).append("\"\n");
        src.append("  path: \"/assets/").append(name).append(".").append(extension).append("\"\n");
        src.append("}\n");
        addFile("/assets/" + name + "." + extension, createGui().toString());
    }

    private static void startSpineCustomNode(StringBuilder src, String id) {
        startCustomNode(src, id, "Spine");
    }

    private static void startCustomNode(StringBuilder src, String id, String typeName) {
        src.append("\nnodes {\n");
        src.append("  type: TYPE_CUSTOM\n");
        src.append("  custom_type: ").append(Integer.toUnsignedLong(MurmurHash.hash32(typeName))).append("\n");
        src.append("  custom_type_name: \"").append(typeName).append("\"\n");
        src.append("  id: \"").append(id).append("\"\n");
    }

    private static void startCustomNodeWithTypeName(StringBuilder src, String id, String typeName) {
        src.append("\nnodes {\n");
        src.append("  type: TYPE_CUSTOM\n");
        src.append("  custom_type_name: \"").append(typeName).append("\"\n");
        src.append("  id: \"").append(id).append("\"\n");
    }

    private static void startLegacySpineNode(StringBuilder src, String id) {
        src.append("\nnodes {\n");
        src.append("  type: TYPE_SPINE\n");
        src.append("  id: \"").append(id).append("\"\n");
    }

    private static void addLegacySpineProperties(StringBuilder src) {
        src.append("  spine_scene: \"spineboy\"\n");
        src.append("  spine_default_animation: \"walk\"\n");
        src.append("  spine_skin: \"default\"\n");
        src.append("  spine_node_child: true\n");
        src.append("  spine_create_bones: true\n");
    }

    private static void addTemplateNode(StringBuilder src, String id, String parent, String template, String[] extraProperties) {
        src.append("nodes {\n");
        src.append("  type: TYPE_TEMPLATE\n");
        src.append("  id: \"").append(id).append("\"\n");
        src.append("  parent: \"").append(parent).append("\"\n");
        for (String prop: extraProperties) { 
            src.append(prop);
        }
        src.append("  template: \"").append(template).append("\"\n");
        src.append("}\n");
    }

    private static void addTemplateNode(StringBuilder src, String id, String parent, String template) {
        String[] properties = {};
        addTemplateNode(src, id, parent, template, properties);
    }

    private static void startOverriddenNode(StringBuilder src, NodeDesc.Type type, String id, String parent, boolean templateNodeChild, List<Integer> overriddenFields) {
        src.append("nodes {\n");
        src.append("  type: ").append(type).append("\n");
        src.append("  id: \"").append(id).append("\"\n");
        src.append("  parent: \"").append(parent).append("\"\n");
        src.append("  template_node_child: ").append(templateNodeChild).append("\n");
        for(int num : overriddenFields) {
            src.append("  overridden_fields: ").append(num).append("\n");
        }
    }

    private static void addOverriddenNode(StringBuilder src, NodeDesc.Type type, String id, String parent, boolean templateNodeChild) {
        startOverriddenNode(src, type, id, parent, templateNodeChild, List.of());
        finishNode(src);
    }

    private static void finishNode(StringBuilder src) {
        src.append("}\n");
    }

    private static void startLayout(StringBuilder src, String name) {
        src.append("layouts {\n");
        src.append("  name: \"").append(name).append("\"\n");
    }

    private static void finishLayout(StringBuilder src) {
        src.append("}\n");
    }

    private StringBuilder createGuiWithTemplateAndLayout() {
        StringBuilder src = createGui();
        addBoxNode(src, "box", "");
        addTextNode(src, "text", "box", "templateText");
        addFile("/template.gui", src.toString());

        src = createGui();
        addBoxNode(src, "box", "");
        addTemplateNode(src, "template", "box", "/template.gui");

        // the parent box from the template - no overrides, but parent differs
        addOverriddenNode(src, NodeDesc.Type.TYPE_BOX, "template/box", "template", true);

        // override text in default layout
        startOverriddenNode(src, NodeDesc.Type.TYPE_TEXT, "template/text", "template/box", true, List.of(NodeDesc.TEXT_FIELD_NUMBER));
        src.append("  text: \"defaultText\"\n");
        finishNode(src);

        startLayout(src, "Landscape");

        // override clipping_visible in Landscape layout
        startOverriddenNode(src, NodeDesc.Type.TYPE_TEXT, "template/text", "template/box", true, List.of(NodeDesc.CLIPPING_VISIBLE_FIELD_NUMBER));
        src.append("  clipping_visible: false\n");
        src.append("  text: \"defaultText\"\n");
        finishNode(src);

        finishLayout(src);
        return src;
    }

    private static NodeDesc findNode(Gui.SceneDesc gui, String layoutName, String nodeName) {
        List<NodeDesc> nodesList = List.of();
        if (layoutName.isEmpty()) {
            nodesList = gui.getNodesList();
        }
        else {
            for(LayoutDesc layout : gui.getLayoutsList()) {
                if (layout.getName().equals(layoutName)) {
                    nodesList = layout.getNodesList();
                }
            }
        }
        for(NodeDesc node : nodesList) {
            if (node.getId().equals(nodeName)) {
                return node;
            }
        }
        return null;
    }

    private static Gui.Property findCustomProperty(NodeDesc node, String propertyName) {
        long propertyNameHash = MurmurHash.hash64(propertyName);
        for (Gui.Property property : node.getCustomPropertiesList()) {
            if (property.hasIdHash() && property.getIdHash() == propertyNameHash) {
                return property;
            }
        }
        return null;
    }

    private static Gui.SceneDesc.ResourceDesc findResource(Gui.SceneDesc gui, String resourceName) {
        for (Gui.SceneDesc.ResourceDesc resource : gui.getResourcesList()) {
            if (resource.getName().equals(resourceName)) {
                return resource;
            }
        }
        return null;
    }

    @Test
    public void test() throws Exception {
        // Kept empty as a future working template
    }

    @Test
    public void testGenericResourcesUseRegisteredOutputExtension() throws Exception {
        StringBuilder src = createGui();
        addGuiResource(src, "panel");

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        Gui.SceneDesc.ResourceDesc resource = findResource(gui, "panel");

        Assert.assertNotNull(resource);
        Assert.assertEquals("/assets/panel.guic", resource.getPath());
    }

    @Test
    public void testSpineCustomNodePropertiesAreMigrated() throws Exception {
        StringBuilder src = createGui();
        addSpineResource(src, "spineboy");
        startSpineCustomNode(src, "spine");
        addLegacySpineProperties(src);
        finishNode(src);

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        NodeDesc node = findNode(gui, "", "spine");

        Assert.assertEquals(NodeDesc.Type.TYPE_CUSTOM, node.getType());
        Assert.assertEquals(405028931, node.getCustomType());
        Assert.assertFalse(node.hasCustomTypeName());
        Assert.assertFalse(node.hasSpineScene());
        Assert.assertFalse(node.hasSpineDefaultAnimation());
        Assert.assertFalse(node.hasSpineSkin());
        Assert.assertFalse(node.hasSpineNodeChild());
        Assert.assertFalse(node.hasSpineCreateBones());
        Assert.assertEquals(4, node.getCustomPropertiesCount());

        Gui.Property spineScene = findCustomProperty(node, "spine_scene");
        Gui.Property defaultAnimation = findCustomProperty(node, "spine_default_animation");
        Gui.Property spineSkin = findCustomProperty(node, "spine_skin");
        Gui.Property spineCreateBones = findCustomProperty(node, "spine_create_bones");
        Assert.assertNotNull(spineScene);
        Assert.assertNotNull(defaultAnimation);
        Assert.assertNotNull(spineSkin);
        Assert.assertNotNull(spineCreateBones);
        Assert.assertFalse(spineScene.hasId());
        Assert.assertEquals(Gui.Property.PropertyType.TYPE_STRING, spineScene.getType());
        Assert.assertEquals(Gui.Property.PropertyType.TYPE_BOOLEAN, spineCreateBones.getType());
        Assert.assertEquals("spineboy", spineScene.getString());
        Assert.assertEquals("walk", defaultAnimation.getString());
        Assert.assertEquals("default", spineSkin.getString());
        Assert.assertTrue(spineCreateBones.getBoolean());
    }

    @Test
    public void testLegacySpineNodeBecomesCustomSpineNode() throws Exception {
        StringBuilder src = createGui();
        addSpineResource(src, "spineboy");
        startLegacySpineNode(src, "spine");
        addLegacySpineProperties(src);
        finishNode(src);

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        NodeDesc node = findNode(gui, "", "spine");

        Assert.assertEquals(NodeDesc.Type.TYPE_CUSTOM, node.getType());
        Assert.assertEquals(405028931, node.getCustomType());
        Assert.assertEquals("spineboy", findCustomProperty(node, "spine_scene").getString());
    }

    @Test
    public void testExistingCustomPropertiesWinOverMigratedValues() throws Exception {
        StringBuilder src = createGui();
        addSpineResource(src, "spineboy");
        startSpineCustomNode(src, "spine");
        addLegacySpineProperties(src);
        src.append("  custom_properties {\n");
        src.append("    id: \"spine_default_animation\"\n");
        src.append("    type: TYPE_STRING\n");
        src.append("    string: \"existing\"\n");
        src.append("  }\n");
        finishNode(src);

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        NodeDesc node = findNode(gui, "", "spine");

        Assert.assertEquals(4, node.getCustomPropertiesCount());
        Gui.Property property = findCustomProperty(node, "spine_default_animation");
        Assert.assertNotNull(property);
        Assert.assertFalse(property.hasId());
        Assert.assertEquals("existing", property.getString());
    }

    @Test(expected = CompileExceptionError.class)
    public void testCustomResourcePropertyIsValidated() throws Exception {
        StringBuilder src = createGui();
        startSpineCustomNode(src, "spine");
        src.append("  spine_scene: \"missing\"\n");
        finishNode(src);

        buildGui(src, "/test.gui");
    }

    private void assertMissingSpineScene(StringBuilder src, String nodeId) throws Exception {
        assertGuiBuildError(src, "GUI node '" + nodeId + "' must specify a resource for custom property 'spine_scene'");
    }

    // Supplies compiled resources and groups only matching nodes into one validation call per GUI.
    @Test
    public void testCustomNodeValidatorReceivesCompiledResources() throws Exception {
        StringBuilder src = createGui();
        addGuiResource(src, "asset");
        startCustomNode(src, "first", "Validated");
        finishNode(src);
        startCustomNode(src, "second", "Validated");
        finishNode(src);
        addBoxNode(src, "box", "");

        buildGui(src, "/test.gui");

        Assert.assertEquals("1", getProject().option("gui-validation-calls", "0"));
        Assert.assertEquals("2", getProject().option("gui-validation-node-count", "0"));
    }

    // Tracks transitive inputs without scanning unrelated tasks in the project for every GUI.
    @Test
    public void testCustomNodeValidationDoesNotScanProjectTasks() throws Exception {
        MockFileSystem fs = new MockFileSystem();
        try (Project project = new Project(fs) {
            @Override
            public List<Task> getTasks() {
                throw new AssertionError("GUI dependency discovery must not scan the whole project");
            }
        }) {
            project.scan(scanner, "com.dynamo.bob.pipeline");
            project.getGuiCustomTypeRegistry().register(TestValidatedGuiNode.class);
            fs.addFile("/names.gui", createGui().toString().getBytes());
            fs.addFile("/asset.guiresource", "material: \"\" resources { name: \"data\" path: \"/names.gui\" }".getBytes());
            for (int i = 0; i < 20; ++i) {
                String path = "/unrelated" + i + ".gui";
                fs.addFile(path, createGui().toString().getBytes());
                project.createTask(project.getResource(path));
            }
            StringBuilder src = createGui();
            src.append(" resources { name: \"asset\" path: \"/asset.guiresource\" }");
            startCustomNode(src, "validated", "Validated");
            finishNode(src);
            fs.addFile("/test.gui", src.toString().getBytes());
            Task task = project.createTask(project.getResource("/test.gui"));
            Assert.assertTrue(task.getInputs().contains(project.getResource("/names.gui")));
            Assert.assertTrue(task.getInputs().contains(project.getResource("/names.gui").changeExt(".guic")));
        }
    }

    // Avoids pulling transitive validation inputs into GUIs whose custom types have no validator.
    @Test
    public void testCustomNodeWithoutValidatorKeepsDirectResourceDependencies() throws Exception {
        StringBuilder src = createGui();
        addGuiResource(src, "asset", "guiresource");
        startCustomNode(src, "optional", "OptionalResource");
        finishNode(src);
        addFile("/test.gui", src.toString());
        Task task = getProject().createTask(getProject().getResource("/test.gui"));
        Assert.assertEquals(List.of(getProject().getResource("/test.gui"),
                getProject().getResource("/assets/asset.guic").output()), task.getInputs());
    }

    // Revalidates an unchanged GUI when resource data changes but its compiled descriptor does not.
    @Test
    public void testCustomNodeValidatorChecksChangedResourceData() throws Exception {
        assertCustomNodeValidatorChecksChangedResourceData(false, "gui");
    }

    // Keeps resource-data dependencies when a validated custom node is imported through a template.
    @Test
    public void testCustomNodeValidatorChecksChangedTemplateResourceData() throws Exception {
        assertCustomNodeValidatorChecksChangedResourceData(true, "gui");
    }

    // Revalidates resource data behind multiple unchanged descriptors built by ordinary ProtoBuilders.
    @Test
    public void testCustomNodeValidatorChecksChangedTransitiveResourceData() throws Exception {
        assertCustomNodeValidatorChecksChangedResourceData(false, "guiresource");
    }

    // Carries transitive resource data dependencies through a template into its containing GUI.
    @Test
    public void testCustomNodeValidatorChecksChangedTransitiveTemplateResourceData() throws Exception {
        assertCustomNodeValidatorChecksChangedResourceData(true, "guiresource");
    }

    private void assertCustomNodeValidatorChecksChangedResourceData(boolean throughTemplate, String descriptorExtension) throws Exception {
        StringBuilder src = createGui();
        addGuiResource(src, "asset", descriptorExtension);
        startCustomNode(src, "validated", "Validated");
        finishNode(src);

        StringBuilder asset = createGui();
        addGuiResource(asset, "data", descriptorExtension);
        addFile("/assets/asset." + descriptorExtension, asset.toString());
        StringBuilder data = createGui();
        addGuiResource(data, "names");
        addFile("/assets/data." + descriptorExtension, data.toString());
        StringBuilder names = createGui();
        addTextNode(names, "name", "", "valid");
        addFile("/assets/names.gui", names.toString());
        if (throughTemplate) {
            addFile("/template.gui", src.toString());
            src = createGui();
            addTemplateNode(src, "template", "", "/template.gui");
        }
        buildGui(src, "/test.gui");
        byte[] compiledAsset = getProject().getResource("/assets/asset.guic").output().getContent();
        byte[] compiledData = getProject().getResource("/assets/data.guic").output().getContent();

        names = createGui();
        addTextNode(names, "name", "", "renamed");
        addFile("/assets/names.gui", names.toString());
        String nodeId = throughTemplate ? "template/validated" : "validated";
        assertGuiBuildError(src, "GUI node '" + nodeId + "': invalid selection");
        Assert.assertArrayEquals(compiledAsset, getProject().getResource("/assets/asset.guic").output().getContent());
        Assert.assertArrayEquals(compiledData, getProject().getResource("/assets/data.guic").output().getContent());
    }

    // Preserves the plugin's GUI file and line information when a final layout value is invalid.
    @Test
    public void testCustomNodeValidatorChecksLayoutOverrides() throws Exception {
        StringBuilder src = createGui();
        addGuiResource(src, "asset");
        startCustomNode(src, "validated", "Validated");
        finishNode(src);
        startLayout(src, "Landscape");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, "validated", "", false, List.of());
        src.append("  custom_properties { id: \"selection\" type: TYPE_STRING string: \"invalid\" }\n");
        finishNode(src);
        finishLayout(src);

        try {
            buildGui(src, "/test.gui");
            Assert.fail("Expected validation of the layout override to fail");
        } catch (CompileExceptionError e) {
            Assert.assertEquals("GUI node 'validated': invalid selection", e.getMessage());
            Assert.assertEquals("test.gui", e.getResource().getPath());
            Assert.assertEquals(17, e.getLineNumber());
        }
    }

    // Preserves layout names and invokes only the preferred callback once when both overloads exist.
    @Test
    public void testCustomNodeValidatorReceivesLayoutNames() throws Exception {
        StringBuilder src = createGui();
        startCustomNode(src, "validated", "LayoutValidated");
        finishNode(src);
        addBoxNode(src, "box", "");
        startLayout(src, "Landscape");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, "validated", "", false, List.of());
        src.append("  custom_properties { id: \"selection\" type: TYPE_STRING string: \"valid\" }\n");
        finishNode(src);
        finishLayout(src);

        buildGui(src, "/test.gui");

        Assert.assertEquals("1", getProject().option("gui-layout-validation-calls", "0"));
        Assert.assertEquals(",Landscape", getProject().option("gui-validated-layouts", "missing"));
    }

    // Lets plugins identify the invalid layout without losing the source GUI or exception line.
    @Test
    public void testCustomNodeValidatorReportsInvalidLayoutName() throws Exception {
        StringBuilder src = createGui();
        startCustomNode(src, "validated", "LayoutValidated");
        finishNode(src);
        startLayout(src, "Landscape");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, "validated", "", false, List.of());
        src.append("  custom_properties { id: \"selection\" type: TYPE_STRING string: \"invalid\" }\n");
        finishNode(src);
        finishLayout(src);

        try {
            buildGui(src, "/test.gui");
            Assert.fail("Expected the layout-specific selection to fail validation");
        } catch (CompileExceptionError e) {
            Assert.assertEquals("GUI node 'validated' in layout 'Landscape': invalid selection", e.getMessage());
            Assert.assertEquals("test.gui", e.getResource().getPath());
            Assert.assertEquals(17, e.getLineNumber());
        }
    }

    // Invokes extension validation after template overrides and imports the template's resources.
    @Test
    public void testCustomNodeValidatorChecksTemplateOverrides() throws Exception {
        StringBuilder templateSrc = createGui();
        addGuiResource(templateSrc, "asset");
        startCustomNode(templateSrc, "validated", "Validated");
        finishNode(templateSrc);
        addFile("/template.gui", templateSrc.toString());

        StringBuilder src = createGui();
        addTemplateNode(src, "template", "", "/template.gui");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, "template/validated", "template", true, List.of());
        src.append("  custom_properties { id: \"selection\" type: TYPE_STRING string: \"invalid\" }\n");
        finishNode(src);

        assertGuiBuildError(src, "GUI node 'template/validated': invalid selection");
    }

    // Accepts a parent override that corrects an invalid template selection before final validation.
    @Test
    public void testCustomNodeValidatorAcceptsCorrectedTemplateSelection() throws Exception {
        StringBuilder templateSrc = createGui();
        addGuiResource(templateSrc, "asset");
        startCustomNode(templateSrc, "validated", "Validated");
        templateSrc.append("  custom_properties { id: \"selection\" type: TYPE_STRING string: \"invalid\" }\n");
        finishNode(templateSrc);
        addFile("/template.gui", templateSrc.toString());

        StringBuilder src = createGui();
        addTemplateNode(src, "template", "", "/template.gui");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, "template/validated", "template", true, List.of());
        src.append("  custom_properties { id: \"selection\" type: TYPE_STRING string: \"valid\" }\n");
        finishNode(src);
        addTemplateNode(src, "second", "", "/template.gui");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, "second/validated", "second", true, List.of());
        src.append("  custom_properties { id: \"selection\" type: TYPE_STRING string: \"valid\" }\n");
        finishNode(src);

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        Assert.assertEquals("valid", findCustomProperty(findNode(gui, "", "template/validated"), "selection").getString());
        Assert.assertEquals("valid", findCustomProperty(findNode(gui, "", "second/validated"), "selection").getString());
        Assert.assertEquals("1", getProject().option("gui-validation-calls", "0"));
        Assert.assertEquals("2", getProject().option("gui-validation-node-count", "0"));

        // The same source still needs validation when compiled as a standalone GUI.
        try {
            buildGui(templateSrc, "/template.gui");
            Assert.fail("Expected the standalone GUI's invalid selection to fail");
        } catch (CompileExceptionError e) {
            Assert.assertEquals("GUI node 'validated': invalid selection", e.getMessage());
            Assert.assertEquals("template.gui", e.getResource().getPath());
        }
    }

    // Validates an uncorrected template selection in the containing GUI, using its instantiated node ID.
    @Test
    public void testCustomNodeValidatorRejectsUncorrectedTemplateSelection() throws Exception {
        StringBuilder templateSrc = createGui();
        addGuiResource(templateSrc, "asset");
        startCustomNode(templateSrc, "validated", "Validated");
        templateSrc.append("  custom_properties { id: \"selection\" type: TYPE_STRING string: \"invalid\" }\n");
        finishNode(templateSrc);
        addFile("/template.gui", templateSrc.toString());

        StringBuilder src = createGui();
        addTemplateNode(src, "template", "", "/template.gui");
        assertGuiBuildError(src, "GUI node 'template/validated': invalid selection");
    }

    // Invalidates the parent GUI when template source changes without a separate compiled template task.
    @Test
    public void testCustomNodeValidatorChecksChangedTemplateSource() throws Exception {
        StringBuilder templateSrc = createGui();
        addGuiResource(templateSrc, "asset");
        startCustomNode(templateSrc, "validated", "Validated");
        finishNode(templateSrc);
        addFile("/template.gui", templateSrc.toString());

        StringBuilder src = createGui();
        addTemplateNode(src, "template", "", "/template.gui");
        buildGui(src, "/test.gui");
        Assert.assertEquals("1", getProject().option("gui-validation-calls", "0"));

        templateSrc = createGui();
        addGuiResource(templateSrc, "asset");
        startCustomNode(templateSrc, "validated", "Validated");
        templateSrc.append("  custom_properties { id: \"selection\" type: TYPE_STRING string: \"invalid\" }\n");
        finishNode(templateSrc);
        addFile("/template.gui", templateSrc.toString());
        assertGuiBuildError(src, "GUI node 'template/validated': invalid selection");
        Assert.assertEquals("2", getProject().option("gui-validation-calls", "0"));
    }

    // Reports circular template source dependencies instead of recursing indefinitely during task creation.
    @Test
    public void testCircularTemplateDependencyFailsBuild() throws Exception {
        StringBuilder templateSrc = createGui();
        addTemplateNode(templateSrc, "child", "", "/test.gui");
        addFile("/template.gui", templateSrc.toString());

        StringBuilder src = createGui();
        addTemplateNode(src, "template", "", "/template.gui");
        try {
            buildGui(src, "/test.gui");
            Assert.fail("Expected circular template dependencies to fail");
        } catch (CompileExceptionError e) {
            Assert.assertTrue(e.getMessage().contains("Circular dependency detected"));
            Assert.assertEquals("test.gui", e.getResource().getPath());
        }
    }

    // Detects cycles across fresh mounted-resource wrappers instead of overflowing the stack.
    @Test
    public void testCircularMountedTemplateDependencyFailsBuild() throws Exception {
        StringBuilder templateSrc = createGui();
        addTemplateNode(templateSrc, "child", "", "/library/./template.gui");
        MockFileSystem mountedFileSystem = new MockFileSystem();
        mountedFileSystem.addFile("/library/template.gui", templateSrc.toString().getBytes());
        getFileSystem().addMountPoint(new FileSystemMountPoint(getFileSystem(), mountedFileSystem));

        StringBuilder src = createGui();
        addTemplateNode(src, "template", "", "/library/template.gui");
        addFile("/test.gui", src.toString());
        try {
            getProject().createTask(getProject().getResource("/test.gui"));
            Assert.fail("Expected circular mounted template dependencies to fail");
        } catch (CompileExceptionError e) {
            Assert.assertEquals("Circular dependency detected in GUI templates: test.gui -> library/template.gui -> library/template.gui", e.getMessage());
            Assert.assertEquals("test.gui", e.getResource().getPath());
        }
    }

    // Tracks a shared mounted template once while preserving each instantiated node hierarchy.
    @Test
    public void testSharedMountedTemplateHasOneSourceDependency() throws Exception {
        StringBuilder templateSrc = createGui();
        addBoxNode(templateSrc, "box", "");
        MockFileSystem mountedFileSystem = new MockFileSystem();
        mountedFileSystem.addFile("/library/template.gui", templateSrc.toString().getBytes());
        getFileSystem().addMountPoint(new FileSystemMountPoint(getFileSystem(), mountedFileSystem));

        StringBuilder src = createGui();
        addTemplateNode(src, "first", "", "/library/template.gui");
        addTemplateNode(src, "second", "", "/library/./template.gui");
        addFile("/test.gui", src.toString());
        Task task = getProject().createTask(getProject().getResource("/test.gui"));
        Assert.assertEquals(1L, task.getInputs().stream()
                .filter(resource -> resource.getPath().equals("library/template.gui"))
                .count());

        task.getBuilder().build(task);
        Gui.SceneDesc gui = Gui.SceneDesc.parseFrom(task.output(0).getContent());
        Assert.assertTrue(nodeExists(gui, "first/box"));
        Assert.assertTrue(nodeExists(gui, "second/box"));
    }

    // Attributes unexpected plugin exceptions to the source GUI instead of exposing reflection errors.
    @Test
    public void testCustomNodeValidatorExceptionReportsGuiResource() throws Exception {
        StringBuilder src = createGui();
        addGuiResource(src, "asset");
        startCustomNode(src, "validated", "Validated");
        src.append("  custom_properties { id: \"selection\" type: TYPE_STRING string: \"exception\" }\n");
        finishNode(src);

        assertGuiBuildError(src, "Unable to validate GUI custom nodes of type 'Validated': Invalid plugin input");
    }

    private void assertGuiBuildError(StringBuilder src, String message) throws Exception {
        try {
            buildGui(src, "/test.gui");
            Assert.fail("Expected the GUI build to fail: " + message);
        } catch (CompileExceptionError e) {
            Assert.assertEquals(message, e.getMessage());
            Assert.assertEquals("test.gui", e.getResource().getPath());
        }
    }

    // Rejects a string in place of the Boolean Spine property that runtime initialization expects.
    @Test
    public void testSpineCreateBonesMustBeBoolean() throws Exception {
        StringBuilder src = createGui();
        addSpineResource(src, "spineboy");
        startSpineCustomNode(src, "spine");
        addLegacySpineProperties(src);
        src.append("  custom_properties { id: \"spine_create_bones\" type: TYPE_STRING string: \"false\" }\n");
        finishNode(src);

        assertGuiBuildError(src, "GUI node 'spine': custom property 'spine_create_bones' must have type TYPE_BOOLEAN, got TYPE_STRING");
    }

    // Checks explicit properties addressed by hash against the extension's registered type.
    @Test
    public void testCustomPropertyTypeIsCheckedByHash() throws Exception {
        StringBuilder src = createGui();
        startCustomNode(src, "typed", "Typed");
        src.append("  custom_properties { id_hash: ").append(Long.toUnsignedString(MurmurHash.hash64("string")))
                .append(" type: TYPE_NUMBER number: 1 }\n");
        finishNode(src);

        assertGuiBuildError(src, "GUI node 'typed': custom property 'string' must have type TYPE_STRING, got TYPE_NUMBER");
    }

    // Rejects a payload whose field disagrees with its declared type, avoiding runtime union misreads.
    @Test
    public void testCustomPropertyValueMustMatchType() throws Exception {
        StringBuilder src = createGui();
        startCustomNode(src, "typed", "Typed");
        src.append("  custom_properties { id: \"boolean\" type: TYPE_BOOLEAN string: \"false\" }\n");
        finishNode(src);

        assertGuiBuildError(src, "GUI node 'typed': custom property 'boolean' must have a value matching TYPE_BOOLEAN");
    }

    // Rejects an explicitly supplied property without a value instead of emitting uninitialized data.
    @Test
    public void testCustomPropertyMustHaveValue() throws Exception {
        StringBuilder src = createGui();
        startCustomNode(src, "typed", "Typed");
        src.append("  custom_properties { id: \"vector3\" type: TYPE_VECTOR3 }\n");
        finishNode(src);

        assertGuiBuildError(src, "GUI node 'typed': custom property 'vector3' must have a value matching TYPE_VECTOR3");
    }

    // Accepts explicitly supplied zero, false, and empty string values as correctly typed values.
    @Test
    public void testCustomPropertyDefaultValuesAreValid() throws Exception {
        StringBuilder src = createGui();
        startCustomNode(src, "typed", "Typed");
        src.append("  custom_properties { id: \"number\" type: TYPE_NUMBER number: 0 }\n");
        src.append("  custom_properties { id: \"boolean\" type: TYPE_BOOLEAN boolean: false }\n");
        src.append("  custom_properties { id: \"string\" type: TYPE_STRING string: \"\" }\n");
        finishNode(src);

        NodeDesc node = findNode(buildGui(src, "/test.gui"), "", "typed");
        Assert.assertEquals(0.0f, findCustomProperty(node, "number").getNumber(), EPSILON);
        Assert.assertFalse(findCustomProperty(node, "boolean").getBoolean());
        Assert.assertEquals("", findCustomProperty(node, "string").getString());
    }

    // Checks types after a layout overrides a valid custom property.
    @Test
    public void testLayoutCustomPropertyTypeIsValidated() throws Exception {
        StringBuilder src = createGui();
        startCustomNode(src, "typed", "Typed");
        finishNode(src);
        startLayout(src, "Landscape");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, "typed", "", false, List.of());
        src.append("  custom_properties { id: \"boolean\" type: TYPE_NUMBER number: 1 }\n");
        finishNode(src);
        finishLayout(src);

        assertGuiBuildError(src, "GUI node 'typed' in layout 'Landscape': custom property 'boolean' must have type TYPE_BOOLEAN, got TYPE_NUMBER");
    }

    // Checks types after a template child overrides a valid custom property.
    @Test
    public void testTemplateCustomPropertyTypeIsValidated() throws Exception {
        StringBuilder templateSrc = createGui();
        startCustomNode(templateSrc, "typed", "Typed");
        finishNode(templateSrc);
        addFile("/template.gui", templateSrc.toString());

        StringBuilder src = createGui();
        addTemplateNode(src, "template", "", "/template.gui");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, "template/typed", "template", true, List.of());
        src.append("  custom_properties { id: \"boolean\" type: TYPE_NUMBER number: 1 }\n");
        finishNode(src);

        assertGuiBuildError(src, "GUI node 'template/typed': custom property 'boolean' must have type TYPE_BOOLEAN, got TYPE_NUMBER");
    }

    // Rejects an omitted Spine scene before an uninitialized node can reach runtime cloning.
    @Test
    public void testMissingRequiredCustomResourceFailsBuild() throws Exception {
        StringBuilder src = createGui();
        startSpineCustomNode(src, "spine");
        finishNode(src);

        assertMissingSpineScene(src, "spine");
    }

    // Rejects an explicitly empty legacy scene after it migrates to custom properties.
    @Test
    public void testEmptyLegacySpineSceneFailsBuild() throws Exception {
        StringBuilder src = createGui();
        startLegacySpineNode(src, "spine");
        src.append("  spine_scene: \"\"\n");
        finishNode(src);

        assertMissingSpineScene(src, "spine");
    }

    // Rejects an empty custom scene even when the legacy field contains a valid fallback.
    @Test
    public void testEmptyCustomSpineSceneFailsBuild() throws Exception {
        StringBuilder src = createGui();
        addSpineResource(src, "spineboy");
        startSpineCustomNode(src, "spine");
        addLegacySpineProperties(src);
        src.append("  custom_properties { id: \"spine_scene\" type: TYPE_STRING string: \"\" }\n");
        finishNode(src);

        assertMissingSpineScene(src, "spine");
    }

    // Keeps empty resource properties valid unless the extension explicitly marks them required.
    @Test
    public void testOptionalCustomResourceMayBeEmpty() throws Exception {
        StringBuilder src = createGui();
        startCustomNode(src, "optional", "OptionalResource");
        finishNode(src);

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        Assert.assertEquals("", findCustomProperty(findNode(gui, "", "optional"), "resource").getString());
    }

    // Rejects a layout override that clears an otherwise valid required Spine scene.
    @Test
    public void testLayoutCannotClearRequiredSpineScene() throws Exception {
        StringBuilder src = createGui();
        addSpineResource(src, "spineboy");
        startSpineCustomNode(src, "spine");
        src.append("  custom_properties { id: \"spine_scene\" type: TYPE_STRING string: \"spineboy\" }\n");
        finishNode(src);
        startLayout(src, "Landscape");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, "spine", "", false, List.of());
        src.append("  custom_properties { id: \"spine_scene\" type: TYPE_STRING string: \"\" }\n");
        finishNode(src);
        finishLayout(src);

        assertGuiBuildError(src, "GUI node 'spine' in layout 'Landscape' must specify a resource for custom property 'spine_scene'");
    }

    // Rejects a template override that clears a required resource after template expansion.
    @Test
    public void testTemplateCannotClearRequiredSpineScene() throws Exception {
        StringBuilder templateSrc = createGui();
        addSpineResource(templateSrc, "spineboy");
        startSpineCustomNode(templateSrc, "spine");
        addLegacySpineProperties(templateSrc);
        finishNode(templateSrc);
        addFile("/template.gui", templateSrc.toString());

        StringBuilder src = createGui();
        addTemplateNode(src, "template", "", "/template.gui");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, "template/spine", "template", true, List.of());
        src.append("  custom_properties { id: \"spine_scene\" type: TYPE_STRING string: \"\" }\n");
        finishNode(src);

        assertMissingSpineScene(src, "template/spine");
    }

    // Rejects a nonexistent required alias introduced after the template's resource checks.
    @Test
    public void testTemplateCannotSelectMissingRequiredResource() throws Exception {
        assertTemplateCannotSelectMissingResource("Spine", "spine_scene");
    }

    // Optional resources may be empty, but template overrides must still name an existing alias.
    @Test
    public void testTemplateCannotSelectMissingOptionalResource() throws Exception {
        assertTemplateCannotSelectMissingResource("OptionalResource", "resource");
    }

    private void assertTemplateCannotSelectMissingResource(String type, String property) throws Exception {
        StringBuilder templateSrc = createGui();
        startCustomNode(templateSrc, "node", type);
        finishNode(templateSrc);
        addFile("/template.gui", templateSrc.toString());

        StringBuilder src = createGui();
        addTemplateNode(src, "template", "", "/template.gui");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, "template/node", "template", true, List.of());
        src.append("  custom_properties { id: \"").append(property).append("\" type: TYPE_STRING string: \"missing\" }\n");
        finishNode(src);

        assertGuiBuildError(src, "GUI node 'template/node': custom property '" + property + "' refers to missing resource 'missing'");
    }

    // Defers alias validation so a parent can correct an unresolved resource in a reusable template.
    @Test
    public void testTemplateCanCorrectMissingResourceAlias() throws Exception {
        StringBuilder templateSrc = createGui();
        startSpineCustomNode(templateSrc, "spine");
        templateSrc.append("  custom_properties { id: \"spine_scene\" type: TYPE_STRING string: \"missing\" }\n");
        finishNode(templateSrc);
        addFile("/template.gui", templateSrc.toString());

        StringBuilder src = createGui();
        addSpineResource(src, "spineboy");
        addTemplateNode(src, "template", "", "/template.gui");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, "template/spine", "template", true, List.of());
        src.append("  custom_properties { id: \"spine_scene\" type: TYPE_STRING string: \"spineboy\" }\n");
        finishNode(src);

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        Assert.assertEquals("spineboy", findCustomProperty(findNode(gui, "", "template/spine"), "spine_scene").getString());
    }

    // Allows the parent GUI to supply a required resource that is omitted in a reusable template.
    @Test
    public void testTemplateOverridesMissingRequiredSpineScene() throws Exception {
        assertTemplateOverridesMissingRequiredSpineScene(false);
    }

    // Defers required resource checks until overrides have passed through every nested template.
    @Test
    public void testNestedTemplateOverridesMissingRequiredSpineScene() throws Exception {
        assertTemplateOverridesMissingRequiredSpineScene(true);
    }

    private void assertTemplateOverridesMissingRequiredSpineScene(boolean nested) throws Exception {
        StringBuilder templateSrc = createGui();
        startSpineCustomNode(templateSrc, "spine");
        finishNode(templateSrc);
        addFile("/template.gui", templateSrc.toString());
        String nodeId = "template/spine";
        if (nested) {
            StringBuilder outerTemplate = createGui();
            addTemplateNode(outerTemplate, "inner", "", "/template.gui");
            addFile("/outer.gui", outerTemplate.toString());
            nodeId = "template/inner/spine";
        }

        StringBuilder src = createGui();
        addSpineResource(src, "spineboy");
        addTemplateNode(src, "template", "", nested ? "/outer.gui" : "/template.gui");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, nodeId, nested ? "template/inner" : "template", true, List.of());
        src.append("  custom_properties { id: \"spine_scene\" type: TYPE_STRING string: \"spineboy\" }\n");
        finishNode(src);

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        Assert.assertEquals("spineboy", findCustomProperty(findNode(gui, "", nodeId), "spine_scene").getString());
    }

    // Keeps an inherited required scene valid when a template overrides another custom property.
    @Test
    public void testTemplateInheritsRequiredSpineScene() throws Exception {
        StringBuilder templateSrc = createGui();
        addSpineResource(templateSrc, "spineboy");
        startSpineCustomNode(templateSrc, "spine");
        templateSrc.append("  custom_properties { id: \"spine_scene\" type: TYPE_STRING string: \"spineboy\" }\n");
        finishNode(templateSrc);
        addFile("/template.gui", templateSrc.toString());

        StringBuilder src = createGui();
        addTemplateNode(src, "template", "", "/template.gui");
        startOverriddenNode(src, NodeDesc.Type.TYPE_CUSTOM, "template/spine", "template", true, List.of());
        src.append("  custom_properties { id: \"spine_default_animation\" type: TYPE_STRING string: \"run\" }\n");
        finishNode(src);

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        Assert.assertEquals("spineboy", findCustomProperty(findNode(gui, "", "template/spine"), "spine_scene").getString());
        Assert.assertEquals("run", findCustomProperty(findNode(gui, "", "template/spine"), "spine_default_animation").getString());
    }

    @Test
    public void testLayoutSpineOverridesAreMigrated() throws Exception {
        StringBuilder src = createGui();
        addSpineResource(src, "spineboy");
        startSpineCustomNode(src, "spine");
        addLegacySpineProperties(src);
        finishNode(src);

        startLayout(src, "Landscape");
        src.append("  nodes {\n");
        src.append("    type: TYPE_CUSTOM\n");
        src.append("    custom_type: 405028931\n");
        src.append("    id: \"spine\"\n");
        src.append("    overridden_fields: "+NodeDesc.SPINE_DEFAULT_ANIMATION_FIELD_NUMBER+"\n");
        src.append("    spine_default_animation: \"jump\"\n");
        src.append("  }\n");
        finishLayout(src);

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        NodeDesc node = findNode(gui, "Landscape", "spine");

        Assert.assertFalse(node.hasSpineDefaultAnimation());
        Assert.assertEquals("jump", findCustomProperty(node, "spine_default_animation").getString());
    }

    // Legacy Spine template child overrides use old Spine field numbers. When the
    // node is migrated to a custom Spine node, those overrides must become custom
    // property overrides so the template value does not win.
    @Test
    public void testTemplateSpineOverridesAreMigrated() throws Exception {
        StringBuilder templateSrc = createGui();
        addSpineResource(templateSrc, "spineboy");
        startLegacySpineNode(templateSrc, "spine");
        addLegacySpineProperties(templateSrc);
        finishNode(templateSrc);
        addFile("/template.gui", templateSrc.toString());

        StringBuilder referencingSrc = createGui();
        addTemplateNode(referencingSrc, "template", "", "/template.gui");
        startOverriddenNode(referencingSrc, NodeDesc.Type.TYPE_CUSTOM, "template/spine", "template", true, List.of(NodeDesc.SPINE_DEFAULT_ANIMATION_FIELD_NUMBER));
        referencingSrc.append("  spine_default_animation: \"jump\"\n");
        finishNode(referencingSrc);

        Gui.SceneDesc gui = buildGui(referencingSrc, "/test.gui");
        NodeDesc node = findNode(gui, "", "template/spine");

        Assert.assertNotNull(node);
        Assert.assertFalse(node.hasSpineDefaultAnimation());
        Assert.assertEquals("jump", findCustomProperty(node, "spine_default_animation").getString());
    }

    @Test
    public void testCustomPropertyDefaultsUseRegisteredTypes() throws Exception {
        StringBuilder src = createGui();
        startCustomNode(src, "typed", "Typed");
        finishNode(src);

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        NodeDesc node = findNode(gui, "", "typed");

        Assert.assertEquals(7, node.getCustomPropertiesCount());
        Assert.assertEquals(PropertyType.TYPE_NUMBER, findCustomProperty(node, "number").getType());
        Assert.assertEquals(1.5f, findCustomProperty(node, "number").getNumber(), EPSILON);
        Assert.assertEquals(PropertyType.TYPE_BOOLEAN, findCustomProperty(node, "boolean").getType());
        Assert.assertTrue(findCustomProperty(node, "boolean").getBoolean());
        Assert.assertEquals(PropertyType.TYPE_HASH, findCustomProperty(node, "hash").getType());
        Assert.assertEquals(MurmurHash.hash64("hash_value"), findCustomProperty(node, "hash").getHash());
        Assert.assertEquals(PropertyType.TYPE_STRING, findCustomProperty(node, "string").getType());
        Assert.assertEquals("text", findCustomProperty(node, "string").getString());
        Assert.assertEquals(PropertyType.TYPE_VECTOR3, findCustomProperty(node, "vector3").getType());
        Assert.assertEquals(3.0f, findCustomProperty(node, "vector3").getVector3().getZ(), EPSILON);
        Assert.assertEquals(PropertyType.TYPE_VECTOR4, findCustomProperty(node, "vector4").getType());
        Assert.assertEquals(7.0f, findCustomProperty(node, "vector4").getVector4().getW(), EPSILON);
        Assert.assertEquals(PropertyType.TYPE_QUAT, findCustomProperty(node, "quat").getType());
        Assert.assertEquals(11.0f, findCustomProperty(node, "quat").getQuat().getW(), EPSILON);
    }

    @Test
    public void testCustomTypeNameResolvesCustomType() throws Exception {
        StringBuilder src = createGui();
        startCustomNodeWithTypeName(src, "typed", "Typed");
        finishNode(src);

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        NodeDesc node = findNode(gui, "", "typed");

        Assert.assertEquals(NodeDesc.Type.TYPE_CUSTOM, node.getType());
        Assert.assertEquals(MurmurHash.hash32("Typed"), node.getCustomType());
        Assert.assertFalse(node.hasCustomTypeName());
        Assert.assertEquals(7, node.getCustomPropertiesCount());
        Assert.assertEquals(MurmurHash.hash64("hash_value"), findCustomProperty(node, "hash").getHash());
    }

    @Test
    public void testCustomTypesAreProjectLocal() throws Exception {
        Project project = new Project(new DefaultFileSystem());
        Project otherProject = new Project(new DefaultFileSystem());
        try {
            project.getGuiCustomTypeRegistry().register(TestTypedGuiNode.class);

            Assert.assertNotNull(project.getGuiCustomTypeRegistry().getByName("Typed"));
            Assert.assertNull(otherProject.getGuiCustomTypeRegistry().getByName("Typed"));
        } finally {
            project.dispose();
            otherProject.dispose();
        }
    }

    @Test(expected = CompileExceptionError.class)
    public void testUnknownCustomTypeNameFailsBuild() throws Exception {
        StringBuilder src = createGui();
        startCustomNodeWithTypeName(src, "missing", "Missing");
        finishNode(src);

        buildGui(src, "/test.gui");
    }

    @Test
    public void testHashCustomPropertyStringIsHashed() throws Exception {
        StringBuilder src = createGui();
        startCustomNode(src, "typed", "Typed");
        src.append("  custom_properties {\n");
        src.append("    id: \"hash\"\n");
        src.append("    type: TYPE_HASH\n");
        src.append("    string: \"readable_hash\"\n");
        src.append("  }\n");
        finishNode(src);

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        NodeDesc node = findNode(gui, "", "typed");
        Gui.Property property = findCustomProperty(node, "hash");

        Assert.assertNotNull(property);
        Assert.assertFalse(property.hasId());
        Assert.assertEquals(PropertyType.TYPE_HASH, property.getType());
        Assert.assertTrue(property.hasHash());
        Assert.assertFalse(property.hasString());
        Assert.assertEquals(MurmurHash.hash64("readable_hash"), property.getHash());
    }

    // Mirrors integration.gui-test/custom-gui-template-custom-property-override-preserves-base-properties-test.
    // A sparse template override of one custom property must preserve other non-default base custom properties.
    @Test
    public void testTemplateCustomPropertyOverridePreservesBaseProperties() throws Exception {
        StringBuilder templateSrc = createGui();
        startCustomNode(templateSrc, "typed", "Typed");
        templateSrc.append("  custom_properties {\n");
        templateSrc.append("    id: \"hash\"\n");
        templateSrc.append("    type: TYPE_HASH\n");
        templateSrc.append("    string: \"base_hash\"\n");
        templateSrc.append("  }\n");
        templateSrc.append("  custom_properties {\n");
        templateSrc.append("    id: \"number\"\n");
        templateSrc.append("    type: TYPE_NUMBER\n");
        templateSrc.append("    number: 2.0\n");
        templateSrc.append("  }\n");
        finishNode(templateSrc);
        addFile("/template.gui", templateSrc.toString());

        StringBuilder referencingSrc = createGui();
        addTemplateNode(referencingSrc, "template", "", "/template.gui");
        startOverriddenNode(referencingSrc, NodeDesc.Type.TYPE_CUSTOM, "template/typed", "template", true, List.of());
        referencingSrc.append("  custom_properties {\n");
        referencingSrc.append("    id: \"number\"\n");
        referencingSrc.append("    type: TYPE_NUMBER\n");
        referencingSrc.append("    number: 7.0\n");
        referencingSrc.append("  }\n");
        finishNode(referencingSrc);

        Gui.SceneDesc gui = buildGui(referencingSrc, "/test.gui");
        NodeDesc node = findNode(gui, "", "template/typed");

        Assert.assertNotNull(node);
        Assert.assertEquals(MurmurHash.hash64("base_hash"), findCustomProperty(node, "hash").getHash());
        Assert.assertEquals(7.0f, findCustomProperty(node, "number").getNumber(), EPSILON);
    }

    // https://github.com/defold/defold/issues/6151
    @Test
    public void testDefaultLayoutOverridesPriorityOverTemplateValues() throws Exception {
        StringBuilder src = createGuiWithTemplateAndLayout();
        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        NodeDesc node = findNode(gui, "Landscape", "template/text");

        Assert.assertNotNull("Can't find node!", node);
        Assert.assertFalse(node.getClippingVisible());
        Assert.assertEquals("defaultText", node.getText());
    }

    @Test
    public void testEnabledPropertyOverridesFromTemplate() throws Exception {
        StringBuilder src = createGui();
        addBoxNode(src, "box", "");
        addTextNode(src, "text", "box", "templateText");
        addFile("/template.gui", src.toString());

        src = createGui();
        String[] properties = {"  enabled: false\n"};
        addBoxNode(src, "box", "");
        addTemplateNode(src, "template", "box", "/template.gui", properties);

        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        NodeDesc boxNode = findNode(gui, "", "template/box");
        NodeDesc textNode = findNode(gui, "", "template/text");

        Assert.assertNotNull("Can't find box node!", boxNode);
        Assert.assertNotNull("Can't find text node!", textNode);
        Assert.assertFalse(boxNode.getEnabled());
        Assert.assertTrue(textNode.getEnabled());
    }

    @Test
    public void testOverrideNodeUniqueness() throws Exception {
        // Template gui file.
        StringBuilder templateSrc = createGui();
        addTextNode(templateSrc, "text", "", "templateText");
        addFile("/template.gui", templateSrc.toString());

        // Referencing gui file.
        StringBuilder referencingSrc = createGui();
        addTemplateNode(referencingSrc, "template", "", "/template.gui");

        // Override text in Default layout.
        startOverriddenNode(referencingSrc, NodeDesc.Type.TYPE_TEXT, "template/text", "template", true, List.of(NodeDesc.TEXT_FIELD_NUMBER));
        referencingSrc.append("  text: \"defaultText\"\n");
        finishNode(referencingSrc);

        // Override line_break in Landscape layout.
        startLayout(referencingSrc, "Landscape");
        startOverriddenNode(referencingSrc, NodeDesc.Type.TYPE_TEXT, "template/text", "template", true, List.of(NodeDesc.LINE_BREAK_FIELD_NUMBER));
        referencingSrc.append("  line_break: true\n");
        finishNode(referencingSrc);
        finishLayout(referencingSrc);

        // Build it and perform tests.
        Gui.SceneDesc templateGui = buildGui(templateSrc, "/template.gui");
        List<String> templateGuiSceneNodeIds = templateGui.getNodesList().stream().map(NodeDesc::getId).sorted().toList();
        Assert.assertEquals(List.of("text"), templateGuiSceneNodeIds);

        Gui.SceneDesc referencingGui = buildGui(referencingSrc, "/referencing.gui");
        List<String> referencingGuiSceneNodeIds = referencingGui.getNodesList().stream().map(NodeDesc::getId).sorted().toList();
        List<String> referencingGuiLayoutNodeIds = referencingGui.getLayoutsList().stream().flatMap(layoutDesc -> layoutDesc.getNodesList().stream()).map(NodeDesc::getId).sorted().toList();
        Assert.assertEquals(List.of("template/text"), referencingGuiSceneNodeIds);
        Assert.assertEquals(List.of("template/text"), referencingGuiLayoutNodeIds);
    }

    private static StringBuilder createSparseTemplateGui() {
        StringBuilder src = createGui();

        // A red box whose properties will not be overridden.
        startBoxNode(src, "nonOverriddenBox", "");
        src.append("  color {\n");
        src.append("    y: 0.0\n");
        src.append("    z: 0.0\n");
        src.append("  }\n");
        finishNode(src);

        // A green box whose alpha property will be overridden.
        startBoxNode(src, "overriddenBox", "");
        src.append("  color {\n");
        src.append("    x: 0.0\n");
        src.append("    z: 0.0\n");
        src.append("  }\n");
        finishNode(src);

        return src;
    }

    @Test
    public void testSparseLayoutOverrides() throws Exception {
        StringBuilder src = createSparseTemplateGui();

        // Landscape layout with just the overriddenBox, and only its overridden fields specified.
        startLayout(src, "Landscape");
        startOverriddenNode(src, NodeDesc.Type.TYPE_BOX, "overriddenBox", "", false, List.of(NodeDesc.ALPHA_FIELD_NUMBER));
        src.append("  alpha: 0.5");
        finishNode(src);
        finishLayout(src);

        // Build it and perform tests.
        Gui.SceneDesc gui = buildGui(src, "/layoutOverrides.gui");

        NodeDesc overriddenBox = findNode(gui, "", "overriddenBox");
        Assert.assertEquals(0.0f, overriddenBox.getColor().getX(), EPSILON);
        Assert.assertEquals(1.0f, overriddenBox.getColor().getY(), EPSILON);
        Assert.assertEquals(0.0f, overriddenBox.getColor().getZ(), EPSILON);

        NodeDesc nonOverriddenBoxInLayout = findNode(gui, "Landscape", "nonOverriddenBox");
        NodeDesc overriddenBoxInLayout = findNode(gui, "Landscape", "overriddenBox");

        Assert.assertNull("nonOverriddenBox should not exist in the layout.", nonOverriddenBoxInLayout);
        Assert.assertNotNull("overriddenBox should exist in the layout.", overriddenBoxInLayout);
        Assert.assertEquals("overriddenBox.color.x should have the original value in the layout.", 0.0f, overriddenBoxInLayout.getColor().getX(), EPSILON);
        Assert.assertEquals("overriddenBox.color.y should have the original value in the layout.", 1.0f, overriddenBoxInLayout.getColor().getY(), EPSILON);
        Assert.assertEquals("overriddenBox.color.z should have the original value in the layout.", 0.0f, overriddenBoxInLayout.getColor().getZ(), EPSILON);
        Assert.assertEquals("overriddenBox.alpha should be overridden in the layout.", 0.5f, overriddenBoxInLayout.getAlpha(), EPSILON);
    }

    @Test
    public void testSparseTemplateOverrides() throws Exception {
        // Template gui scene.
        StringBuilder templateSrc = createSparseTemplateGui();
        addFile("/template.gui", templateSrc.toString());

        // Referencing gui scene.
        StringBuilder referencingSrc = createGui();
        addTemplateNode(referencingSrc, "template", "", "/template.gui");

        // Override node for the nonOverriddenBox. Parent differs from original.
        addOverriddenNode(referencingSrc, NodeDesc.Type.TYPE_BOX, "template/nonOverriddenBox", "template", true);

        // Override node for the overriddenBox with only its overridden fields specified.
        startOverriddenNode(referencingSrc, NodeDesc.Type.TYPE_BOX, "template/overriddenBox", "template", true, List.of(NodeDesc.ALPHA_FIELD_NUMBER));
        referencingSrc.append("  alpha: 0.5");
        finishNode(referencingSrc);

        // Build it and perform tests.
        Gui.SceneDesc gui = buildGui(referencingSrc, "/templateOverrides.gui");
        NodeDesc nonOverriddenBox = findNode(gui, "", "template/nonOverriddenBox");
        NodeDesc overriddenBox = findNode(gui, "", "template/overriddenBox");

        Assert.assertNotNull("template/nonOverriddenBox should exist.", nonOverriddenBox);
        Assert.assertEquals("template/nonOverriddenBox.color.x should have the original value.", 1.0f, nonOverriddenBox.getColor().getX(), EPSILON);
        Assert.assertEquals("template/nonOverriddenBox.color.y should have the original value.", 0.0f, nonOverriddenBox.getColor().getY(), EPSILON);
        Assert.assertEquals("template/nonOverriddenBox.color.z should have the original value.", 0.0f, nonOverriddenBox.getColor().getZ(), EPSILON);

        Assert.assertNotNull("template/overriddenBox should exist.", overriddenBox);
        Assert.assertEquals("template/overriddenBox.color.x should have the original value.", 0.0f, overriddenBox.getColor().getX(), EPSILON);
        Assert.assertEquals("template/overriddenBox.color.y should have the original value.", 1.0f, overriddenBox.getColor().getY(), EPSILON);
        Assert.assertEquals("template/overriddenBox.color.z should have the original value.", 0.0f, overriddenBox.getColor().getZ(), EPSILON);
        Assert.assertEquals("template/overriddenBox.alpha should be overridden by us.", 0.5f, overriddenBox.getAlpha(), EPSILON);
    }

    @Test
    public void testSparseNestedTemplateOverrides() throws Exception {
        // Template gui scene.
        StringBuilder templateSrc = createSparseTemplateGui();
        addFile("/template.gui", templateSrc.toString());

        // Inner referencing gui scene.
        StringBuilder innerReferencingSrc = createGui();
        addTemplateNode(innerReferencingSrc, "template", "", "/template.gui");

        // Override node for the nonOverriddenBox. Parent differs from original.
        addOverriddenNode(innerReferencingSrc, NodeDesc.Type.TYPE_BOX, "template/nonOverriddenBox", "template", true);

        // Override node for the overriddenBox with only its overridden fields specified.
        startOverriddenNode(innerReferencingSrc, NodeDesc.Type.TYPE_BOX, "template/overriddenBox", "template", true, List.of(NodeDesc.ALPHA_FIELD_NUMBER, NodeDesc.BLEND_MODE_FIELD_NUMBER));
        innerReferencingSrc.append("  alpha: 0.25");
        innerReferencingSrc.append("  blend_mode: BLEND_MODE_ADD");
        finishNode(innerReferencingSrc);

        addFile("/nestedInnerOverrides.gui", innerReferencingSrc.toString());

        // Outer referencing gui scene.
        StringBuilder outerReferencingSrc = createGui();
        addTemplateNode(outerReferencingSrc, "inner", "", "/nestedInnerOverrides.gui");

        // The non-overridden nodes with altered parent.
        addOverriddenNode(outerReferencingSrc, NodeDesc.Type.TYPE_TEMPLATE, "inner/template", "inner", true);
        addOverriddenNode(outerReferencingSrc, NodeDesc.Type.TYPE_BOX, "inner/template/nonOverriddenBox", "inner/template", true);

        // Override node for the overriddenBox with only its overridden fields specified.
        startOverriddenNode(outerReferencingSrc, NodeDesc.Type.TYPE_BOX, "inner/template/overriddenBox", "inner/template", true, List.of(NodeDesc.ALPHA_FIELD_NUMBER));
        outerReferencingSrc.append("  alpha: 0.5");
        finishNode(outerReferencingSrc);

        // Build it and perform tests.
        Gui.SceneDesc gui = buildGui(outerReferencingSrc, "/nestedOverrides.gui");
        NodeDesc nonOverriddenBox = findNode(gui, "", "inner/template/nonOverriddenBox");
        NodeDesc overriddenBox = findNode(gui, "", "inner/template/overriddenBox");

        Assert.assertNotNull("inner/template/nonOverriddenBox should exist.", nonOverriddenBox);
        Assert.assertEquals("inner/template/nonOverriddenBox.color.x should have the original value.", 1.0f, nonOverriddenBox.getColor().getX(), EPSILON);
        Assert.assertEquals("inner/template/nonOverriddenBox.color.y should have the original value.", 0.0f, nonOverriddenBox.getColor().getY(), EPSILON);
        Assert.assertEquals("inner/template/nonOverriddenBox.color.z should have the original value.", 0.0f, nonOverriddenBox.getColor().getZ(), EPSILON);

        Assert.assertNotNull("inner/template/overriddenBox should exist.", overriddenBox);
        Assert.assertEquals("inner/template/overriddenBox.color.x should have the original value.", 0.0f, overriddenBox.getColor().getX(), EPSILON);
        Assert.assertEquals("inner/template/overriddenBox.color.y should have the original value.", 1.0f, overriddenBox.getColor().getY(), EPSILON);
        Assert.assertEquals("inner/template/overriddenBox.color.z should have the original value.", 0.0f, overriddenBox.getColor().getZ(), EPSILON);
        Assert.assertEquals("inner/template/overriddenBox.blend_mode should have the overridden value from the inner scene.", NodeDesc.BlendMode.BLEND_MODE_ADD, overriddenBox.getBlendMode());
        Assert.assertEquals("inner/template/overriddenBox.alpha should be overridden by us.", 0.5f, overriddenBox.getAlpha(), EPSILON);
    }
}
