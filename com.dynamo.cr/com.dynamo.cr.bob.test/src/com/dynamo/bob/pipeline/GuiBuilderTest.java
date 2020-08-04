// Copyright 2020 The Defold Foundation
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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.Enumeration;
import java.util.List;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.Path;
import org.junit.Test;

import static org.junit.Assert.assertTrue;

import com.dynamo.gui.proto.Gui;
import com.google.protobuf.Message;

public class GuiBuilderTest extends AbstractProtoBuilderTest {

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

    @Test
    public void testSpineGui() throws Exception {
        addTestFiles();

        StringBuilder src = new StringBuilder();
        src.append("script: \"\"\n");
        src.append("nodes {\n");
        src.append("  type: TYPE_SPINE\n");
        src.append("  blend_mode: BLEND_MODE_ALPHA\n");
        src.append("  id: \"spine\"\n");
        src.append("  pivot: PIVOT_CENTER\n");
        src.append("  adjust_mode: ADJUST_MODE_FIT\n");
        src.append("  template_node_child: false\n");
        src.append("  size_mode: SIZE_MODE_AUTO\n");
        src.append("  spine_scene: \"spine_test\"\n");
        src.append("  spine_default_animation: \"\"\n");
        src.append("  spine_skin: \"\"\n");
        src.append("}\n");
        src.append("material: \"/builtins/materials/gui.material\"\n");
        src.append("adjust_reference: ADJUST_REFERENCE_PARENT\n");
        src.append("max_nodes: 512\n");
        src.append("spine_scenes {\n");
        src.append("  name: \"spine_test\"\n");
        src.append("  spine_scene: \"/test.spinescene\"\n");
        src.append("}");

        List<Message> outputs = build("/test.gui", src.toString());
        Gui.SceneDesc scene = (Gui.SceneDesc)outputs.get(0);
        assertTrue(nodeExists(scene, "spine/bone"));
        assertTrue(nodeExists(scene, "spine/bone2"));
        assertTrue(nodeExists(scene, "spine/bone3"));
        assertTrue(nodeExists(scene, "spine/bone4"));
        assertTrue(nodeExists(scene, "spine/bone5"));
    }

    @Test
    public void testTemplatedSpineGui() throws Exception {
        addTestFiles();

        StringBuilder src = new StringBuilder();
        src.append("script: \"\"");
        src.append("nodes {\n");
        src.append("  type: TYPE_TEMPLATE\n");
        src.append("  blend_mode: BLEND_MODE_ALPHA\n");
        src.append("  id: \"spine_templated\"\n");
        src.append("  xanchor: XANCHOR_NONE\n");
        src.append("  yanchor: YANCHOR_NONE\n");
        src.append("  pivot: PIVOT_CENTER\n");
        src.append("  adjust_mode: ADJUST_MODE_FIT\n");
        src.append("  layer: \"\"\n");
        src.append("  inherit_alpha: true\n");
        src.append("  clipping_mode: CLIPPING_MODE_NONE\n");
        src.append("  clipping_visible: true\n");
        src.append("  clipping_inverted: false\n");
        src.append("  alpha: 1.0\n");
        src.append("  template: \"/spine_templated.gui\"\n");
        src.append("  template_node_child: false\n");
        src.append("  size_mode: SIZE_MODE_AUTO\n");
        src.append("}\n");
        src.append("nodes {\n");
        src.append("  type: TYPE_SPINE\n");
        src.append("  blend_mode: BLEND_MODE_ALPHA\n");
        src.append("  id: \"spine_templated/spine\"\n");
        src.append("  xanchor: XANCHOR_NONE\n");
        src.append("  yanchor: YANCHOR_NONE\n");
        src.append("  pivot: PIVOT_CENTER\n");
        src.append("  adjust_mode: ADJUST_MODE_FIT\n");
        src.append("  parent: \"spine_templated\"\n");
        src.append("  layer: \"\"\n");
        src.append("  inherit_alpha: true\n");
        src.append("  clipping_mode: CLIPPING_MODE_NONE\n");
        src.append("  clipping_visible: true\n");
        src.append("  clipping_inverted: false\n");
        src.append("  alpha: 1.0\n");
        src.append("  template_node_child: true\n");
        src.append("  size_mode: SIZE_MODE_AUTO\n");
        src.append("  spine_scene: \"spine_test\"\n");
        src.append("  spine_default_animation: \"\"\n");
        src.append("  spine_skin: \"\"\n");
        src.append("}\n");
        src.append("material: \"/builtins/materials/gui.material\"\n");
        src.append("adjust_reference: ADJUST_REFERENCE_PARENT\n");
        src.append("max_nodes: 512\n");

        List<Message> outputs = build("/test.gui", src.toString());
        Gui.SceneDesc scene = (Gui.SceneDesc)outputs.get(0);
        assertTrue(nodeExists(scene, "spine_templated/spine/bone"));
        assertTrue(nodeExists(scene, "spine_templated/spine/bone2"));
        assertTrue(nodeExists(scene, "spine_templated/spine/bone3"));
        assertTrue(nodeExists(scene, "spine_templated/spine/bone4"));
        assertTrue(nodeExists(scene, "spine_templated/spine/bone5"));
    }
}
