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

package com.dynamo.bob.test;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.anyOf;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.matchers.JUnitMatchers.hasItem;

import java.io.IOException;
import java.security.MessageDigest;
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
import com.dynamo.bob.CommandBuilder;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.CopyBuilder;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.NullProgress;
import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.test.util.MockFileSystem;
import com.dynamo.bob.test.util.MockResource;
import com.dynamo.bob.TaskResult;

public class JBobTest {
    @BuilderParams(name = "InCopyBuilder", inExts = ".in", outExt = ".out")
    public static class InCopyBuilder extends CopyBuilder {}

    @BuilderParams(name = "InCopyBuilderMulti", inExts = ".in2", outExt = ".out")
    public static class InCopyBuilderMulti extends InCopyBuilder {}

    @BuilderParams(name = "ArcBuilder", inExts = ".proj", outExt = ".arc", createOrder = 1000)
    public static class ArcBuilder extends Builder<Void> {

        @Override
        public Task<Void> create(IResource input) throws IOException,
                CompileExceptionError {

            TaskBuilder<Void> builder = Task.<Void>newBuilder(this)
                    .setName(params.name())
                    .addInput(input)
                    .addOutput(input.changeExt(params.outExt()));

            for (Task<?> task : project.getTasks()) {
                for (IResource output : task.getOutputs()) {
                    builder.addInput(output);
                }
            }

            return builder.build();
        }

        @Override
        public void build(Task<Void> task) throws CompileExceptionError,
                IOException {

            StringBuilder sb = new StringBuilder();
            for (IResource input : task.getInputs()) {
                sb.append(new String(input.getContent()));
            }

            IResource out = task.getOutputs().get(0);
            out.setContent(sb.toString().getBytes());
        }

    }

    @BuilderParams(name = "CBuilder", inExts = ".c", outExt = ".o")
    public static class CBuilder extends CopyBuilder {
        @Override
        public void signature(MessageDigest digest) {
            digest.update(project.option("COPTIM", "").getBytes());
        }
    }

    @BuilderParams(name = "NoOutput", inExts = ".nooutput", outExt = ".nooutputc")
    public static class NoOutputBuilder extends CopyBuilder {
        @Override
        public void build(Task<Void> task) {
        }
    }

    @BuilderParams(name = "FailingBuilder", inExts = ".in_err", outExt = ".out_err")
    public static class FailingBuilder extends Builder<Void> {
        @Override
        public Task<Void> create(IResource input) {
            return defaultTask(input);
        }

        @Override
        public void build(Task<Void> task) throws CompileExceptionError {
            throw new CompileExceptionError(task.input(0), 0, "Failed to build");
        }
    }

    @BuilderParams(name = "CreateException", inExts = ".in_ce", outExt = ".out_ce")
    public static class CreateExceptionBuilder extends Builder<Void> {
        @Override
        public Task<Void> create(IResource input) {
            throw new RuntimeException("error");
        }

        @Override
        public void build(Task<Void> task) throws CompileExceptionError {
        }
    }


    @BuilderParams(name = "DynamicBuilder", inExts = ".dynamic", outExt = ".number")
    public static class DynamicBuilder extends Builder<Void> {

        @Override
        public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
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
                Task<?> numberTask = project.createTask(numberInput);
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
                throws CompileExceptionError, IOException {
            IResource input = task.input(0);
            String content = new String(input.getContent());
            String[] lst = content.split("\n");
            int i = 0;
            for (String s : lst) {
                task.output(i++).setContent(s.getBytes());
            }
        }
    }

    @BuilderParams(name = "NumberBuilder", inExts = ".number", outExt = ".numberc")
    public static class NumberBuilder extends Builder<Void> {
        @Override
        public Task<Void> create(IResource input) {
            return defaultTask(input);
        }

