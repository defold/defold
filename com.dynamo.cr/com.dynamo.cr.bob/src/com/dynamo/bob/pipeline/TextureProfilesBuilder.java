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

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.graphics.proto.Graphics;

import java.io.IOException;
import java.util.LinkedList;
import java.util.List;

@ProtoParams(srcClass = Graphics.TextureProfiles.class, messageClass = Graphics.TextureProfiles.class)
@BuilderParams(name = "TextureProfile", inExts = {".texture_profiles"}, outExt = ".texture_profilesc", paramsForSignature = {"platform"})
public class TextureProfilesBuilder extends ProtoBuilder<Graphics.TextureProfiles.Builder> {

    public void build(Task task) throws CompileExceptionError, IOException {
        Graphics.TextureProfiles.Builder texProfilesBuilder = Graphics.TextureProfiles.newBuilder();
        IResource texProfilesInput = task.firstInput();
        ProtoUtil.merge(texProfilesInput, texProfilesBuilder);

        // If Bob is building for a specific platform, we need to
        // filter out any platform entries not relevant to the target platform.
        // (i.e. we don't want win32 specific profiles lingering in android bundles)
        Platform targetPlatform = project.getPlatform();

        List<Graphics.TextureProfile> newProfiles = new LinkedList<Graphics.TextureProfile>();
        for (int i = 0; i < texProfilesBuilder.getProfilesCount(); i++) {

            Graphics.TextureProfile profile = texProfilesBuilder.getProfiles(i);
            Graphics.TextureProfile.Builder profileBuilder = Graphics.TextureProfile.newBuilder();
            profileBuilder.mergeFrom(profile);
            profileBuilder.clearPlatforms();

            // Take only the platforms that matches the target platform
            for (Graphics.PlatformProfile platformProfile : profile.getPlatformsList()) {
                if (targetPlatform.matchesOS(platformProfile.getOs())) {
                    profileBuilder.addPlatforms(platformProfile);
                }
            }

            newProfiles.add(profileBuilder.build());
        }

        // Update profiles list with new filtered one
        // Now it should only contain profiles with platform entries
        // relevant for the target platform...
        texProfilesBuilder.clearProfiles();
        texProfilesBuilder.addAllProfiles(newProfiles);

        // Add the current texture profiles to the project, since this
        // needs to be reachedable by the TextureGenerator.
        Graphics.TextureProfiles textureProfiles = texProfilesBuilder.build();

        task.output(0).setContent(textureProfiles.toByteArray());
    }
}
