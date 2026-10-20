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

import org.junit.Test;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.dynamo.bob.CompileExceptionError;

public class RenderTargetBuilderTest extends AbstractProtoBuilderTest {

    // Reject empty and zero-sized attachments, even after a valid color attachment.
    @Test
    public void testInvalidAttachments() throws Exception {
        String[] attachments = {
            "",
            "color_attachments { width: 0 height: 0 }",
            "color_attachments { width: 0 height: 32 }",
            "color_attachments { width: 64 height: 0 }",
            "color_attachments { width: 64 height: 32 } color_attachments { width: 0 height: 0 }",
            "color_attachments { width: 64 height: 32 } color_attachments { width: 0 height: 32 }",
            "color_attachments { width: 64 height: 32 } color_attachments { width: 64 height: 0 }"
        };
        for (int i = 0; i < attachments.length; ++i) {
            try {
                build("/invalid" + i + ".render_target", attachments[i]);
                fail("Expected invalid attachments to fail: " + attachments[i]);
            } catch (CompileExceptionError e) {
                assertTrue(e.getMessage().contains(i == 0 ? "at least one attachment" : "non-zero dimensions"));
            }
        }
    }

    // A depth/stencil-only target is valid and must not be rejected as attachment-free.
    @Test
    public void testDepthStencilOnly() throws Exception {
        ParseUtil.addParser("render_targetc", com.dynamo.render.proto.RenderTarget.RenderTargetDesc::parseFrom);
        build("/depth.render_target", "depth_stencil_attachment { width: 64 height: 32 texture_storage: true }\n");
    }

    // Color-only targets remain valid when the depth/stencil attachment is disabled.
    @Test
    public void testColorOnly() throws Exception {
        ParseUtil.addParser("render_targetc", com.dynamo.render.proto.RenderTarget.RenderTargetDesc::parseFrom);
        build("/minimal.render_target", "color_attachments { width: 1 height: 1 }\n");
        build("/color.render_target", "color_attachments { width: 64 height: 32 }\n");
    }

    @Test(expected = CompileExceptionError.class)
    public void testUnsupportedTextureType() throws Exception {
        build("/unsupported.render_target",
              "type: TYPE_3D\n" +
              "color_attachments {\n" +
              "  width: 16\n" +
              "  height: 16\n" +
              "}\n");
    }
}