        @Override
        public void build(Task<Void> task) throws CompileExceptionError, IOException {
            IResource input = task.input(0);
            int number = Integer.parseInt(new String(input.getContent()));
            task.output(0).setContent(Integer.toString(number * 10).getBytes());
        }
    }

    @BuilderParams(name = "FailOnEmptyAlwaysOutput", inExts = ".foeao", outExt = ".foeaoc")
    public static class FailOnEmptyAlwaysOutputBuilder extends Builder<Void> {
        @Override
        public Task<Void> create(IResource input) {
            return defaultTask(input);
        }

        @Override
        public void build(Task<Void> task) throws CompileExceptionError, IOException {
            task.output(0).setContent(new byte[0]);
            if (task.input(0).getContent().length == 0) {
                throw new CompileExceptionError(task.input(0), 0, "Failed to build");
            }
        }
    }

    private MockFileSystem fileSystem;
    private Project project;

    @Before
    public void setUp() throws Exception {
        fileSystem = new MockFileSystem();
        project = new Project(fileSystem);
        project.scan(new ClassLoaderScanner(), "com.dynamo.bob.test");
    }

    List<TaskResult> build() throws IOException, CompileExceptionError, MultipleCompileException {
        return project.build(new NullProgress(), "build");
    }

    @After
    public void tearDown() throws Exception {
        this.project.dispose();
    }

    @Test
    public void testFilePackageScan() throws Exception {
        Set<String> classes = new ClassLoaderScanner().scan("com.dynamo.bob.test");
        assertThat(classes, hasItem("com.dynamo.bob.test.JBobTest"));
    }

    @Test
    public void testCopy() throws Exception {
        fileSystem.addFile("test.in", "test data".getBytes());
        project.setInputs(Arrays.asList("test.in"));
        List<TaskResult> result = build();
        assertThat(result.size(), is(1));
        IResource testOut = fileSystem.get("test.out").output();
        assertNotNull(testOut);
        assertThat(new String(testOut.getContent()), is("test data"));
    }

    @Test(expected=CompileExceptionError.class)
    public void testTaskOutputMultipleInput() throws Exception {
        fileSystem.addFile("test.in", "test data".getBytes());
        fileSystem.addFile("test.in2", "test data 2".getBytes());
        project.setInputs(Arrays.asList("test.in", "test.in2"));
        build();
    }

    @Test
    public void testTaskOutputAsInput() throws Exception {
        fileSystem.addFile("test.proj", "".getBytes());
        fileSystem.addFile("test1.in", "A".getBytes());
        fileSystem.addFile("test2.in", "B".getBytes());
        project.setInputs(Arrays.asList("test.proj", "test1.in", "test2.in"));
        List<TaskResult> result = build();
        assertThat(result.size(), is(3));
        IResource test1Out = fileSystem.get("test1.out").output();
        assertThat(new String(test1Out.getContent()), is("A"));
        IResource test2Out = fileSystem.get("test2.out").output();
        assertThat(new String(test2Out.getContent()), is("B"));
        IResource arcOut = fileSystem.get("test.arc").output();
        assertThat(new String(arcOut.getContent()), anyOf(is("AB"), is("BA")));
    }

    @Test
    public void testTaskOutputAsInputFailing() throws Exception {
        fileSystem.addFile("test.proj", "".getBytes());
        fileSystem.addFile("test1.in_err", "A".getBytes());
        project.setInputs(Arrays.asList("test.proj", "test1.in_err"));
        List<TaskResult> result = build();
        assertThat(result.size(), is(1));
        assertThat(result.get(0).isOk(), is(false));
    }

    @Test
    public void testAbsPath() throws Exception {
        fileSystem.addFile("/root/test.in", "test data".getBytes());
        project.setInputs(Arrays.asList("/root/test.in"));
        List<TaskResult> result = build();
        assertThat(result.size(), is(1));
        IResource testOut = fileSystem.get("/root/test.out").output();
        assertThat(testOut.exists(), is(true));
        assertThat(new String(testOut.getContent()), is("test data"));
    }

