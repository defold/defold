package com.dynamo.bob.pipeline;

import java.io.IOException;

import com.dynamo.bob.Builder;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.test.util.MockFileSystem;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.Message;

public abstract class AbstractProtoBuilderTest {
    private MockFileSystem fileSystem;
    private Project project;

    public AbstractProtoBuilderTest() {
        this.fileSystem = new MockFileSystem();
        this.fileSystem.setBuildDirectory("");
        this.project = new Project(this.fileSystem);
    }

    protected Message build(String file, String source) throws CompileExceptionError, IOException {
        addFile(file, source);
        Builder<Void> builder = createBuilder();
        builder.setProject(this.project);
        Task<Void> task = builder.create(this.fileSystem.get(file));
        builder.build(task);
        return parseMessage(task.getOutputs().get(0).getContent());
    }

    protected abstract Builder<Void> createBuilder();
    protected abstract Message parseMessage(byte[] content) throws InvalidProtocolBufferException;

    protected void addFile(String file, String source) {
        this.fileSystem.addFile(file, source.getBytes());
    }
}
