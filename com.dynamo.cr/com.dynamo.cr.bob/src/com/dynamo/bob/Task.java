package com.dynamo.bob;

import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

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
    private List<IResource> dependencies = new ArrayList<IResource>();
    private Map<String, String> options = new HashMap<String, String>();
    private Task<?> productOf;

    public T data;
    private Builder<T> builder;
    private byte[] signature;

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

        public TaskBuilder<T> addInput(IResource input) {
            task.inputs.add(input);
            return this;
        }

        public TaskBuilder<T> addOutput(IResource output) {
            if (!output.isOutput()) {
                throw new IllegalArgumentException(String.format("Resource '%s' is not an output resource", output));
            }
            task.outputs.add(output);
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

    @Override
    public String toString() {
        return String.format("task(%s) %s -> %s", name, inputs.toString(), outputs.toString());
    }

    public List<IResource> getInputs() {
        return Collections.unmodifiableList(inputs);
    }

    public IResource input(int i) {
        return inputs.get(i);
    }

    public List<IResource> getOutputs() {
        return Collections.unmodifiableList(outputs);
    }

    public IResource output(int i) {
        return outputs.get(i);
    }

    public byte[] calculateSignature(Project project) throws IOException {
        // TODO: Checksum of builder-class byte-code. Seems to be rather difficult though..
        MessageDigest digest;
        try {
            digest = MessageDigest.getInstance("SHA1");
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }

        for (IResource r : inputs) {
            digest.update(r.sha1());
        }

        for (IResource r : dependencies) {
            digest.update(r.sha1());
        }

        builder.signature(digest);
        digest.update(options.toString().getBytes());

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