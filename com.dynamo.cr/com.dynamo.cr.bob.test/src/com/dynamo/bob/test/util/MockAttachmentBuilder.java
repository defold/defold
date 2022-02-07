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

package com.dynamo.bob.test.util;

import com.dynamo.bob.util.RigUtil.AttachmentBuilder;

public class MockAttachmentBuilder extends AttachmentBuilder {

    public MockAttachmentBuilder(com.dynamo.rig.proto.Rig.MeshAnimationTrack.Builder builder) {
        super(builder);
    }

    public int GetMeshAttachment(int index) {
        return builder.getMeshAttachment(index);
    }

    public int GetMeshAttachmentCount() {
        return builder.getMeshAttachmentCount();
    }
}
