package com.dynamo.bob;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.util.BobProjectProperties;

public class CopyCustomResourcesBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) throws CompileExceptionError {
        BobProjectProperties properties = new BobProjectProperties();
        try {
            properties.load(new ByteArrayInputStream(input.getContent()));
        } catch (Exception e) {
            throw new CompileExceptionError(input, -1, "Failed to parse game.project", e);
        }

        String[] resources = properties.getStringValue("project", "custom_resources", "").split(",");

        TaskBuilder<Void> b = Task.<Void>newBuilder(this)
                .setName("Copy Custom Resources");
        for (String s : resources) {
            s = s.trim();
            if (s.length() > 0) {
                IResource r = this.project.getResource(s);
                b.addInput(r);
                b.addOutput(r.output());
            }
        }
        return b.build();
    }

    @Override
    public void build(Task<Void> task) throws IOException {
        int n = task.getInputs().size();
        for (int i = 0; i < n; i++) {
            task.getOutputs().get(i).setContent(task.getInputs().get(i).getContent());
        }
    }
}
