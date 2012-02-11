package com.dynamo.bob.test;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.matchers.JUnitMatchers.hasItem;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.lang3.StringUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.ClassScanner;
import com.dynamo.bob.CommandBuilder;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.CopyBuilder;
import com.dynamo.bob.IFileSystem;
import com.dynamo.bob.IResource;
import com.dynamo.bob.Project;
import com.dynamo.bob.ResourceUtil;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.TaskResult;

public class JBobTest {
    @BuilderParams(name = "InCopyBuilder", inExt = ".in", outExt = ".out")
    public static class InCopyBuilder extends CopyBuilder {}

    @BuilderParams(name = "CBuilder", inExt = ".c", outExt = ".o")
    public static class CBuilder extends CopyBuilder {
        @Override
        public void signature(MessageDigest digest) {
            digest.update(project.option("COPTIM", "").getBytes());
        }
    }

    @BuilderParams(name = "NoOutput", inExt = ".nooutput", outExt = ".nooutputc")
    public static class NoOutputBuilder extends CopyBuilder {
        @Override
        public void build(Task<Void> task) {
        }
    }

    @BuilderParams(name = "FailingBuilder", inExt = ".in_err", outExt = ".out_err")
    public static class FailingBuilder extends Builder<Void> {
        @Override
        public Task<Void> create(IResource input) {
            return defaultTask(input);
        }

        @Override
        public void build(Task<Void> task) throws CompileExceptionError {
            throw new CompileExceptionError("Failed to build", 5);
        }
    }

    @BuilderParams(name = "DynamicBuilder", inExt = ".dynamic", outExt = ".number")
    public static class DynamicBuilder extends Builder<Void> {

        @Override
        public Task<Void> create(IResource input) {
            TaskBuilder<Void> builder = Task.<Void>newBuilder(this)
                    .setName(params.name())
                    .addInput(input);

            String content = new String(input.getContent());
            String[] lst = content.split("\n");
            String baseName = FilenameUtils.removeExtension(input.getPath());
            int i = 0;
            List<Task<?>> numberTasks = new ArrayList<Task<?>>();
            for (@SuppressWarnings("unused") String s : lst) {
                String numberName = String.format("%s_%d.number", baseName, i);
                IResource numberInput = input.getResource(numberName).output();
                builder.addOutput(numberInput);
                Task<?> numberTask = project.buildResource(numberInput);
                numberTasks.add(numberTask);
                ++i;
            }
            Task<Void> t = builder.build();
            for (Task<?> task : numberTasks) {
                task.setProductOf(t);
            }
            return t;
        }

        @Override
        public void build(Task<Void> task)
                throws CompileExceptionError {
            IResource input = task.input(0);
            String content = new String(input.getContent());
            String[] lst = content.split("\n");
            int i = 0;
            for (String s : lst) {
                task.output(i++).setContent(s.getBytes());
            }
        }
    }

    @BuilderParams(name = "NumberBuilder", inExt = ".number", outExt = ".numberc")
    public static class NumberBuilder extends Builder<Void> {
        @Override
        public Task<Void> create(IResource input) {
            return defaultTask(input);
        }

        @Override
        public void build(Task<Void> task) throws CompileExceptionError {
            IResource input = task.input(0);
            int number = Integer.parseInt(new String(input.getContent()));
            task.output(0).setContent(Integer.toString(number * 10).getBytes());
        }
    }

    class MockResource implements IResource {

        private MockFileSystem fileSystem;
        private String path;
        private byte[] content;

        MockResource(MockFileSystem fileSystem, String path, byte[] content) {
            this.fileSystem = fileSystem;
            this.path = path;
            this.content = content;
        }

        @Override
        public boolean isOutput() {
            return path.startsWith(fileSystem.buildDirectory + "/")
            || path.startsWith(fileSystem.buildDirectory + "\\");
        }

        @Override
        public IResource changeExt(String ext) {
            String newName = ResourceUtil.changeExt(path, ext);
            MockResource newResource = (MockResource) fileSystem.get(newName);
            return newResource.output();
        }

        @Override
        public byte[] getContent() {
            return content;
        }

        @Override
        public void setContent(byte[] content) {
            if (!isOutput()) {
                throw new IllegalArgumentException(String.format("Resource '%s' is not an output resource", this.toString()));
            }
            this.content = Arrays.copyOf(content, content.length);
        }

        // Only for testing
        public void forceSetContent(byte[] content) {
            this.content = Arrays.copyOf(content, content.length);
        }

        @Override
        public byte[] sha1() {
            if (content == null) {
                throw new IllegalArgumentException(String.format("Resource '%s' is not created", path));
            }
            MessageDigest sha1;
            try {
                sha1 = MessageDigest.getInstance("SHA1");
            } catch (NoSuchAlgorithmException e) {
                throw new RuntimeException(e);
            }
            sha1.update(content);
            return sha1.digest();
        }

        @Override
        public boolean exists() {
            return content != null;
        }

        @Override
        public String getPath() {
            return path;
        }

        @Override
        public void remove() {
            content = null;
        }

        @Override
        public IResource getResource(String name) {
            String basePath = FilenameUtils.getPath(this.path);
            String fullPath = FilenameUtils.concat(basePath, name);
            return this.fileSystem.get(fullPath);
        }

        @Override
        public IResource output() {
            if (isOutput()) {
                return this;
            } else {
                String p = FilenameUtils.concat(this.fileSystem.buildDirectory, this.path);
                return fileSystem.get(p);
            }
        }

        @Override
        public String toString() {
            return path;
        }

    }

    class MockFileSystem implements IFileSystem {
        Map<String, MockResource> files = new HashMap<String, MockResource>();
        private String buildDirectory;

