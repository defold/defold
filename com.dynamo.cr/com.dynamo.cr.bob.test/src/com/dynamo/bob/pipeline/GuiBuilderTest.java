// Copyright 2020-2024 The Defold Foundation
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
import java.util.List;
import java.util.Set;
import java.util.Arrays;

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

    private StringBuilder createGui() {
        StringBuilder src = new StringBuilder();
        return src;
    }

    private Gui.SceneDesc buildGui(StringBuilder src, String path) throws Exception {
        return (Gui.SceneDesc)build(path, src.toString()).get(0);
    }

    private void addBoxNode(StringBuilder src, String id, String parent) {
        src.append("nodes {\n");
        src.append("  type: TYPE_BOX\n");
        src.append("  id: \""+id+"\"\n");
        src.append("  parent: \""+parent+"\"\n");
        src.append("}\n");
    }

    private void addTextNode(StringBuilder src, String id, String parent, String text) {
        src.append("nodes {\n");
        src.append("  type: TYPE_TEXT\n");
        src.append("  id: \""+id+"\"\n");
        src.append("  parent: \""+parent+"\"\n");
        src.append("  text: \""+text+"\"\n");
        src.append("}\n");
    }

    private void addTemplateNode(StringBuilder src, String id, String parent, String template, String[] extraProperties) {
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

    private void addTemplateNode(StringBuilder src, String id, String parent, String template) {
        String[] properties = {};
        addTemplateNode(src, id, parent, template, properties);
    }

    private void startOverridedNode(StringBuilder src, String type, String id, String parent, List<Integer> overrides) {
        src.append("nodes {\n");
        src.append("  type: "+type+"\n");
        src.append("  id: \""+id+"\"\n");
        src.append("  parent: \""+parent+"\"\n");
        for(int num : overrides) {
            src.append("  overridden_fields: "+num+"\n");
        }
    }

    private void finishOverridedNode(StringBuilder src) {
        src.append("}\n");
    }

    private void startLayout(StringBuilder src, String name) {
        src.append("layouts {\n");
        src.append("  name: \""+name+"\"\n");
    }

    private void finishLayout(StringBuilder src) {
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

        // override text in default layout
        startOverridedNode(src, "TYPE_TEXT", "template/text", "template/box", Arrays.asList(8));
        src.append("  text: \"defaultText\"\n");
        finishOverridedNode(src);

        startLayout(src, "Landscape");
        addBoxNode(src, "template/box", "");

        // override clipping_visible in Landscape layout
        startOverridedNode(src, "TYPE_TEXT", "template/text", "template/box", Arrays.asList(28));
        src.append("  clipping_visible: false\n");
        src.append("  text: \"defaultText\"\n");
        finishOverridedNode(src);

        finishLayout(src);
        return src;
    }

    private NodeDesc findNode(Gui.SceneDesc gui, String layoutName, String nodeName) {
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
}
