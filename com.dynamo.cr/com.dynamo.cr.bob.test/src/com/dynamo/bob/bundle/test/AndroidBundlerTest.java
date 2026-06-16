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

package com.dynamo.bob.bundle.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.IOException;

import org.junit.Test;

import com.dynamo.bob.Project;
import com.dynamo.bob.bundle.AndroidBundler;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.pipeline.ShaderCompilers;

public class AndroidBundlerTest {

    @Test
    public void testVkQualityPackagingFollowsGraphicsAdapters() throws IOException {
        Project project = new Project(new DefaultFileSystem(), ".", "build");

        assertTrue(AndroidBundler.usesVulkanGraphicsAdapter(project));

        project.setOption(ShaderCompilers.SHADER_ADAPTERS_OPTION, ShaderCompilers.SHADER_ADAPTER_OPENGLES);
        assertFalse(AndroidBundler.usesVulkanGraphicsAdapter(project));

        project.setOption(ShaderCompilers.SHADER_ADAPTERS_OPTION, String.join(",",
                ShaderCompilers.SHADER_ADAPTER_OPENGLES,
                ShaderCompilers.SHADER_ADAPTER_VULKAN));
        assertTrue(AndroidBundler.usesVulkanGraphicsAdapter(project));
    }
}
