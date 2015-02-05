package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.util.LinkedList;
import java.util.List;
import java.util.regex.Pattern;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;

import com.dynamo.graphics.proto.Graphics.PathSettings;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.dynamo.graphics.proto.Graphics.TextureProfiles;

@BuilderParams(name = "TextureProfiles", inExts = {".texture_profiles"}, outExt = ".texture_profilesc")
public class TextureProfilesBuilder extends Builder<Void>  {


    //@Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {

        TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                                     .setName(params.name())
                                     .addInput(input)
                                     .addOutput(input.changeExt(params.outExt()));

        return taskBuilder.build();

    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        IResource input = task.input(0);

        TextureProfiles.Builder builder = TextureProfiles.newBuilder();
        ProtoUtil.merge(input, builder);

        // If Bob is building for a specific platform, we need to
        // filter out any platform entries not relevant to the target platform.
        // (i.e. we don't want win32 specific profiles lingering in android bundles)
        String targetPlatform = project.option("platform", null);
        if (targetPlatform != null) {

            // We are in "bundle" mode, we need to filter out any profiles
            // that does not match the target platform...
            List<TextureProfile> newProfiles = new LinkedList<TextureProfile>();
            for (int i = 0; i < builder.getProfilesCount(); i++) {

                TextureProfile profile = builder.getProfiles(i);
                TextureProfile.Builder profileBuilder = TextureProfile.newBuilder();
                profileBuilder.mergeFrom(profile);

                // Take the first matching platform entry
                PlatformProfile platformMatchedProfile = null;
                for (PlatformProfile platformProfile : profile.getPlatformsList()) {
                    if ( targetPlatform.matches(platformProfile.getPlatform())) {

                        profileBuilder.clearPlatforms();
                        profileBuilder.addPlatforms(platformProfile);
                        newProfiles.add( profileBuilder.build() );

                        break;
                    }
                }

            }

            // Update profiles list with new filtered one
            // Now it should only contain profiles with platform entries
            // relevant for the target platform...
            builder.clearProfiles();
            builder.addAllProfiles(newProfiles);
        }


        // Update path strings to regular expressions
        for (int i = 0; i < builder.getPathSettingsList().size(); i++) {

            PathSettings pathSettings = builder.getPathSettings(i);
            String regexPath = pathSettings.getPath().toString();

            PathSettings.Builder pathBuilder = PathSettings.newBuilder();
            pathBuilder.mergeFrom(pathSettings);

            // Convert Ant-glob syntax to regex
            // Example string:
            //
            // /first_dir/second_dir/**/test.png
            //
            // Steps:
            //
            // 1. "quote" string:
            // 2. find & replace ?
            // 3. find & replace *
            // 4. find & replace **

            regexPath = Pattern.quote( regexPath );                                        // Wraps string in regex quote
            regexPath = regexPath.replaceAll(              "\\?",     "\\\\E[^\\/]\\\\Q"); // turns ? into regex
            regexPath = regexPath.replaceAll(           "\\*\\*",        "\\\\E.*?\\\\Q"); // turns ** into regex
            regexPath = regexPath.replaceAll("\\*([^\\*\\?].*?)", "\\\\E[^\\/]*?\\\\Q$1"); // turns * into regex
            pathBuilder.setPath(regexPath);

            // TODO(sven): Use compiled Patterns instead of strings...
            builder.setPathSettings(i, pathBuilder.build());
        }

        // TODO(sven): We should do this ONLY ONCE...
        //             Having more than one .texture_profiles file will overwrite
        //             anyone read before...
        TextureProfiles textureProfiles = builder.build();
        this.project.setTextureProfiles(textureProfiles);

        // Write same data to output for now, we need this in runtime as well.
        IResource outfile = task.getOutputs().get(0);
        outfile.setContent( textureProfiles.toString().getBytes() );

        return;
    }

}