    @Test
    public void testChangeInput() throws Exception {
        fileSystem.addFile("test.in", "test data".getBytes());
        project.setInputs(Arrays.asList("test.in"));
        List<TaskResult> result;

        // build
        result = build();
        assertThat(result.size(), is(1));

        // rebuild with same input
        result = build();
        assertThat(result.size(), is(0));

        // rebuild with new input
        MockResource testIn = (MockResource) fileSystem.get("test.in");
        testIn.setContent("test data prim".getBytes());
        result = build();
        assertThat(result.size(), is(1));
    }

    @Test
    public void testRemoveOutput() throws Exception {
        fileSystem.addFile("test.in", "test data".getBytes());
        project.setInputs(Arrays.asList("test.in"));
        List<TaskResult> result;

        // build
        result = build();
        assertThat(result.size(), is(1));

        // remove output
        fileSystem.get("test.out").output().remove();

        // rebuild
        result = build();
        assertThat(result.size(), is(1));
    }

    @Test
    public void testRemoveGeneratedOutput() throws Exception {
        fileSystem.addFile("test.dynamic", "1\n2\n".getBytes());
        project.setInputs(Arrays.asList("test.dynamic"));

        // build
        List<TaskResult> result = build();
        assertThat(result.size(), is(3));

        // remove generated output, ie input to another task
        fileSystem.get("test_0.numberc").output().remove();

        // rebuild
        result = build();
        assertThat(result.size(), is(1));
    }

    @Test
    public void testCompileError() throws Exception {
        fileSystem.addFile("test.in_err", "test data_err".getBytes());
        project.setInputs(Arrays.asList("test.in_err"));
        List<TaskResult> result;

        // build
        result = build();
        assertThat(result.size(), is(1));
        assertFalse(result.get(0).isOk());

        // build again
        result = build();
        assertThat(result.size(), is(1));
        assertFalse(result.get(0).isOk());
    }

    @Test(expected=CompileExceptionError.class)
    public void testCreateError() throws Exception {
        fileSystem.addFile("test.in_ce", "test".getBytes());
        project.setInputs(Arrays.asList("test.in_ce"));
        // build
        build();
    }

    @Test
    public void testMissingOutput() throws Exception {
        fileSystem.addFile("test.nooutput", "test data".getBytes());
        project.setInputs(Arrays.asList("test.nooutput"));
        List<TaskResult> result = build();
        assertThat(result.size(), is(1));
        assertFalse(result.get(0).isOk());
    }

    String getResourceString(String name) throws IOException {
        return new String(fileSystem.get(name).output().getContent());
    }

    @SuppressWarnings("rawtypes")
    @Test
    public void testDynamic() throws Exception {
        fileSystem.addFile("test.dynamic", "1\n2\n".getBytes());
        project.setInputs(Arrays.asList("test.dynamic"));
        List<TaskResult> result = build();
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
        result = build();
        assertThat(result.size(), is(1));

        // rebuild with new option
        project.setOption("COPTIM", "-O2");
        result = build();
        assertThat(result.size(), is(1));

        // rebuild with same option
        result = build();
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

    @Test
    public void testCompileErrorOutputCreated() throws Exception {
        fileSystem.addFile("test.foeao", "test".getBytes());
        project.setInputs(Arrays.asList("test.foeao"));
        List<TaskResult> result;

        // build
        result = build();
        assertThat(result.size(), is(1));
        assertTrue(result.get(0).isOk());

        fileSystem.addFile("test.foeao", "".getBytes());

        // fail build
        result = build();
        assertThat(result.size(), is(1));
        assertFalse(result.get(0).isOk());

        fileSystem.addFile("test.foeao", "test".getBytes());

        // remedy build
        result = build();
        assertThat(result.size(), is(1));
        assertTrue(result.get(0).isOk());
    }
}
