package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.PropertiesUtil;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.ComponentPropertyDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstancePropertyDesc;
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

    private int buildEmbedded(IResource input, CollectionDesc.Builder builder, Task<Void> task, int embedIndex) throws IOException, CompileExceptionError {
        for (EmbeddedInstanceDesc desc : builder.getEmbeddedInstancesList()) {
            IResource genResource = task.getOutputs().get(embedIndex+1);
            // TODO: This is a hack!
            // If the file isn't created here GameObjectBuilder#create
            // can't run as the generated file is loaded in GameObjectBuilder#create
            // to search for embedded components.
            // How to solve?
            genResource.setContent(desc.getData().getBytes());

            Task<?> embedTask = project.buildResource(genResource);
            if (embedTask == null) {
                throw new CompileExceptionError(input,
                                                0,
                                                String.format("Failed to create build task for component '%s'", desc.getId()));
            }
            embedTask.setProductOf(task);
            ++embedIndex;
        }

        for (CollectionInstanceDesc c : builder.getCollectionInstancesList()) {
            IResource collResource = this.project.getResource(c.getCollection());
            CollectionDesc.Builder subCollBuilder = CollectionDesc.newBuilder();
            ProtoUtil.merge(collResource, subCollBuilder);
            embedIndex = buildEmbedded(input, subCollBuilder, task, embedIndex);
        }

        return embedIndex;
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

        Task<Void> task = taskBuilder.build();
        buildEmbedded(input, builder, task, 0);
        return task;
    }

    private Map<String, Map<String, PropertyDesc>> toMap(List<ComponentPropertyDesc> compProps) {
        Map<String, Map<String, PropertyDesc>> compMap = new HashMap<String, Map<String, PropertyDesc>>();
        for (ComponentPropertyDesc compProp : compProps) {
            if (!compMap.containsKey(compProp.getId())) {
                compMap.put(compProp.getId(), new HashMap<String, PropertyDesc>());
            }
            Map<String, PropertyDesc> propMap = compMap.get(compProp.getId());
            List<PropertyDesc> props = compProp.getPropertiesList();
            for (PropertyDesc prop : props) {
                propMap.put(prop.getId(), prop);
            }
        }
        return compMap;
    }

    private List<ComponentPropertyDesc> toList(Map<String, Map<String, PropertyDesc>> map) {
        List<ComponentPropertyDesc> result = new ArrayList<ComponentPropertyDesc>();
        for (Map.Entry<String, Map<String, PropertyDesc>> compEntry : map.entrySet()) {
            ComponentPropertyDesc.Builder builder = ComponentPropertyDesc.newBuilder();
            builder.setId(compEntry.getKey());
            builder.addAllProperties(compEntry.getValue().values());
            result.add(builder.build());
        }
        return result;
    }

    private List<ComponentPropertyDesc> mergeComponentProperties(List<ComponentPropertyDesc> sourceProps, List<ComponentPropertyDesc> overrideProps) {
        Map<String, Map<String, PropertyDesc>> sourceMap = toMap(sourceProps);
        Map<String, Map<String, PropertyDesc>> overrideMap = toMap(overrideProps);
        for (Map.Entry<String, Map<String, PropertyDesc>> entry : overrideMap.entrySet()) {
            Map<String, PropertyDesc> propMap = sourceMap.get(entry.getKey());
            if (propMap != null) {
                propMap.putAll(entry.getValue());
            } else {
                sourceMap.put(entry.getKey(), entry.getValue());
            }
        }
        return toList(sourceMap);
    }

    private void mergeSubCollections(IResource owner, CollectionDesc.Builder collectionBuilder) throws IOException, CompileExceptionError {
        Set<String> childIds = new HashSet<String>();
        Map<String, List<ComponentPropertyDesc>> properties = new HashMap<String, List<ComponentPropertyDesc>>();
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
            for (EmbeddedInstanceDesc inst : subCollBuilder.getEmbeddedInstancesList()) {
                childIds.addAll(inst.getChildrenList());
            }
            String pathPrefix = collInst.getId() + "/";
            // Collect instance properties
            properties.clear();
            for (InstancePropertyDesc instProps : collInst.getInstancePropertiesList()) {
                properties.put(pathPrefix + instProps.getId(), instProps.getPropertiesList());
            }
            Point3d p = MathUtil.ddfToVecmath(collInst.getPosition());
            Quat4d r = MathUtil.ddfToVecmath(collInst.getRotation());

            Vector3d s;
            if (collInst.hasScale3()) {
                s = MathUtil.ddfToVecmath(collInst.getScale3());
            } else {
                double scale = collInst.getScale();
                if (subCollBuilder.getScaleAlongZ() != 0) {
                    s = new Vector3d(scale, scale, scale);
                } else {
                    s = new Vector3d(scale, scale, 1);
                }
            }

            for (InstanceDesc inst : subCollBuilder.getInstancesList()) {
                InstanceDesc.Builder instBuilder = InstanceDesc.newBuilder(inst);
                BuilderUtil.checkResource(this.project, owner, "prototype", inst.getPrototype());
                // merge id
                String id = pathPrefix + inst.getId();
                instBuilder.setId(pathPrefix + inst.getId());
                // merge component properties
                if (properties.containsKey(id)) {
                    List<ComponentPropertyDesc> overrideProperties = properties.get(id);
                    List<ComponentPropertyDesc> sourceProperties = instBuilder.getComponentPropertiesList();
                    instBuilder.clearComponentProperties();
                    instBuilder.addAllComponentProperties(mergeComponentProperties(sourceProperties, overrideProperties));
                }

                // merge transform for non-children
                if (!childIds.contains(inst.getId())) {

                    Vector3d instS;
                    if (inst.hasScale3()) {
                        instS = MathUtil.ddfToVecmath(inst.getScale3());
                    } else {
                        double scale = inst.getScale();
                        instS = new Vector3d(scale, scale, scale);
                    }
                    instS.set(instS.getX() * s.getX(), instS.getY() * s.getY(), instS.getZ() * s.getZ());

                    Point3d instP = MathUtil.ddfToVecmath(inst.getPosition());
                    instP.set(s.getX() * instP.getX(), s.getY() * instP.getY(), s.getZ() * instP.getZ());

                    MathUtil.rotate(r, instP);
                    instP.add(p);
                    instBuilder.setPosition(MathUtil.vecmathToDDF(instP));
                    Quat4d instR = MathUtil.ddfToVecmath(inst.getRotation());
                    instR.mul(r, instR);
                    instBuilder.setRotation(MathUtil.vecmathToDDF(instR));
                    instBuilder.setScale3(MathUtil.vecmathToDDF(instS));
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
                String id = pathPrefix + inst.getId();
                instBuilder.setId(id);
                // merge component properties
                if (properties.containsKey(id)) {
                    List<ComponentPropertyDesc> overrideProperties = properties.get(id);
                    List<ComponentPropertyDesc> sourceProperties = instBuilder.getComponentPropertiesList();
                    instBuilder.clearComponentProperties();
                    instBuilder.addAllComponentProperties(mergeComponentProperties(sourceProperties, overrideProperties));
                }
                // merge transform for non-children
                if (!childIds.contains(inst.getId())) {
                    Vector3d instS;
                    if (inst.hasScale3()) {
                        instS = MathUtil.ddfToVecmath(inst.getScale3());
                    } else {
                        double scale = inst.getScale();
                        instS = new Vector3d(scale, scale, scale);
                    }
                    instS.set(instS.getX() * s.getX(), instS.getY() * s.getY(), instS.getZ() * s.getZ());

                    Point3d instP = MathUtil.ddfToVecmath(inst.getPosition());
                    if (subCollBuilder.getScaleAlongZ() != 0) {
                        instP.set(s.getX() * instP.getX(), s.getY() * instP.getY(), s.getZ() * instP.getZ());
                    } else {
                        instP.set(s.getX() * instP.getX(), s.getY() * instP.getY(), instP.getZ());
                    }
                    MathUtil.rotate(r, instP);
                    instP.add(p);
                    instBuilder.setPosition(MathUtil.vecmathToDDF(instP));
                    Quat4d instR = MathUtil.ddfToVecmath(inst.getRotation());
                    instR.mul(r, instR);
                    instBuilder.setRotation(MathUtil.vecmathToDDF(instR));
                    instBuilder.setScale3(MathUtil.vecmathToDDF(instS));
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
            // TODO: We have to set content again here as distclean might have removed everything at this point
            // See comment in create. Should tasks be recreated after distclean (or before actual build?). See Project.java
            genResource.setContent(desc.getData().getBytes());

            int buildDirLen = project.getBuildDirectory().length();
            String path = genResource.getPath().substring(buildDirLen);

            InstanceDesc.Builder instBuilder = InstanceDesc.newBuilder();
            instBuilder.setId(desc.getId())
                .setPrototype(path)
                .addAllChildren(desc.getChildrenList())
                .setPosition(desc.getPosition())
                .setRotation(desc.getRotation())
                .addAllComponentProperties(desc.getComponentPropertiesList());

            // Promote to scale3
            if (desc.hasScale3()) {
                instBuilder.setScale3(desc.getScale3());
            } else {
                double s = desc.getScale();
                instBuilder.setScale3(MathUtil.vecmathToDDF(new Vector3d(s, s, s)));
            }

            messageBuilder.addInstances(instBuilder);
            ++embedIndex;
        }
        messageBuilder.clearEmbeddedInstances();

        Collection<String> propertyResources = new HashSet<String>();
        for (int i = 0; i < messageBuilder.getInstancesCount(); ++i) {
            InstanceDesc.Builder b = InstanceDesc.newBuilder().mergeFrom(messageBuilder.getInstances(i));
            b.setId("/" + b.getId());
            for (int j = 0; j < b.getChildrenCount(); ++j) {
                b.setChildren(j, "/" + b.getChildren(j));
            }

            // Promote to scale3
            if (!b.hasScale3()) {
                double s = b.getScale();
                b.setScale3(MathUtil.vecmathToDDF(new Vector3d(s, s, s)));
            }

            b.setPrototype(BuilderUtil.replaceExt(b.getPrototype(), ".go", ".goc"));
            for (int j = 0; j < b.getComponentPropertiesCount(); ++j) {
                ComponentPropertyDesc.Builder compPropBuilder = ComponentPropertyDesc.newBuilder(b.getComponentProperties(j));
                PropertyDeclarations.Builder properties = PropertyDeclarations.newBuilder();
                for (PropertyDesc desc : compPropBuilder.getPropertiesList()) {
                    if (!PropertiesUtil.transformPropertyDesc(project, desc, properties, propertyResources)) {
                        throw new CompileExceptionError(resource, 0, String.format("The property %s.%s.%s has an invalid format: %s", b.getId(), compPropBuilder.getId(), desc.getId(), desc.getValue()));
                    }
                }
                compPropBuilder.setPropertyDecls(properties);
                b.setComponentProperties(j, compPropBuilder);
            }
            messageBuilder.setInstances(i, b);
        }
        messageBuilder.addAllPropertyResources(propertyResources);

        return messageBuilder;
    }

}
