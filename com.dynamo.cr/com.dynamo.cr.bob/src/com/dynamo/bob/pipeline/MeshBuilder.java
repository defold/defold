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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;

import com.dynamo.gamesys.proto.MeshProto.MeshDesc;
import com.google.protobuf.TextFormat;

@ProtoParams(srcClass = MeshDesc.class, messageClass = MeshDesc.class)
@BuilderParams(name="Mesh", inExts=".mesh", outExt=".meshc")
public class MeshBuilder extends ProtoBuilder<MeshDesc.Builder> {

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        ByteArrayInputStream mesh_is = new ByteArrayInputStream(task.input(0).getContent());
        InputStreamReader mesh_isr = new InputStreamReader(mesh_is);
        MeshDesc.Builder meshDescBuilder = MeshDesc.newBuilder();
        TextFormat.merge(mesh_isr, meshDescBuilder);

        IResource resource = task.input(0);
        BuilderUtil.checkResource(this.project, resource, "vertices", meshDescBuilder.getVertices());
        meshDescBuilder.setVertices(ResourceUtil.minifyPathAndReplaceExt(meshDescBuilder.getVertices(), ".buffer", ".bufferc"));
        BuilderUtil.checkResource(this.project, resource, "material", meshDescBuilder.getMaterial());
        meshDescBuilder.setMaterial(ResourceUtil.minifyPathAndReplaceExt(meshDescBuilder.getMaterial(), ".material", ".materialc"));

        List<String> newTextureList = new ArrayList<String>();
        for (String t : meshDescBuilder.getTexturesList()) {
            if (t.isEmpty())
                continue;
            BuilderUtil.checkResource(this.project, resource, "texture", t);
            newTextureList.add(ProtoBuilders.replaceTextureName(t));
        }
        meshDescBuilder.clearTextures();
        meshDescBuilder.addAllTextures(newTextureList);

        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        meshDescBuilder.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}
