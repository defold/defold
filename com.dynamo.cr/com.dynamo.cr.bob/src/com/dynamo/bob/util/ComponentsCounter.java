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

package com.dynamo.bob.util;

import org.apache.commons.io.FilenameUtils;

import java.util.Map;
import java.util.AbstractMap;
import java.util.Set;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Iterator;
import java.io.ByteArrayOutputStream;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ProtoUtil;
import com.dynamo.bob.pipeline.BuilderUtil;
import com.dynamo.gameobject.proto.GameObject.ComponenTypeDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gamesys.proto.GameSystem.CollectionFactoryDesc;
import com.dynamo.gamesys.proto.GameSystem.FactoryDesc;

/**
 * Class helps to count components in GOs and Collections
 */
public class ComponentsCounter {

    public static final String EXT_GO = ".compcount_go";
    public static final String EXT_COL = ".compcount_col";
    public static final Integer DYNAMIC_VALUE = 0xFFFFFFFF;

    /**
     * Class represents a storage for counted components which can be written on disk
     */
    public static class Storage implements Serializable {

        private static final long serialVersionUID = 1L;

        private Map<String, Integer> components = new HashMap<>();
        private Boolean isInDynamicFactory = false;

        public Map<String, Integer> get() {
            return components;
        }

        public void makeDynamic() {
            isInDynamicFactory = true;
        }

        public Boolean isDynamic() {
            return isInDynamicFactory;
        }

        public void add(String componentName, Integer count) {
            if (componentName.charAt(0) == '.') {
                componentName = componentName.substring(1);
            }
            Integer currentValue = components.getOrDefault(componentName, 0);
            Integer value = currentValue + count;
            // We can't multiply or sum DYNAMIC_VALUE, just set it
            if (count.equals(DYNAMIC_VALUE) || currentValue.equals(DYNAMIC_VALUE)) {
                value = DYNAMIC_VALUE;
            }
            components.put(componentName, value);
        }

        public void add(String componentName) {
            add(componentName, 1);
        }

        public void add(Storage storage) {
            Map<String, Integer> comps = storage.get();
            for (Map.Entry<String,Integer> entry : comps.entrySet()) {
                add(entry.getKey(), entry.getValue());
            }
        }

        public void add(Storage storage, Integer count) {
            Map<String, Integer> comps = storage.get();
            for (Map.Entry<String,Integer> entry : comps.entrySet()) {
                Integer value = entry.getValue();
                // We can't multiply or sum DYNAMIC_VALUE, just set it
                if (count.equals(DYNAMIC_VALUE) || value.equals(DYNAMIC_VALUE)) {
                    value = DYNAMIC_VALUE;
                }
                else {
                    value *= count;
                }
                add(entry.getKey(), value);
            }
        }

        public byte[] toByteArray() throws IOException {
            ByteArrayOutputStream bos = new ByteArrayOutputStream(128 * 16);
            ObjectOutputStream os = new ObjectOutputStream(bos);
            os.writeObject(this);
            os.close();
            return bos.toByteArray();
        }

        public String toString() {
            return components.toString();
        }

        public static Storage load(IResource resource) throws IOException {
            byte[] content = resource.getContent();
            if (content == null) {
                return new Storage();
            } else {
                try {
                    ObjectInputStream is = new ObjectInputStream(new ByteArrayInputStream(content));
                    Storage storage = (Storage) is.readObject();
                    return storage;
                } catch (Throwable e) {
                    System.err.println("Unable to load storage");
                    e.printStackTrace();
                    return new Storage();
                }
            }
        }
    }

    public static Storage createStorage() {
        return new Storage();
    }

    private static Boolean isFactoryType(String type, boolean compiled) {
        if (type.charAt(0) == '.') {
            type = type.substring(1);
        }
        if (compiled)
        {
            type = type.substring(0, type.length() - 1);
        }
        return type.equals("factory") || type.equals("collectionfactory");
    }

    public static Boolean isCompCounterStorage(String path) {
        return path.endsWith(EXT_GO) || path.endsWith(EXT_COL);
    }

