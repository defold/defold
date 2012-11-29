package com.dynamo.cr.tileeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.swt.widgets.Display;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.cr.tileeditor.scene.AtlasLoader;
import com.dynamo.cr.tileeditor.scene.AtlasNode;
import com.dynamo.cr.tileeditor.scene.Messages;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class AtlasTest extends AbstractNodeTest {

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }

        super.setup();
    }

    AtlasNode load(String content) throws CoreException, IOException {
        // We load, save and then load again in order to test save
        // with every test
        String path = "/test.atlas";
        String path2 = "/test2.atlas";
        registerFile(path, content);
        AtlasNode atlas = new AtlasLoader().load(getLoaderContext(), getFile(path).getContents());
        atlas.setModel(getModel());

        Message msg = new AtlasLoader().buildMessage(getLoaderContext(), atlas, new NullProgressMonitor());
        String content2 = TextFormat.printToString(msg);

        registerFile(path2, content2);
        AtlasNode atlas2 = new AtlasLoader().load(getLoaderContext(), getFile(path2).getContents());
        atlas2.setModel(getModel());
        atlas2.updateStatus();

        return atlas2;
    }

    @Test
    public void testLoad() throws Exception {
        AtlasNode node = load("images: \"/2x5_16_1.png\"");
        assertThat(node.getChildren().size(), is(1));
        assertNodeStatus(node, IStatus.OK, null);
    }

    @Test
    public void testLoadAnimation() throws Exception {
        AtlasNode node = load("animations: { images: \"/2x5_16_1.png\" }");
        assertThat(node.getChildren().size(), is(1));
        assertThat(node.getChildren().get(0).getChildren().size(), is(1));
        assertNodeStatus(node, IStatus.OK, null);
    }

    @Test
    public void testImageNotFound() throws Exception {
        AtlasNode node = load("images: \"/does_not_exists.png\"");
        assertThat(node.getChildren().size(), is(1));
        assertNodeStatus(node, IStatus.ERROR, Messages.Atlas_UNABLE_TO_LOAD_IMAGE);
    }

}
