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

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.HashMap;
import java.util.HashSet;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.PropertiesUtil;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.ComponentPropertyDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstancePropertyDesc;
import com.dynamo.gameobject.proto.GameObject.PropertyDesc;
import com.dynamo.gameobject.proto.GameObject.ComponenTypeDesc;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;

import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gamesys.proto.GameSystem.CollectionFactoryDesc;
import com.dynamo.gamesys.proto.GameSystem.FactoryDesc;

@ProtoParams(srcClass = CollectionDesc.class, messageClass = CollectionDesc.class)
@BuilderParams(name="Collection", inExts=".collection", outExt=".collectionc")
public class CollectionBuilder extends ProtoBuilder<CollectionDesc.Builder> {

    private Map<Long, IResource> uniqueResources = new HashMap<>();
    private Set<Long> productsOfThisTask = new HashSet<>();
    private List<Task<?>> embedTasks = new ArrayList<>();

    private HashMap<String, Integer> components = new HashMap<>();
    private HashMap<String, Integer> componentsInFactories = new HashMap<>();

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

    private void createGeneratedResources(Project project, CollectionDesc.Builder builder) throws IOException, CompileExceptionError {
        for (EmbeddedInstanceDesc desc : builder.getEmbeddedInstancesList()) {
            byte[] data = desc.getData().getBytes();
            long hash = MurmurHash.hash64(data, data.length);

            IResource genResource = project.getGeneratedResource(hash, "go");
            if (genResource == null) {
                genResource = project.createGeneratedResource(hash, "go");

                // TODO: This is a hack derived from the same problem with embedded gameobjects from collections (see CollectionBuilder.create)!
                // If the file isn't created here <EmbeddedComponent>#create
                // can't access generated resource data (embedded component desc)
                genResource.setContent(data);

                uniqueResources.put(hash, genResource);
                productsOfThisTask.add(hash);
            }
        }

        for (CollectionInstanceDesc c : builder.getCollectionInstancesList()) {
            IResource collectionResource = this.project.getResource(c.getCollection());
            CollectionDesc.Builder subCollectionBuilder = CollectionDesc.newBuilder();
            ProtoUtil.merge(collectionResource, subCollectionBuilder);

            createGeneratedResources(project, subCollectionBuilder);
        }
    }