    private static Map.Entry<String, Boolean> getCounterNameAndPrototypeInfo(String type, IResource resource) throws IOException, CompileExceptionError {
        if (type.equals("factory") || type.equals("collectionfactory")) {
            return getCounterNameAndPrototypeInfo(type, resource, resource.getContent());
        }
        return null;
    }
    private static Map.Entry<String, Boolean> getCounterNameAndPrototypeInfo(String type, IResource resource, byte[] resourceContent) throws IOException, CompileExceptionError {
        if (type.equals("factory")) {
            FactoryDesc.Builder factoryDesc = FactoryDesc.newBuilder();
            ProtoUtil.merge(resource, resourceContent, factoryDesc);
            Boolean isDynamic = factoryDesc.getDynamicPrototype();
            String counterName = BuilderUtil.replaceExt(factoryDesc.getPrototype(), ".go", EXT_GO);
            Map.Entry<String,Boolean> entry = new AbstractMap.SimpleEntry<String, Boolean>(counterName, isDynamic);
            return entry;
        } else if (type.equals("collectionfactory")) {
            CollectionFactoryDesc.Builder factoryDesc = CollectionFactoryDesc.newBuilder();
            ProtoUtil.merge(resource, resourceContent, factoryDesc);
            Boolean isDynamic = factoryDesc.getDynamicPrototype();
            String counterName = BuilderUtil.replaceExt(factoryDesc.getPrototype(), ".collection", EXT_COL);
            Map.Entry<String,Boolean> entry = new AbstractMap.SimpleEntry<String, Boolean>(counterName, isDynamic);
            return entry;
        }
        return null;
    }

    public static Boolean ifStaticFactoryAddProtoAsInput(EmbeddedComponentDesc ec,
                                                         IResource genResource,
                                                         byte[] genResourceContent,
                                                         TaskBuilder taskBuilder,
                                                         IResource input) throws IOException, CompileExceptionError {
        Map.Entry<String, Boolean> info = getCounterNameAndPrototypeInfo(ec.getType(), genResource, genResourceContent);
        if (info != null) {
            Boolean isStatic = !info.getValue();
            if (isStatic) {
                taskBuilder.addInput(input.getResource(info.getKey()).output());
            }
            return isStatic;
        }
        return null;
    }

    public static Boolean ifStaticFactoryAddProtoAsInput(ComponentDesc cd, TaskBuilder taskBuilder,
        IResource input, Project project) throws IOException, CompileExceptionError {
        
        String comp = cd.getComponent();
        String type = FilenameUtils.getExtension(comp);
        if (ComponentsCounter.isFactoryType(type, false)) {
            IResource genResource = project.getResource(comp);
            Map.Entry<String, Boolean> info = getCounterNameAndPrototypeInfo(type, genResource);
            if (info != null) {
                Boolean isStatic = !info.getValue();
                if (isStatic) {
                    taskBuilder.addInput(input.getResource(info.getKey()).output());
                }
                return isStatic;
            }
        }
        return null;
    }

    public static Set<IResource> getCounterInputs(Task task) {
        List<IResource> inputs = task.getInputs();
        Set<IResource> counterInputs = new HashSet<IResource>();
        for (IResource res : inputs) {
            if (isCompCounterStorage(res.getPath())) {
                counterInputs.add(res);
            }
        }
        return counterInputs;
    }

    public static void sumInputs(Storage targetStorage, List<IResource> inputs, Integer count) throws IOException, CompileExceptionError  {
        for (IResource res :  inputs) {
            if (isCompCounterStorage(res.getPath())) {
                Storage inputStorage = Storage.load(res);
                if (inputStorage.isDynamic()) {
                    targetStorage.makeDynamic();
                }
                else {
                    targetStorage.add(inputStorage, count);
                }
            }
        }
    }

