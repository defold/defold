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

import com.dynamo.bob.CompileExceptionError;

public class RenderTargetBuilderTest extends AbstractProtoBuilderTest {

    // Reject empty resources before an attachment-free framebuffer reaches a graphics driver.
    @Test(expected = CompileExceptionError.class)
    public void testMissingAttachments() throws Exception {
        build("/empty.render_target", "");
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
