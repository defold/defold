package com.dynamo.bob.pipeline;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.xml.stream.XMLStreamException;

import org.apache.commons.io.FilenameUtils;

// import com.dynamo.bob.pipeline.Mesh;
import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.RigScene;
import com.dynamo.model.proto.Model;

import com.dynamo.model.proto.Model.ModelDesc;
import com.dynamo.bob.ProtoParams;
import com.dynamo.gui.proto.Gui.SceneDesc;

import com.dynamo.rig.proto.Rig;
import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.AnimationTrack;
import com.dynamo.rig.proto.Rig.IKAnimationTrack;
import com.dynamo.rig.proto.Rig.Bone;
import com.dynamo.rig.proto.Rig.EventKey;
import com.dynamo.rig.proto.Rig.EventTrack;
import com.dynamo.rig.proto.Rig.IK;
import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.rig.proto.Rig.MeshAnimationTrack;
import com.dynamo.rig.proto.Rig.MeshEntry;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Skeleton;
import com.dynamo.rig.proto.Rig.RigAnimation;


@BuilderParams(name="ColladaModel", inExts=".dae", outExt=".rigscenec")
public class ColladaModelBuilder extends Builder<Void>  {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
            .setName(params.name())
            .addInput(input);
        taskBuilder.addOutput(input.changeExt(params.outExt()));
        taskBuilder.addOutput(input.changeExt(".skeletonc"));
        taskBuilder.addOutput(input.changeExt(".meshsetc"));
        taskBuilder.addOutput(input.changeExt(".animationsetc"));
        return taskBuilder.build();
    }


    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        ByteArrayOutputStream out;

        // MeshSet
        ByteArrayInputStream mesh_is = new ByteArrayInputStream(task.input(0).getContent());
        Mesh mesh;
        try {
            mesh = ColladaUtil.loadMesh(mesh_is);
        } catch (XMLStreamException e) {
            throw new CompileExceptionError(task.input(0), e.getLocation().getLineNumber(), "Failed to compile mesh", e);
        } catch (LoaderException e) {
            throw new CompileExceptionError(task.input(0), -1, "Failed to compile mesh", e);
        }
        MeshSet.Builder meshSetBuilder = MeshSet.newBuilder();
        MeshEntry.Builder meshEntryBuilder = MeshEntry.newBuilder();
        meshEntryBuilder.addMeshes(mesh);
        meshEntryBuilder.setId(0);
        meshSetBuilder.addMeshEntries(meshEntryBuilder);
        out = new ByteArrayOutputStream(64 * 1024);
        meshSetBuilder.build().writeTo(out);
        out.close();
        task.output(2).setContent(out.toByteArray());

        // Skeleton
        Skeleton.Builder skeletonBuilder = Skeleton.newBuilder();
        out = new ByteArrayOutputStream(64 * 1024);
        skeletonBuilder.build().writeTo(out);
        out.close();
        task.output(1).setContent(out.toByteArray());

        // AnimationSet
        AnimationSet.Builder animSetBuilder = AnimationSet.newBuilder();
        out = new ByteArrayOutputStream(64 * 1024);
        animSetBuilder.build().writeTo(out);
        out.close();
        task.output(3).setContent(out.toByteArray());

        // Rigscene
        com.dynamo.rig.proto.Rig.RigScene.Builder rigSceneBuilder = com.dynamo.rig.proto.Rig.RigScene.newBuilder();
        out = new ByteArrayOutputStream(64 * 1024);

        int buildDirLen = project.getBuildDirectory().length();
        rigSceneBuilder.setSkeleton(task.output(1).getPath().substring(buildDirLen));
        rigSceneBuilder.setMeshSet(task.output(2).getPath().substring(buildDirLen));
        rigSceneBuilder.setAnimationSet(task.output(3).getPath().substring(buildDirLen));
        rigSceneBuilder.setTextureSet(""); // this is set in the model

        rigSceneBuilder.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());

    }
}