    public static void sumInputs(Storage targetStorage, List<IResource> inputs,
        Map<IResource, Integer> compCounterInputsCount) throws IOException, CompileExceptionError  {
        for (IResource res :  inputs) {
            if (isCompCounterStorage(res.getPath())) {
                Storage inputStorage = Storage.load(res);
                if (inputStorage.isDynamic()) {
                    targetStorage.makeDynamic();
                }
                else {
                    targetStorage.add(inputStorage, compCounterInputsCount.getOrDefault(res, 0));
                }
            }
        }
    }

    public static String replaceExt(String path) {
        if (path.endsWith(".go")) {
            return BuilderUtil.replaceExt(path, ".go", EXT_GO);
        } else if (path.endsWith(".collection")) {
            return BuilderUtil.replaceExt(path, ".collection", EXT_COL);
        }
        return null;
    }

    public static String replaceExt(IResource res) {
        String path = "/" + res.getPath();
        return replaceExt(path);
    }

    public static void countComponentsInEmbededObjects(Project project, IResource input, byte[] inputContent, Storage compStorage) throws IOException, CompileExceptionError {
        PrototypeDesc.Builder prot = PrototypeDesc.newBuilder();
        ProtoUtil.merge(input, inputContent, prot);

        for (EmbeddedComponentDesc cd : prot.getEmbeddedComponentsList()) {
            String type = cd.getType();
            compStorage.add(type);
            if (isFactoryType(type, false)) {
                byte[] data = cd.getData().getBytes();
                long hash = MurmurHash.hash64(data, data.length);
                IResource genResource = project.getGeneratedResource(hash, type);
                Map.Entry<String, Boolean> info = getCounterNameAndPrototypeInfo(type, genResource, data);
                if (info != null && info.getValue()) {
                    compStorage.makeDynamic();
                    return;
                }
            }
        }
        for (ComponentDesc cd : prot.getComponentsList()) {
            String comp = cd.getComponent();
            String type = FilenameUtils.getExtension(comp);
            compStorage.add(type);
            if (isFactoryType(type, false)) {
                IResource genResource = project.getResource(comp);
                Map.Entry<String, Boolean> info = getCounterNameAndPrototypeInfo(type, genResource);
                if (info != null && info.getValue()) {
                    compStorage.makeDynamic();
                    return;
                }
            }
        }
    }

    // Temporary workaround for max_instances counter (GOs) to make sure componens with bones work fine
    private static boolean hasBones(String name) {
        return name.equals("modelc") || name.equals("spinemodelc") || name.equals("rivemodelc");
    }

    public static void copyDataToBuilder(Storage storage, Project project, CollectionDesc.Builder builder) {
        //Do not copy values for collections with dynamic factories
        if (storage.isDynamic()) {
            return;
        }
        Map<String, Integer> components = storage.get();
        HashMap<String, Integer> mergedComponents = new HashMap<>();
        boolean hasDynamicValue = false;
        for (Map.Entry<String, Integer> entry : components.entrySet()) {
            // different input component names may have the same output name
            // for example wav and sound both are soundc
            String name = project.replaceExt("." + entry.getKey()).substring(1);
            Integer value = entry.getValue();
            if (mergedComponents.containsKey(name)) {
                Integer mergedValue = mergedComponents.get(name);
                if (mergedValue.equals(DYNAMIC_VALUE) || value.equals(DYNAMIC_VALUE)) {
                    mergedComponents.put(name, DYNAMIC_VALUE);
                } else {
                    mergedComponents.put(name, mergedValue + value);
                }
            } else {
                mergedComponents.put(name, value);
            }
            hasDynamicValue |= isFactoryType(name, true);
            hasDynamicValue |= hasBones(name);
        }
        if (hasDynamicValue) {
            mergedComponents.put("goc", DYNAMIC_VALUE);
        }
        for (Map.Entry<String, Integer> entry : mergedComponents.entrySet()) {
            ComponenTypeDesc.Builder componentTypeDesc = ComponenTypeDesc.newBuilder();
            componentTypeDesc.setNameHash(MurmurHash.hash64(entry.getKey())).setMaxCount(entry.getValue());
            builder.addComponentTypes(componentTypeDesc);
        }
    }
}
