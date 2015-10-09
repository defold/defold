package com.dynamo.bob.pipeline;

import java.awt.image.BufferedImage;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Enumeration;
import java.util.List;

import javax.imageio.ImageIO;

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.Path;
import org.eclipse.swt.widgets.Display;
import org.junit.After;
import org.osgi.framework.Bundle;
import org.osgi.framework.FrameworkUtil;

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
        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }

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

    protected byte[] getFile(String file) throws IOException {
        return this.fileSystem.get(file).getContent();
    }

    protected void addFile(String file, String source) {
        addFile(file, source.getBytes());
    }

    protected void addFile(String file, byte[] content) {
        this.fileSystem.addFile(file, content);
    }

    protected void addTestFiles() {
        Bundle bundle = FrameworkUtil.getBundle(getClass());
        Enumeration<URL> entries = bundle.findEntries("/test", "*", true);
        if (entries != null) {
            while (entries.hasMoreElements()) {
                final URL url = entries.nextElement();
                IPath path = new Path(url.getPath()).removeFirstSegments(1);
                InputStream is = null;
                try {
                    is = url.openStream();
                    ByteArrayOutputStream os = new ByteArrayOutputStream();
                    IOUtils.copy(is, os);
                    String p = "/" + path.toString();
                    addFile(p, os.toByteArray());
                } catch (IOException e) {
                    throw new RuntimeException(e);
                } finally {
                    IOUtils.closeQuietly(is);
                }
            }
        }
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
