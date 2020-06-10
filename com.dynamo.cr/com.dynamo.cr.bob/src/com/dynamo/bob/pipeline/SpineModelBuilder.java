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

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.spine.proto.Spine.SpineModelDesc;

@ProtoParams(messageClass = SpineModelDesc.class)
@BuilderParams(name="SpineModel", inExts=".spinemodel", outExt=".spinemodelc")
public class SpineModelBuilder extends ProtoBuilder<SpineModelDesc.Builder> {

    @Override
    protected SpineModelDesc.Builder transform(Task<Void> task, IResource resource, SpineModelDesc.Builder messageBuilder) throws CompileExceptionError {
        BuilderUtil.checkResource(this.project, resource, "spineScene", messageBuilder.getSpineScene());
        messageBuilder.setSpineScene(BuilderUtil.replaceExt(messageBuilder.getSpineScene(), ".spinescene", ".rigscenec"));
        BuilderUtil.checkResource(this.project, resource, "material", messageBuilder.getMaterial());
        messageBuilder.setMaterial(BuilderUtil.replaceExt(messageBuilder.getMaterial(), ".material", ".materialc"));
        return messageBuilder;
    }
}
