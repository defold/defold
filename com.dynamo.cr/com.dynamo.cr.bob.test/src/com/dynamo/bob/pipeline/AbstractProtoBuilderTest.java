package com.dynamo.bob.pipeline;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.junit.After;

import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.NullProgress;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.test.util.MockFileSystem;
import com.google.protobuf.Message;

public abstract class AbstractProtoBuilderTest {
    private MockFileSystem fileSystem;
    private Project project;

    @After
    public void tearDown() {
        this.project.dispose();
    }

    public AbstractProtoBuilderTest() {
        this.fileSystem = new MockFileSystem();
        this.fileSystem.setBuildDirectory("");
        this.project = new Project(this.fileSystem);

        ClassLoaderScanner scanner = new TestClassLoaderScanner();
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");
    }

    protected List<Message> build(String file, String source) throws Exception {
        addFile(file, source);
        project.setInputs(Collections.singletonList(file));
        List<TaskResult> results = project.build(new NullProgress(), "build");
        List<Message> messages = new ArrayList<Message>();
        for (TaskResult result : results) {
            if (!result.isOk()) {
                throw new CompileExceptionError(project.getResource(file), result.getLineNumber(), result.getMessage());
            }
            Task<?> task = result.getTask();
            for (IResource output : task.getOutputs()) {
                messages.add(ParseUtil.parse(output));
            }
        }
        return messages;
    }

    protected void addFile(String file, String source) {
        addFile(file, source.getBytes());
    }

    protected void addFile(String file, byte[] content) {
        this.fileSystem.addFile(file, content);
    }
}
