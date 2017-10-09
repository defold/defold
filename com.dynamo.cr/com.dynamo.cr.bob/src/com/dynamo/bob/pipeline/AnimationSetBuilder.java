package com.dynamo.bob.pipeline;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import javax.xml.stream.XMLStreamException;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.AnimationSetDesc;
import com.dynamo.rig.proto.Rig.AnimationInstanceDesc;
import com.google.protobuf.TextFormat;


@BuilderParams(name="AnimationSet", inExts=".animationset", outExt=".animationsetc")
public class AnimationSetBuilder extends Builder<Void>  {

    ArrayList<String> animFiles;

    public static void collectAnimations(Task.TaskBuilder<Void> taskBuilder, Project project, IResource owner, AnimationSetDesc.Builder animSetDescBuilder) throws IOException, CompileExceptionError  {
        for(AnimationInstanceDesc instance : animSetDescBuilder.getAnimationsList()) {
            IResource animFile = BuilderUtil.checkResource(project, owner, "animationset", instance.getAnimation());
            taskBuilder.addInput(animFile);

            if(instance.getAnimation().endsWith(".animationset")) {
                ByteArrayInputStream animFileIS = new ByteArrayInputStream(animFile.getContent());
                InputStreamReader subAnimSetDescBuilderISR = new InputStreamReader(animFileIS);
                AnimationSetDesc.Builder subAnimSetDescBuilder = AnimationSetDesc.newBuilder();
                TextFormat.merge(subAnimSetDescBuilderISR, subAnimSetDescBuilder);
                collectAnimations(taskBuilder, project, owner, subAnimSetDescBuilder);
            }
        }
    }


    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
            .setName(params.name())
            .addInput(input);
        taskBuilder.addOutput(input.changeExt(params.outExt()));

        if( input.getAbsPath().endsWith(".animationset") ) {
            ByteArrayInputStream animFileIS = new ByteArrayInputStream(input.getContent());
            InputStreamReader animSetDescBuilderISR = new InputStreamReader(animFileIS);
            AnimationSetDesc.Builder animSetDescBuilder = AnimationSetDesc.newBuilder();
            TextFormat.merge(animSetDescBuilderISR, animSetDescBuilder);

            AnimationSetBuilder.collectAnimations(taskBuilder, this.project, input, animSetDescBuilder);
        }

        return taskBuilder.build();
    }

    private void validateFile(Task<Void> task, String path) throws CompileExceptionError {
        if(animFiles.contains(path)) {
            throw new CompileExceptionError(task.input(0), -1, "Animation file referenced more than once: " + path);
        }
        animFiles.add(path);
    }

    private void buildAnimations(Task<Void> task, AnimationSetDesc.Builder animSetDescBuilder, AnimationSet.Builder animationSetBuilder, String parentId) throws CompileExceptionError, IOException {
        ArrayList<String> idList = new ArrayList<>(animSetDescBuilder.getAnimationsCount());
        List<Long> boneList = new ArrayList<Long>();
        for(AnimationInstanceDesc instance : animSetDescBuilder.getAnimationsList()) {
            if(instance.getAnimation().endsWith(".animationset")) {
                IResource animFile = BuilderUtil.checkResource(this.project, task.input(0), "animationset", instance.getAnimation());
                validateFile(task, animFile.getAbsPath());
                ByteArrayInputStream animFileIS = new ByteArrayInputStream(animFile.getContent());
                InputStreamReader subAnimSetDescBuilderISR = new InputStreamReader(animFileIS);
                AnimationSetDesc.Builder subAnimSetDescBuilder = AnimationSetDesc.newBuilder();
                TextFormat.merge(subAnimSetDescBuilderISR, subAnimSetDescBuilder);
                buildAnimations(task, subAnimSetDescBuilder, animationSetBuilder, FilenameUtils.getBaseName(animFile.getPath()));
                continue;
            }
            IResource animFile = BuilderUtil.checkResource(this.project, task.input(0), "animation", instance.getAnimation());
            validateFile(task, animFile.getAbsPath());

            String animId = (parentId.isEmpty() ? "" : parentId + "/" ) + FilenameUtils.getBaseName(animFile.getPath());
            if(idList.contains(animId)) {
                throw new CompileExceptionError(task.input(0), -1, "Animation id already exists: " + animId);
            }
            idList.add(animId);

            ByteArrayInputStream animFileIS = new ByteArrayInputStream(animFile.getContent());
            AnimationSet.Builder animBuilder = AnimationSet.newBuilder();
            ArrayList<String> animationIds = new ArrayList<String>();
            try {
                ColladaUtil.loadAnimations(animFileIS, animBuilder, animId, animationIds);
            } catch (XMLStreamException e) {
                throw new CompileExceptionError(animFile, e.getLocation().getLineNumber(), "Failed to load animation: " + e.getLocalizedMessage(), e);
            } catch (LoaderException e) {
                throw new CompileExceptionError(animFile, -1, "Failed to load animation: " + e.getLocalizedMessage(), e);
            }

            animationSetBuilder.addAllAnimations(animBuilder.getAnimationsList());
            if(animBuilder.getBoneListList().size() > boneList.size()) {
                boneList = animBuilder.getBoneListList();
            }
        }
        animationSetBuilder.addAllBoneList(boneList);
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        // load input
        ByteArrayInputStream animSetDescIS = new ByteArrayInputStream(task.input(0).getContent());
        InputStreamReader animSetDescISR = new InputStreamReader(animSetDescIS);
        AnimationSetDesc.Builder animSetDescBuilder = AnimationSetDesc.newBuilder();
        TextFormat.merge(animSetDescISR, animSetDescBuilder);

        // evaluate hierarchy
        AnimationSet.Builder animationSetBuilder = AnimationSet.newBuilder();
        animFiles = new ArrayList<String>();
        animFiles.add(task.input(0).getAbsPath());
        buildAnimations(task, animSetDescBuilder, animationSetBuilder, "");

        // write merged animationset
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        animationSetBuilder.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}