        @Override
        public void setBuildDirectory(String buildDirectory) {
            this.buildDirectory = buildDirectory;
        }

        public void addFile(String name, byte[] content) {
            name = FilenameUtils.normalize(name, true);
            files.put(name, new MockResource(this, name, content));
        }

        public void addFile(MockResource resource) {
            this.files.put(resource.path, resource);
        }

        @Override
        public IResource get(String name) {
            name = FilenameUtils.normalize(name, true);
            IResource r = files.get(name);
            if (r == null) {
                r = new MockResource(fileSystem, name, null);
                files.put(name, (MockResource) r);
            }
            return r;
        }
    }

    private MockFileSystem fileSystem;
    private Project project;

    @Before
    public void setUp() throws Exception {
        fileSystem = new MockFileSystem();;
        project = new Project(fileSystem);
        project.scanPackage("com.dynamo.bob.test");
    }

    @After
    public void tearDown() throws Exception {
    }

    @Test
    public void testFilePackageScan() throws Exception {
        Set<String> classes = ClassScanner.scan("com.dynamo.bob.test");
        assertThat(classes, hasItem("com.dynamo.bob.test.JBobTest"));
    }

    @Test
    public void testCopy() throws Exception {
        fileSystem.addFile("test.in", "test data".getBytes());
        project.setInputs(Arrays.asList("test.in"));
        List<TaskResult> result = project.build();
        assertThat(result.size(), is(1));
        IResource testOut = fileSystem.get("test.out").output();
        assertNotNull(testOut);
        assertThat(new String(testOut.getContent()), is("test data"));
    }

    @Test
    public void testChangeInput() throws Exception {
        fileSystem.addFile("test.in", "test data".getBytes());
        project.setInputs(Arrays.asList("test.in"));
        List<TaskResult> result;

        // build
        result = project.build();
        assertThat(result.size(), is(1));

        // rebuild with same input
        result = project.build();
        assertThat(result.size(), is(0));

        // rebuild with new input
        MockResource testIn = (MockResource) fileSystem.get("test.in");
        testIn.forceSetContent("test data prim".getBytes());
        result = project.build();
        assertThat(result.size(), is(1));
    }

    @Test
    public void testRemoveOutput() throws Exception {
        fileSystem.addFile("test.in", "test data".getBytes());
        project.setInputs(Arrays.asList("test.in"));
        List<TaskResult> result;

        // build
        result = project.build();
        assertThat(result.size(), is(1));

        // remove output
        fileSystem.get("test.out").output().remove();

        // rebuild
        result = project.build();
        assertThat(result.size(), is(1));
    }

    @Test
    public void testCompileError() throws Exception {
        fileSystem.addFile("test.in_err", "test data_err".getBytes());
        project.setInputs(Arrays.asList("test.in_err"));
        List<TaskResult> result;

        // build
        result = project.build();
        assertThat(result.size(), is(1));
        assertThat(result.get(0).getReturnCode(), is(5));

        // build again
        result = project.build();
        assertThat(result.size(), is(1));
        assertThat(result.get(0).getReturnCode(), is(5));
    }

    @Test
    public void testMissingOutput() throws Exception {
        fileSystem.addFile("test.nooutput", "test data".getBytes());
        project.setInputs(Arrays.asList("test.nooutput"));
        List<TaskResult> result = project.build();
        assertThat(result.size(), is(1));
        assertThat(result.get(0).getReturnCode(), not(is(0)));
    }

    String getResourceString(String name) {
        return new String(fileSystem.get(name).output().getContent());
    }

    @SuppressWarnings("rawtypes")
    @Test
    public void testDynamic() throws Exception {
        fileSystem.addFile("test.dynamic", "1\n2\n".getBytes());
        project.setInputs(Arrays.asList("test.dynamic"));
        List<TaskResult> result = project.build();
        assertThat(result.size(), is(3));
        assertThat(getResourceString("test_0.numberc"), is("10"));
        assertThat(getResourceString("test_1.numberc"), is("20"));
        assertThat(result.get(1).getTask().getProductOf(), is((Task) result.get(0).getTask()));
        assertThat(result.get(2).getTask().getProductOf(), is((Task) result.get(0).getTask()));
    }

    @Test
    public void testChangeOptions() throws Exception {
        fileSystem.addFile("test.c", "f();".getBytes());
        project.setInputs(Arrays.asList("test.c"));
        List<TaskResult> result;

        // build
        result = project.build();
        assertThat(result.size(), is(1));

        // rebuild with new option
        project.setOption("COPTIM", "-O2");
        result = project.build();
        assertThat(result.size(), is(1));

        // rebuild with same option
        result = project.build();
        assertThat(result.size(), is(0));
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testCommandSubstitute1() throws Exception {
        Map<String, Object> p1 = new HashMap<String, Object>();
        p1.put("CC", "gcc");
        p1.put("COPTIM", "-O2");

        Map<String, Object> p2 = new HashMap<String, Object>();
        p2.put("INPUTS", Arrays.asList("a.c", "b.c"));
        p2.put("OUTPUTS", Arrays.asList("x.o"));
        p2.put("COPTIM", "-O0");

        List<String> lst = CommandBuilder.substitute("${CC} ${COPTIM} -c ${INPUTS} -o ${OUTPUTS[0]}", p1, p2);
        assertThat("gcc -O0 -c a.c b.c -o x.o", is(StringUtils.join(lst, " ")));

        List<String> lst2 = CommandBuilder.substitute("${CC} ${COPTIM} -c ${INPUTS[1]} -o ${OUTPUTS[0]}", p1, p2);
        assertThat("gcc -O0 -c b.c -o x.o", is(StringUtils.join(lst2, " ")));
    }

}

