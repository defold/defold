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
import com.dynamo.gamesys.proto.Gui.SceneDesc.LayoutDesc;
import com.dynamo.gamesys.proto.Gui.NodeDesc;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ClassLoaderScanner;

public class GuiBuilderTest extends AbstractProtoBuilderTest {

    private static final float EPSILON = 0.00001f;
    private ClassLoaderScanner scanner = null;


    public GuiBuilderTest() throws IOException {
        this.scanner = new ClassLoaderScanner(this.getClass().getClassLoader());
        registerProtoBuilderNames();
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
        src.append("  id: \""+id+"\"\n");
        src.append("  parent: \""+parent+"\"\n");
    }

    private static void addBoxNode(StringBuilder src, String id, String parent) {
        startBoxNode(src, id, parent);
        finishNode(src);
    }

    private static void addTextNode(StringBuilder src, String id, String parent, String text) {
        src.append("nodes {\n");
        src.append("  type: TYPE_TEXT\n");
        src.append("  id: \""+id+"\"\n");
        src.append("  parent: \""+parent+"\"\n");
        src.append("  text: \""+text+"\"\n");
        src.append("}\n");
    }

    private static void addTemplateNode(StringBuilder src, String id, String parent, String template, String[] extraProperties) {
        src.append("nodes {\n");
        src.append("  type: TYPE_TEMPLATE\n");
        src.append("  id: \""+id+"\"\n");
        src.append("  parent: \""+parent+"\"\n");
        for (String prop: extraProperties) { 
            src.append(prop);
        }
        src.append("  template: \""+template+"\"\n");
        src.append("}\n");
    }

    private static void addTemplateNode(StringBuilder src, String id, String parent, String template) {
        String[] properties = {};
        addTemplateNode(src, id, parent, template, properties);
    }

    private static void startOverriddenNode(StringBuilder src, NodeDesc.Type type, String id, String parent, boolean templateNodeChild, List<Integer> overriddenFields) {
        src.append("nodes {\n");
        src.append("  type: "+type+"\n");
        src.append("  id: \""+id+"\"\n");
        src.append("  parent: \""+parent+"\"\n");
        src.append("  template_node_child: "+templateNodeChild+"\n");
        for(int num : overriddenFields) {
            src.append("  overridden_fields: "+num+"\n");
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
        src.append("  name: \""+name+"\"\n");
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
        List<NodeDesc> nodesList = Arrays.asList();
        if (layoutName.equals("")) {
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

    @Test
    public void test() throws Exception {
        // Kept empty as a future working template
    }

    // https://github.com/defold/defold/issues/6151
    @Test
    public void testDefaultLayoutOverridesPriorityOverTemplateValues() throws Exception {
        StringBuilder src = createGuiWithTemplateAndLayout();
        Gui.SceneDesc gui = buildGui(src, "/test.gui");
        NodeDesc node = findNode(gui, "Landscape", "template/text");

        Assert.assertFalse("Can't find node!", node == null);
        Assert.assertFalse(node.getClippingVisible());
        Assert.assertEquals(node.getText(), "defaultText");
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

        Assert.assertFalse("Can't find box node!", boxNode == null);
        Assert.assertFalse("Can't find text node!", textNode == null);
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
