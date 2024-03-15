// Copyright 2020-2024 The Defold Foundation
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

package com.dynamo.bob;

import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.HashSet;
import java.util.Comparator;

import com.dynamo.bob.fs.IResource;

/**
 * Task abstraction. Contains the instance data for a {@link Builder}
 * @author Christian Murray
 *
 * @param <T> currently not used. The idea is to pass data directly. TODO: Remove?
 */
public class Task<T> {
    private String name;
    private List<IResource> inputs = new ArrayList<IResource>();
    private List<IResource> outputs = new ArrayList<IResource>();
    private List<String> extraCacheKeys = new ArrayList<String>();
    private Task<?> productOf;

    private HashSet<IResource> inputLookup = new HashSet<IResource>();

    public T data;
    private Builder<T> builder;
    private byte[] signature;
    private boolean cacheable = true;

    /**
     * Task builder for create a {@link Task}.
     * @note Not to be confused with {@link Builder}
     *
     * @param <T> currently not used. The idea is to pass data directly. TODO: Remove?
     */
    public static class TaskBuilder<T> {
        Task<T> task;

        public TaskBuilder(Builder<T> builder) {
            task = new Task<T>(builder);
        }

        public Task<T> build() {
            return task;
        }

        public TaskBuilder<T> setName(String name) {
            task.name = name;
            return this;
        }

        public TaskBuilder<T> disableCache() {
            task.cacheable = false;
            return this;
        }

        public TaskBuilder<T> addInput(IResource input) {
            if (task.inputLookup.add(input)) {
                task.inputs.add(input);
            }
            return this;
        }

        public TaskBuilder<T> addOutput(IResource output) {
            if (!output.isOutput()) {
                throw new IllegalArgumentException(String.format("Resource '%s' is not an output resource", output));
            }
            task.outputs.add(output);
            return this;
        }

        public TaskBuilder<T> addExtraCacheKey(String key) {
            task.extraCacheKeys.add(key);
            return this;
        }

        public TaskBuilder<T> setData(T data) {
            task.data = data;
            return this;
        }
    }

    public Task(Builder<T> builder) {
        this.builder = builder;
    }

    public Builder<T> getBuilder() {
        return builder;
    }

    public static <T> TaskBuilder<T> newBuilder(Builder<T> builder) {
        return new TaskBuilder<T>(builder);
    }

    public T getData() {
        return data;
    }

    public String getName() {
        return name;
    }

    @Override
    public String toString() {
        return String.format("task(%s) %s -> %s", name, getInputsString(), getOutputsString());
    }

    public List<IResource> getInputs() {
        return Collections.unmodifiableList(inputs);
    }

    public String getInputsString() {
        return inputs.toString();
    }

    public IResource input(int i) {
        return inputs.size() > i ? inputs.get(i) : null;
    }

    public List<IResource> getOutputs() {
        return Collections.unmodifiableList(outputs);
    }

    public String getOutputsString() {
        return outputs.toString();
    }

    public IResource output(int i) {
        return outputs.size() > i ? outputs.get(i) : null;
    }

    public boolean isCacheable() {
        return cacheable;
    }

    /**
     * Update a message digest with a list of resources.
     * A copy of the list of resources will be used and the copy will be sorted
     * before added to the digest. This guarantees that we get the same digest
     * each time given the same set of resources.
     * @param, digest The digest to update with the resources
     * @param resources A list of resources to add
     */
    private void updateDigestWithResources(MessageDigest digest, List<IResource> resources) throws IOException {
        List<IResource> sortedResources = new ArrayList<IResource>(resources);
        Collections.sort(sortedResources, new Comparator<IResource>() {
            @Override
            public int compare(IResource r1, IResource r2) {
                return r1.getAbsPath().compareTo(r2.getAbsPath());
            }
        });
        for (IResource r : sortedResources) {
            digest.update(r.sha1());
        }
    }

    /**
     * Update a message digest with a list of extra cache parameters.
     * @param, digest The digest to update with the resources
     * @param keys A list of keys to add
     */
    private void updateDigestWithExtraCacheKeys(MessageDigest digest, List<String> keys) throws IOException {
        if (keys.size() == 0)
        {
            return;
        }
        List<String> sortedKeys = new ArrayList<String>(keys);
        Collections.sort(sortedKeys);
        for (String r : sortedKeys) {
            digest.update(r.getBytes());
        }
    }

    public MessageDigest calculateSignatureDigest() throws IOException {
        // TODO: Checksum of builder-class byte-code. Seems to be rather difficult though..
        MessageDigest digest;
        try {
            digest = MessageDigest.getInstance("SHA1");
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }

        updateDigestWithResources(digest, inputs);
        updateDigestWithExtraCacheKeys(digest, extraCacheKeys);

        builder.signature(digest);
        return digest;
    }

    public byte[] calculateSignature() throws IOException {
        MessageDigest digest = calculateSignatureDigest();
        signature = digest.digest();
        return signature;
    }

    public void setProductOf(Task<?> task) {
        this.productOf = task;
    }

    public Task<?> getProductOf() {
        return productOf;
    }

}