    // TODO: do we really need this?
    // This method is never called. Is it mistake?
    private int buildEmbedded(IResource input, CollectionDesc.Builder builder, Task<Void> task, int embedIndex) throws IOException, CompileExceptionError {
        for (EmbeddedInstanceDesc desc : builder.getEmbeddedInstancesList()) {
            IResource genResource = task.getOutputs().get(embedIndex+1);

            // TODO: This is a hack!
            // If the file isn't created here GameObjectBuilder#create
            // can't run as the generated file is loaded in GameObjectBuilder#create
            // to search for embedded components.
            // How to solve?
            genResource.setContent(desc.getData().getBytes());

            Task<?> embedTask = project.createTask(genResource);
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

    private void addOneComponent(String component, Map<String, Integer> target) {
        target.put(component, target.getOrDefault(component, 0) + 1);
    }

    private void countComponentsInFactory(Project project, IResource input, String type) throws IOException, CompileExceptionError {
        if (type.equals("factory")) {
            FactoryDesc.Builder factoryDesc = FactoryDesc.newBuilder();
            ProtoUtil.merge(input, factoryDesc);
            IResource res = project.getResource(factoryDesc.getPrototype());

            countComponentInResource(project, res, componentsInFactories);
        } else if (type.equals("collectionfactory")) {
            CollectionFactoryDesc.Builder factoryDesc = CollectionFactoryDesc.newBuilder();
            ProtoUtil.merge(input, factoryDesc);
            IResource res = project.getResource(factoryDesc.getPrototype());
            CollectionDesc.Builder builder = CollectionDesc.newBuilder();
            ProtoUtil.merge(res, builder);

            countComponentsInCollectionRecursively(project, builder, componentsInFactories);
        }
    }

    private boolean isFactoryType(String type) throws IOException, CompileExceptionError {
        return type.equals("factory") || type.equals("collectionfactory");
    }

    private void countComponentInResource(Project project, IResource res, Map<String, Integer> target) throws IOException, CompileExceptionError {
        PrototypeDesc.Builder prot = PrototypeDesc.newBuilder();
        ProtoUtil.merge(res, prot);

        for (EmbeddedComponentDesc cd : prot.getEmbeddedComponentsList()) {
            String type = cd.getType();
            addOneComponent(type, target);
            if (isFactoryType(type)) {
                byte[] data = cd.getData().getBytes();
                long hash = MurmurHash.hash64(data, data.length);
                IResource resource = project.getGeneratedResource(hash, type);
                resource.setContent(data);
                countComponentsInFactory(project, resource, type);
            }
        }
        for (ComponentDesc cd : prot.getComponentsList()) {
            String comp = cd.getComponent();
            String type = FilenameUtils.getExtension(comp);
            addOneComponent(type, target);
            if (isFactoryType(type)) {
                IResource resource = project.getResource(comp);
                countComponentsInFactory(project, resource, type);
            }
        }
    }

    private void countComponentsInCollectionRecursively(Project project, CollectionDesc.Builder builder, Map<String, Integer> target) throws IOException, CompileExceptionError {
        for (InstanceDesc inst : builder.getInstancesList()) {
            IResource res = project.getResource(inst.getPrototype());
            countComponentInResource(project, res, target);
        }
        for (EmbeddedInstanceDesc desc : builder.getEmbeddedInstancesList()) {
            byte[] data = desc.getData().getBytes();
            long hash = MurmurHash.hash64(data, data.length);

            IResource res = project.getGeneratedResource(hash, "go");
            countComponentInResource(project, res, target);
        }
        for (CollectionInstanceDesc collInst : builder.getCollectionInstancesList()) {
            IResource collResource = project.getResource(collInst.getCollection());
            CollectionDesc.Builder subCollBuilder = CollectionDesc.newBuilder();
            ProtoUtil.merge(collResource, subCollBuilder);
            countComponentsInCollectionRecursively(project, subCollBuilder, target);
        }
    }

    private String replaceComponentName(Project project, String in) {
        return project.replaceExt("." + in).substring(1);
    }

    private void countAllComponentTypes(Project project, CollectionDesc.Builder builder) throws IOException, CompileExceptionError {
        countComponentsInCollectionRecursively(this.project, builder, components);

        HashMap<String, Integer> mergedComponents =  new HashMap<>();

        for (Map.Entry<String, Integer> entry : components.entrySet()) {
            String name = replaceComponentName(project, entry.getKey());
            // different input component names may have the same output name
            // for example wav ans sound both are soundc
            if (mergedComponents.containsKey(name)) {
                mergedComponents.put(name, mergedComponents.get(name) + entry.getValue());
            } else {
                mergedComponents.put(name, entry.getValue());
            }
        }

        // if component is in factory or collectionfactory use 0xFFFFFFFF
        for (Map.Entry<String, Integer> entry : componentsInFactories.entrySet()) {
            mergedComponents.put(replaceComponentName(project, entry.getKey()), 0xFFFFFFFF);
        }

        for (Map.Entry<String, Integer> entry : mergedComponents.entrySet()) {
            ComponenTypeDesc.Builder componentTypeDesc = ComponenTypeDesc.newBuilder();
            componentTypeDesc.setNameHash(MurmurHash.hash64(entry.getKey())).setMaxCount(entry.getValue());
            builder.addComponentTypes(componentTypeDesc);
        }
    }

    private void createResourcePropertyTasks(List<ComponentPropertyDesc> overrideProps, IResource input) throws CompileExceptionError {
        for (ComponentPropertyDesc compProp : overrideProps) {
            Collection<String> resources = PropertiesUtil.getPropertyDescResources(project, compProp.getPropertiesList());
            for(String r : resources) {
                IResource resource = BuilderUtil.checkResource(project, input, "resource", r);
                PropertiesUtil.createResourcePropertyTextureTask(project, resource, input);
            }
        }
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

        for (InstanceDesc inst : builder.getInstancesList()) {
            InstanceDesc.Builder instBuilder = InstanceDesc.newBuilder(inst);
            List<ComponentPropertyDesc> sourceProperties = instBuilder.getComponentPropertiesList();
            createResourcePropertyTasks(sourceProperties, input);
        }

        createGeneratedResources(this.project, builder);

        for (long hash : uniqueResources.keySet()) {
            IResource genResource = uniqueResources.get(hash);

            taskBuilder.addOutput(genResource);

            if (productsOfThisTask.contains(hash)) {

                Task<?> embedTask = project.createTask(genResource);
                if (embedTask == null) {
                    throw new CompileExceptionError(input,
                                                    0,
                                                    String.format("Failed to create build task for component '%s'", genResource.getPath()));
                }
                embedTasks.add(embedTask);
            }
        }

        Task<Void> task = taskBuilder.build();
        for (Task<?> et : embedTasks) {
            et.setProductOf(task);
        }
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
        countAllComponentTypes(project, messageBuilder);

        mergeSubCollections(resource, messageBuilder);

        int embedIndex = 0;
        for (EmbeddedInstanceDesc desc : messageBuilder.getEmbeddedInstancesList()) {
            byte[] data = desc.getData().getBytes();
            long hash = MurmurHash.hash64(data, data.length);

            IResource genResource = project.getGeneratedResource(hash, "go");

            // TODO: We have to set content again here as distclean might have removed everything at this point
            // See comment in create. Should tasks be recreated after distclean (or before actual build?). See Project.java
            genResource.setContent(data);

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
