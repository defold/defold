package com.dynamo.bob;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;

import com.dynamo.bob.Project.Walker;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.BobProjectProperties;

public class CopyCustomResourcesBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) throws CompileExceptionError {
        BobProjectProperties properties = this.project.getProjectProperties();

        String[] resources = properties.getStringArrayValue("project", "custom_resources", new String[0]);

        TaskBuilder<Void> b = Task.<Void>newBuilder(this)
                .setName("Copy Custom Resources");

        for (String s : resources) {
            s = s.trim();
            if (s.length() > 0) {
                // Could be directory or file; use findResourcePaths to traverse & grab all.
                ArrayList<String> paths = new ArrayList<String>();
                this.project.findResourcePaths(s, paths);
                for (String path : paths) {
                    IResource r = this.project.getResource(path);
                    b.addInput(r);
                    b.addOutput(r.output());
                }
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
