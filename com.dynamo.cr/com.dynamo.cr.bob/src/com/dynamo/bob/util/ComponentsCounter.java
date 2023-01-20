// Copyright 2020-2023 The Defold Foundation
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
import java.util.Set;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.io.ByteArrayOutputStream;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ProtoUtil;
import com.dynamo.bob.pipeline.BuilderUtil;
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
    public static final Integer UNCOUNTABLE = 0xFFFFFFFF;

    /**
     * Class represents a storage for counted components which can be written on disk
     */
    public static class Storage implements Serializable {

        private static final long serialVersionUID = 1L;

        private Map<String, Integer> components = new HashMap<>();

        public Map<String, Integer> get() {
            return components;
        }

        public void add(String componentName, Integer count) {
            if (componentName.charAt(0) == '.') {
                componentName = componentName.substring(1);
            }
            Integer currentValue = components.getOrDefault(componentName, 0);
            System.out.println("Bob: " +" Add component " + componentName + " count "+ count);
            if (count == UNCOUNTABLE) {
                components.put(componentName, UNCOUNTABLE);
            } 
            else if (currentValue != UNCOUNTABLE) {
                components.put(componentName, components.getOrDefault(componentName, 0) + count);
            }
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
                add(entry.getKey(), count == UNCOUNTABLE ? UNCOUNTABLE : entry.getValue() * count);
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

    private static Boolean isFactoryType(String type) {
        if (type.charAt(0) == '.') {
            type = type.substring(1);
        }
        return type.equals("factory") || type.equals("collectionfactory");
    }

    private static Boolean isCompCountInput(IResource res) {
        return res.getPath().endsWith(EXT_GO) || res.getPath().endsWith(EXT_COL);
    }

    private static String getCompCounterPathFromFactoryPrototype(String type, IResource resource) throws IOException, CompileExceptionError {
        if (type.equals("factory")) {
            FactoryDesc.Builder factoryDesc = FactoryDesc.newBuilder();
            ProtoUtil.merge(resource, factoryDesc);
            return BuilderUtil.replaceExt(factoryDesc.getPrototype(), ".go", EXT_GO);
        } else if (type.equals("collectionfactory")) {
            CollectionFactoryDesc.Builder factoryDesc = CollectionFactoryDesc.newBuilder();
            ProtoUtil.merge(resource, factoryDesc);
            return BuilderUtil.replaceExt(factoryDesc.getPrototype(), ".collection", EXT_COL);
        }
        return null;
    }

    public static Boolean addCompCounterInputFromFactory(EmbeddedComponentDesc ec, IResource genResource,
        TaskBuilder taskBuilder, IResource input) throws IOException, CompileExceptionError {
        
        String compCounterPath = getCompCounterPathFromFactoryPrototype(ec.getType(), genResource);
        if (compCounterPath != null) {
            taskBuilder.addInputIfUnique(input.getResource(compCounterPath).output());
            return true;
        }
        return false;
    }

    public static Boolean addCompCounterInputFromFactory(ComponentDesc cd, TaskBuilder taskBuilder,
        IResource input, Project project) throws IOException, CompileExceptionError {
        
        String comp = cd.getComponent();
        String type = FilenameUtils.getExtension(comp);
        if (ComponentsCounter.isFactoryType(type)) {
            IResource genResource = project.getResource(comp);
            String compCounterPath = getCompCounterPathFromFactoryPrototype(type, genResource);
            if (compCounterPath != null) {
                taskBuilder.addInputIfUnique(input.getResource(compCounterPath).output());
                return true;
            }
        }
        return false;
    }

    public static Set<IResource> getCounterInputs(Task<?> task) {
        List<IResource> inputs = task.getInputs();
        Set<IResource> counterInputs = new HashSet<IResource>();
        for (IResource res : inputs) {
            if (isCompCountInput(res)) {
                counterInputs.add(res);
            }
        }
        return counterInputs;
    }

    public static void sumInputs(Storage targetStorage, List<IResource> inputs, Integer count) throws IOException, CompileExceptionError  {
        for (IResource res :  inputs) {
            if (isCompCountInput(res)) {
                targetStorage.add(Storage.load(res), count);
            }
        }
    }

    public static void sumInputs(Storage targetStorage, List<IResource> inputs,
        Map<IResource, Integer> compCounterInputsCount) throws IOException, CompileExceptionError  {
        for (IResource res :  inputs) {
            if (isCompCountInput(res)) {
                targetStorage.add(Storage.load(res), compCounterInputsCount.get(res));
            }
        }
    }

    public static String replaceExt(IResource res) {
        String path = res.getPath();
         if (path.endsWith(".go")) {
            return "/" + BuilderUtil.replaceExt(path, ".go", EXT_GO);
        } else if (path.endsWith(".collection")) {
            return "/" + BuilderUtil.replaceExt(path, ".collection", EXT_COL);
        }
        return null;
    }
}
