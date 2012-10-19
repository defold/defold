package com.dynamo.cr.ddfeditor.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.swt.widgets.Display;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.ddfeditor.scene.SoundLoader;
import com.dynamo.cr.ddfeditor.scene.SoundNode;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.sound.proto.Sound.SoundDesc;

public class SoundNodeTest extends AbstractNodeTest {

    private SoundNode node;
    private SoundLoader loader;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();

        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }

        this.loader = new SoundLoader();

        String sound = "sound: \"/test.ogg\" ";
        registerFile("/test.sound", sound);
        registerFile("/test.ogg", "test");
    }

    @Test
    public void testLoadSave() throws Exception {
        this.node = this.loader.load(getLoaderContext(), getFile("/test.sound").getContents());

        assertNotNull(this.node);

        assertEquals("/test.ogg", this.node.getSound());
        assertEquals(false, this.node.isLooping());

        SoundDesc msg = (SoundDesc) this.loader.buildMessage(getLoaderContext(), this.node, new NullProgressMonitor());
        assertEquals("/test.ogg", msg.getSound());
        assertEquals(0, msg.getLooping());
    }

}

