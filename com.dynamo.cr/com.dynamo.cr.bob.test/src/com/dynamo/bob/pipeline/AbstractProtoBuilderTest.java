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

package com.dynamo.bob.pipeline;

import java.awt.image.BufferedImage;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.security.CodeSource;

import javax.imageio.ImageIO;

import org.apache.commons.io.FilenameUtils;

import org.junit.After;

import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.NullProgress;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ZipMountPoint;
import com.dynamo.bob.fs.FileSystemWalker;
import com.dynamo.bob.test.util.MockFileSystem;
import com.google.protobuf.Message;


public abstract class AbstractProtoBuilderTest {
    private MockFileSystem fileSystem;
    private Project project;
    private ZipMountPoint mp;
    private ClassLoaderScanner scanner = null;

    @After
    public void tearDown() {
        this.project.dispose();
    }

    public AbstractProtoBuilderTest() {

        this.fileSystem = new MockFileSystem();
        this.fileSystem.setBuildDirectory("");
        this.project = new Project(this.fileSystem);

        scanner = new ClassLoaderScanner();

        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");

        try {
            CodeSource src = getClass().getProtectionDomain().getCodeSource();
            String jarPath = new File(src.getLocation().toURI()).getAbsolutePath();
            this.mp = new ZipMountPoint(null, jarPath, false);
            this.mp.mount();
        } catch (Exception e) {
            // let the tests fail later on
            System.err.printf("Failed mount the .jar file");
        }
    }

    protected Class<?> getClass(String className) {
        try {
            return Class.forName(className, true, scanner.getClassLoader());
        } catch(ClassNotFoundException e) {
            System.err.printf("Class not found '%s':\n", className);
            return null;
        }
    }

    protected static Object callFunction(Object instance, String functionName) {
        try {
            Method function = instance.getClass().getDeclaredMethod(functionName);
            return function.invoke(instance);
        } catch (NoSuchMethodException e) {
            System.err.printf("No method %s.%s() found!", instance.getClass().getName(), functionName);
            e.printStackTrace(System.err);
            return null;
        } catch (Exception e) {
            e.printStackTrace(System.err);
            return null;
        }
    }

    protected static Object callStaticFunction0(Class<?> klass, String functionName) {
        try {
            Method function = klass.getDeclaredMethod(functionName);
            return function.invoke(null);
        } catch (NoSuchMethodException e) {
            System.err.printf("No method %s.%s() found!", klass.getName(), functionName);
            e.printStackTrace(System.err);
            return null;
        } catch (Exception e) {
            e.printStackTrace(System.err);
            return null;
        }
    }

    protected static Object callStaticFunction1(Class<?> klass, String functionName, Class<?> cls1, Object arg1) {
        try {
            Method function = klass.getDeclaredMethod(functionName, cls1);
            return function.invoke(null, arg1);
        } catch (NoSuchMethodException e) {
            System.err.printf("No method %s.%s() found!", klass.getName(), functionName);
            e.printStackTrace(System.err);
            return null;
        } catch (Exception e) {
            e.printStackTrace(System.err);
            return null;
        }
    }


    protected static Object getField(Class<?> klass, Object instance, String fieldName) {
        try {
            Field field = klass.getField(fieldName);
            return field.get(instance);
        } catch (NoSuchFieldException e) {
            System.err.printf("No field %s.%s found!", klass.getName(), fieldName);
            return null;
        } catch (Exception e) {
            e.printStackTrace(System.err);
            return null;
        }
    }

    protected Project GetProject() {
        return this.project;
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
                Message msg = ParseUtil.parse(output);
                if (msg != null) {
                    messages.add(msg);
                }
            }
        }
        return messages;
    }

    protected byte[] getFile(String file) throws IOException {
        return this.fileSystem.get(file).getContent();
    }

    protected void addFile(String file, String source) {
        addFile(file, source.getBytes());
    }

    protected void addFile(String file, byte[] content) {
        this.fileSystem.addFile(file, content);
    }

    class Walker extends FileSystemWalker {

        private String basePath;
        private ZipMountPoint mp;

        public Walker(ZipMountPoint mp, String basePath) {
            this.mp = mp;
            this.basePath = basePath;
            if (basePath.startsWith("/")) {
                this.basePath = basePath.substring(1, basePath.length());
            }
        }

        @Override
        public void handleFile(String path, Collection<String> results) {
            String first = path.substring(0, path.indexOf("/"));
            if (first.equals(basePath)) {
                try {
                    IResource resource = this.mp.get(path);
                    path = path.substring(basePath.length()+1);
                    addFile("/" + path, resource.getContent());
                } catch (IOException e) {
                    System.err.printf("Failed to add %s to test resources", "/" + path);
                }
            }
        }
    }

    protected void addResourceDirectory(String dir) {
        List<String> results = new ArrayList<String>(1024);
        this.mp.walk(dir, new Walker(this.mp, dir), results);
    }

    protected void addTestFiles() {
        addResourceDirectory("/test");
    }

    private BufferedImage newImage(int w, int h) {
        return new BufferedImage(w, h, BufferedImage.TYPE_4BYTE_ABGR);
    }

    protected void addImage(String path, int w, int h) throws IOException {
        BufferedImage img = newImage(w, h);
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        ImageIO.write(img, FilenameUtils.getExtension(path), baos);
        baos.flush();
        addFile(path, baos.toByteArray());
    }

}
