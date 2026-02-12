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

import java.io.IOException;
import java.util.*;
import java.util.stream.Collectors;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

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
import com.dynamo.bob.util.ComponentsCounter;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.ComponentPropertyDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstancePropertyDesc;
import com.dynamo.gameobject.proto.GameObject.PropertyDesc;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;

@ProtoParams(srcClass = CollectionDesc.class, messageClass = CollectionDesc.class)
@BuilderParams(name="Collection", inExts=".collection", outExt=".collectionc")
public class CollectionBuilder extends ProtoBuilder<CollectionDesc.Builder> {
    private Map<IResource, Integer> compCounterInputsCount = new HashMap<>();

    private void collectSubCollections(CollectionDesc.Builder collection, Map<IResource, Integer> subCollections) throws CompileExceptionError, IOException {
        for (CollectionInstanceDesc sub : collection.getCollectionInstancesList()) {
            IResource subResource = project.getResource(sub.getCollection());
            subCollections.put(subResource, subCollections.getOrDefault(subResource, 0) + 1);
            CollectionDesc.Builder builder = CollectionDesc.newBuilder();
            ProtoUtil.merge(subResource, builder);
            collectSubCollections(builder, subCollections);
        }
    }

    private void createGeneratedResources(Project project, CollectionDesc.Builder builder,
        Map<Long, IResource> uniqueResources, Map<Long, IResource> allResources) throws IOException, CompileExceptionError {

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
            }
            allResources.put(hash, genResource);
        }

        for (CollectionInstanceDesc c : builder.getCollectionInstancesList()) {
            IResource collectionResource = this.project.getResource(c.getCollection());
            CollectionDesc.Builder subCollectionBuilder = CollectionDesc.newBuilder();
            ProtoUtil.merge(collectionResource, subCollectionBuilder);

            createGeneratedResources(project, subCollectionBuilder, uniqueResources, allResources);
        }
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()))
                .addOutput(input.changeExt(ComponentsCounter.EXT_COL));
        CollectionDesc.Builder builder = getSrcBuilder(input);
        createSubTasks(builder, taskBuilder);

        Map<IResource, Integer> subCollections = new HashMap<>();
        collectSubCollections(builder, subCollections);
        for (IResource subCollection : subCollections.keySet()) {
            IResource compCounterInput = input.getResource(ComponentsCounter.replaceExt(subCollection)).output();
            compCounterInputsCount.put(compCounterInput, 1);
        }

        for (InstanceDesc inst : builder.getInstancesList()) {
            InstanceDesc.Builder instBuilder = InstanceDesc.newBuilder(inst);
            List<ComponentPropertyDesc> sourceProperties = instBuilder.getComponentPropertiesList();
            for (ComponentPropertyDesc compProp : sourceProperties) {
                Map<String, String> resources = PropertiesUtil.getPropertyDescResources(project, compProp.getPropertiesList());
                for (Map.Entry<String, String> entry : resources.entrySet()) {
                    createSubTask(entry.getValue(), entry.getKey(), taskBuilder);
                }
            }

            IResource res = project.getResource(inst.getPrototype());
            IResource compCounterInput = input.getResource(ComponentsCounter.replaceExt(res)).output();
            compCounterInputsCount.put(compCounterInput, 1);
        }

        Map<Long, IResource> uniqueResources = new HashMap<>();
        Map<Long, IResource> allResources = new HashMap<>();
        createGeneratedResources(this.project, builder, uniqueResources, allResources);

        List<Task> embedTasks = new ArrayList<>();
        for (long hash : uniqueResources.keySet()) {
            IResource genResource = uniqueResources.get(hash);
            Task embedTask = createSubTask(genResource, taskBuilder);
            embedTasks.add(embedTask);
        }

        for (IResource genResource : allResources.values()) {
            Task embedTask = createSubTask(genResource, taskBuilder);
            // if embeded objects have factories, they should be in input for our collection
            Set<IResource> counterInputs = ComponentsCounter.getCounterInputs(embedTask);
            for(IResource res : counterInputs) {
                taskBuilder.addInput(res);
                compCounterInputsCount.put(res, ComponentsCounter.DYNAMIC_VALUE);
            }
        }

        Task task = taskBuilder.build();
        for (Task et : embedTasks) {
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
            Quat4d r = MathUtil.ddfToVecmath(collInst.getRotation(), "%s collection: %s".formatted(owner, collInst.getId()));

            Vector3d s;
            if (collInst.hasScale3()) {
                s = MathUtil.ddfToVecmath(collInst.getScale3());
            } else {
                double scale = collInst.getScale();
                s = new Vector3d(scale, scale, scale);
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
                    Quat4d instR = MathUtil.ddfToVecmath(inst.getRotation(), "%s gameobject: %s".formatted(owner, inst.getId()));
                    instR.mul(r, instR);
                    instBuilder.setRotation(MathUtil.vecmathToDDF(instR));
                    instBuilder.setScale3(MathUtil.vecmathToDDFOne(instS));
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
                    instP.set(s.getX() * instP.getX(), s.getY() * instP.getY(), s.getZ() * instP.getZ());
                    MathUtil.rotate(r, instP);
                    instP.add(p);
                    instBuilder.setPosition(MathUtil.vecmathToDDF(instP));
                    Quat4d instR = MathUtil.ddfToVecmath(inst.getRotation(), "%s go: %s".formatted(owner, inst.getId()));
                    instR.mul(r, instR);
                    instBuilder.setRotation(MathUtil.vecmathToDDF(instR));
                    instBuilder.setScale3(MathUtil.vecmathToDDFOne(instS));
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

    // Sort instances by hierarchy
    private List<InstanceDesc.Builder> sortInstancesByHierarchy(IResource resource, List<InstanceDesc.Builder> instances) {
        Map<String, InstanceDesc.Builder> instanceMap = new HashMap<>();
        Map<String, List<String>> children = new HashMap<>();

        for (InstanceDesc.Builder inst : instances) {
            instanceMap.put(inst.getId(), inst);
            children.put(inst.getId(), inst.getChildrenList());
        }

        Map<String, Integer> depthMap = new HashMap<>();
        for (String id : instanceMap.keySet()) {
            calculateDepth(id, children, depthMap, 0);
        }

        Map<Integer, List<InstanceDesc.Builder>> depthGroups = new HashMap<>();
        for (Map.Entry<String, Integer> entry : depthMap.entrySet()) {
            Integer depth = entry.getValue();
            if (!depthGroups.containsKey(depth)) {
                depthGroups.put(depth, new ArrayList<InstanceDesc.Builder>());
            }
            depthGroups.get(depth).add(instanceMap.get(entry.getKey()));
        }

        List<InstanceDesc.Builder> sortedInstances = new ArrayList<>();
        List<Integer> depths = new ArrayList<>(depthGroups.keySet());
        Collections.sort(depths);
        for (Integer depth : depths) {
            List<InstanceDesc.Builder> group = depthGroups.get(depth);
            Collections.sort(group, new Comparator<InstanceDesc.Builder>() {
                public int compare(InstanceDesc.Builder o1, InstanceDesc.Builder o2) {
                    return o1.getId().compareTo(o2.getId());
                }
            });
            sortedInstances.addAll(group);
        }

        return sortedInstances;
    }

    // Recursive function to calculate the depth of each instance
    private void calculateDepth(String id, Map<String, List<String>> children, Map<String, Integer> depthMap, int currentDepth) {
        if (depthMap.containsKey(id)) {
            return;
        }

        depthMap.put(id, currentDepth);
        if (children.containsKey(id)) {
            for (String childId : children.get(id)) {
                calculateDepth(childId, children, depthMap, currentDepth + 1);
            }
        }
    }

    @Override
    protected CollectionDesc.Builder transform(Task task, IResource resource, CollectionDesc.Builder messageBuilder) throws CompileExceptionError, IOException {
        Integer countOfRealEmbededObjects = messageBuilder.getEmbeddedInstancesCount();
        int goCount = messageBuilder.getInstancesCount();
        mergeSubCollections(resource, messageBuilder);
        ComponentsCounter.Storage compStorage = ComponentsCounter.createStorage();
        int embedIndex = 0;
        for (EmbeddedInstanceDesc desc : messageBuilder.getEmbeddedInstancesList()) {
            byte[] data = desc.getData().getBytes();
            long hash = MurmurHash.hash64(data, data.length);

            IResource genResource = project.getGeneratedResource(hash, "go");

            // TODO: We have to set content again here as distclean might have removed everything at this point
            // See comment in create. Should tasks be recreated after distclean (or before actual build?). See Project.java
            genResource.setContent(data);

            // mergeSubCollections() embeds instances, but we want to count only "real" embeded instances
            if (embedIndex < countOfRealEmbededObjects) {
                ComponentsCounter.countComponentsInEmbededObjects(project, genResource, data, compStorage);
                goCount++;
            }

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
                instBuilder.setScale3(MathUtil.vecmathToDDFOne(new Vector3d(s, s, s)));
            }

            messageBuilder.addInstances(instBuilder);
            ++embedIndex;
        }
        messageBuilder.clearEmbeddedInstances();

        // Sort instances by children/parent hierarchy
        List<InstanceDesc.Builder> sortedInstanceBuilders = sortInstancesByHierarchy(resource, messageBuilder.getInstancesList().stream()
                .map(InstanceDesc::newBuilder)
                .collect(Collectors.toList()));

        messageBuilder.clearInstances();
        for (InstanceDesc.Builder builder : sortedInstanceBuilders) {
            messageBuilder.addInstances(builder.build());
        }

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
                b.setScale3(MathUtil.vecmathToDDFOne(new Vector3d(s, s, s)));
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

        compStorage.add("goc", goCount);
        ComponentsCounter.sumInputs(compStorage, task.getInputs(), compCounterInputsCount);
        ComponentsCounter.copyDataToBuilder(compStorage, project, messageBuilder);
        task.output(1).setContent(compStorage.toByteArray());

        return messageBuilder;
    }

    @Override
    public void clearState() {
        super.clearState();
        compCounterInputsCount = null;
    }
}