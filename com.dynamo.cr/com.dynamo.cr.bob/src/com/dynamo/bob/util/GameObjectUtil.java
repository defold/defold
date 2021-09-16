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

package com.dynamo.bob.util;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ProtoUtil;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.gameobject.proto.GameObject;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gamesys.proto.Sound.SoundDesc;
import com.google.protobuf.TextFormat;

public class GameObjectUtil {

    public static  PrototypeDesc.Builder loadPrototype(IResource input) throws IOException, CompileExceptionError {
        PrototypeDesc.Builder b = PrototypeDesc.newBuilder();
        ProtoUtil.merge(input, b);

        List<ComponentDesc> lst = b.getComponentsList();
        List<ComponentDesc> newList = new ArrayList<GameObject.ComponentDesc>();


        for (ComponentDesc componentDesc : lst) {
            // Convert .wav resource component to an embedded sound
            // We might generalize this in the future if necessary
            if (componentDesc.getComponent().endsWith(".wav")) {
                SoundDesc.Builder sd = SoundDesc.newBuilder().setSound(componentDesc.getComponent());
                EmbeddedComponentDesc ec = EmbeddedComponentDesc.newBuilder()
                    .setId(componentDesc.getId())
                    .setType("sound")
                    .setData(TextFormat.printToString(sd.build()))
                    .build();
                b.addEmbeddedComponents(ec);
            } else {
                newList.add(componentDesc);
            }
        }

        b.clearComponents();
        b.addAllComponents(newList);

        return b;
    }

}
