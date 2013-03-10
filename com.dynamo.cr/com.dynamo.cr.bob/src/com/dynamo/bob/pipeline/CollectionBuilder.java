package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.util.HashSet;
import java.util.Set;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.PropertiesUtil;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.ComponentPropertyDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
import com.dynamo.gameobject.proto.GameObject.PropertyDesc;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;

@ProtoParams(messageClass = CollectionDesc.class)
@BuilderParams(name="Collection", inExts=".collection", outExt=".collectionc")
public class CollectionBuilder extends ProtoBuilder<CollectionDesc.Builder> {

    private void collectSubCollections(CollectionDesc.Builder collection, Set<IResource> subCollections) throws CompileExceptionError, IOException {
        for (CollectionInstanceDesc sub : collection.getCollectionInstancesList()) {
            IResource subResource = project.getResource(sub.getCollection());
            subCollections.add(subResource);
            CollectionDesc.Builder builder = CollectionDesc.newBuilder();
            ProtoUtil.merge(subResource, builder);
            collectSubCollections(builder, subCollections);
        }
    }

    private int countEmbeddedOutputs(CollectionDesc.Builder builder) throws IOException, CompileExceptionError {
        int count = 0;
        count += builder.getEmbeddedInstancesCount();
        for (CollectionInstanceDesc c : builder.getCollectionInstancesList()) {
            CollectionDesc.Builder b = CollectionDesc.newBuilder();
            ProtoUtil.merge(project.getResource(c.getCollection()), b);
            count += countEmbeddedOutputs(b);
        }
        return count;
    }

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));
        CollectionDesc.Builder builder = CollectionDesc.newBuilder();
        ProtoUtil.merge(input, builder);
        Set<IResource> subCollections = new HashSet<IResource>();
        collectSubCollections(builder, subCollections);
        for (IResource subCollection : subCollections) {
            taskBuilder.addInput(subCollection);
        }

        String name = FilenameUtils.getBaseName(input.getPath());
        int embeddedCount = countEmbeddedOutputs(builder);
        for (int i = 0; i < embeddedCount; ++i) {
            String embedName = String.format("%s_generated_%d.go", name, i);
            IResource genResource = input.getResource(embedName).output();
            taskBuilder.addOutput(genResource);
        }

        return taskBuilder.build();
    }

    private void mergeSubCollections(IResource owner, CollectionDesc.Builder collectionBuilder) throws IOException, CompileExceptionError {
        Set<String> childIds = new HashSet<String>();
        for (CollectionInstanceDesc collInst : collectionBuilder.getCollectionInstancesList()) {
            IResource collResource = this.project.getResource(collInst.getCollection());
            CollectionDesc.Builder subCollBuilder = CollectionDesc.newBuilder();
            ProtoUtil.merge(collResource, subCollBuilder);
            mergeSubCollections(owner, subCollBuilder);
            // Collect child ids
            childIds.clear();
            for (InstanceDesc inst : subCollBuilder.getInstancesList()) {
                childIds.addAll(inst.getChildrenList());
            }
            Point3d p = MathUtil.ddfToVecmath(collInst.getPosition());
            Quat4d r = MathUtil.ddfToVecmath(collInst.getRotation());
            double s = collInst.getScale();
            String pathPrefix = collInst.getId() + "/";
            for (InstanceDesc inst : subCollBuilder.getInstancesList()) {
                InstanceDesc.Builder instBuilder = InstanceDesc.newBuilder(inst);
                BuilderUtil.checkFile(this.project, owner, "prototype", inst.getPrototype());
                // merge id
                instBuilder.setId(pathPrefix + inst.getId());
                // merge transform for non-children
                if (!childIds.contains(inst.getId())) {
                    Point3d instP = MathUtil.ddfToVecmath(inst.getPosition());
                    if (subCollBuilder.getScaleAlongZ() != 0) {
                        instP.scale(s);
                    } else {
                        instP.set(s * instP.getX(), s * instP.getY(), instP.getZ());
                    }
                    MathUtil.rotate(r, instP);
                    instP.add(p);
                    instBuilder.setPosition(MathUtil.vecmathToDDF(instP));
                    Quat4d instR = MathUtil.ddfToVecmath(inst.getRotation());
                    instR.mul(r, instR);
                    instBuilder.setRotation(MathUtil.vecmathToDDF(instR));
                    instBuilder.setScale((float)(s * inst.getScale()));
                }
                // adjust child ids
                for (int i = 0; i < instBuilder.getChildrenCount(); ++i) {
                    instBuilder.setChildren(i, pathPrefix + instBuilder.getChildren(i));
                }
                // add merged instance
                collectionBuilder.addInstances(instBuilder);
            }
            for (EmbeddedInstanceDesc inst : subCollBuilder.getEmbeddedInstancesList()) {
                EmbeddedInstanceDesc.Builder instBuilder = EmbeddedInstanceDesc.newBuilder(inst);
                // merge id
                instBuilder.setId(pathPrefix + inst.getId());
                // merge transform for non-children
                if (!childIds.contains(inst.getId())) {
                    Point3d instP = MathUtil.ddfToVecmath(inst.getPosition());
                    if (subCollBuilder.getScaleAlongZ() != 0) {
                        instP.scale(s);
                    } else {
                        instP.set(s * instP.getX(), s * instP.getY(), instP.getZ());
                    }
                    MathUtil.rotate(r, instP);
                    instP.add(p);
                    instBuilder.setPosition(MathUtil.vecmathToDDF(instP));
                    Quat4d instR = MathUtil.ddfToVecmath(inst.getRotation());
                    instR.mul(r, instR);
                    instBuilder.setRotation(MathUtil.vecmathToDDF(instR));
                    instBuilder.setScale((float)(s * inst.getScale()));
                }
                // adjust child ids
                for (int i = 0; i < instBuilder.getChildrenCount(); ++i) {
                    instBuilder.setChildren(i, pathPrefix + instBuilder.getChildren(i));
                }
                // add merged instance
                collectionBuilder.addEmbeddedInstances(instBuilder);
            }
        }
        collectionBuilder.clearCollectionInstances();
    }

    @Override
    protected CollectionDesc.Builder transform(Task<Void> task, IResource resource, CollectionDesc.Builder messageBuilder) throws CompileExceptionError, IOException {

        mergeSubCollections(resource, messageBuilder);

        int embedIndex = 0;
        for (EmbeddedInstanceDesc desc : messageBuilder.getEmbeddedInstancesList()) {
            IResource genResource = task.getOutputs().get(embedIndex+1);
            genResource.setContent(desc.getData().getBytes());
            Task<?> embedTask = project.buildResource(genResource);
            if (embedTask == null) {
                throw new CompileExceptionError(resource,
                                                0,
                                                String.format("Failed to create build task for component '%s'", desc.getId()));
            }
            embedTask.setProductOf(task);

            int buildDirLen = project.getBuildDirectory().length();
            String path = genResource.getPath().substring(buildDirLen);

            InstanceDesc.Builder instBuilder = InstanceDesc.newBuilder();
            instBuilder.setId(desc.getId())
                .setPrototype(path)
                .addAllChildren(desc.getChildrenList())
                .setPosition(desc.getPosition())
                .setRotation(desc.getRotation())
                .setScale(desc.getScale());

            messageBuilder.addInstances(instBuilder);
            ++embedIndex;
        }
        messageBuilder.clearEmbeddedInstances();

        for (int i = 0; i < messageBuilder.getInstancesCount(); ++i) {
            InstanceDesc.Builder b = InstanceDesc.newBuilder().mergeFrom(messageBuilder.getInstances(i));
            b.setId("/" + b.getId());
            for (int j = 0; j < b.getChildrenCount(); ++j) {
                b.setChildren(j, "/" + b.getChildren(j));
            }
            b.setPrototype(BuilderUtil.replaceExt(b.getPrototype(), ".go", ".goc"));
            for (int j = 0; j < b.getComponentPropertiesCount(); ++j) {
                ComponentPropertyDesc.Builder compPropBuilder = ComponentPropertyDesc.newBuilder(b.getComponentProperties(j));
                PropertyDeclarations.Builder properties = PropertyDeclarations.newBuilder();
                for (PropertyDesc desc : compPropBuilder.getPropertiesList()) {
                    if (!PropertiesUtil.transformPropertyDesc(resource, desc, properties)) {
                        throw new CompileExceptionError(resource, 0, String.format("The property %s.%s.%s has an invalid format: %s", b.getId(), compPropBuilder.getId(), desc.getId(), desc.getValue()));
                    }
                }
                compPropBuilder.setPropertyDecls(properties);
                b.setComponentProperties(j, compPropBuilder);
            }
            messageBuilder.setInstances(i, b);
        }

        return messageBuilder;
    }

}
