package com.dynamo.cr.ddfeditor.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import java.io.IOException;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.swt.widgets.Display;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.ddfeditor.scene.MeshLoader;
import com.dynamo.cr.ddfeditor.scene.MeshNode;
import com.dynamo.cr.ddfeditor.scene.ModelLoader;
import com.dynamo.cr.ddfeditor.scene.ModelNode;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.model.proto.Model.ModelDesc;

public class ModelNodeTest extends AbstractNodeTest {

    private ModelNode node;
    private ModelLoader loader;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();

        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }

        this.loader = new ModelLoader();

        String model = "name: \"test\" " +
                "mesh: \"/cube.dae\" " +
                "material: \"/test.material\"";
        registerFile("/test.model", model);
        registerFile("/cube.dae", IOUtils.toString(getClass().getResourceAsStream("cube.dae")));
        MeshNode cubeNode = new MeshLoader().load(getLoaderContext(), getFile("/cube.dae").getContents());
        registerLoadedNode("/cube.dae", cubeNode);


        String invalidDae = "name: \"test\" " +
                "mesh: \"/invalid.dae\" " +
                "material: \"/test.material\"";
        registerFile("/invalid_dae.model", invalidDae);
        registerFile("/invalid.dae", IOUtils.toString(getClass().getResourceAsStream("invalid.dae")));
        MeshNode invalidNode = new MeshLoader().load(getLoaderContext(), getFile("/invalid.dae").getContents());
        registerLoadedNode("/invalid.dae", invalidNode);
    }

    @Test
    public void testLoadSave() throws Exception {
        this.node = this.loader.load(getLoaderContext(), getFile("/test.model").getContents());

        // Force loading of mesh
        this.node.setModel(getModel());

        assertNotNull(this.node);
        Node mesh = this.node.getMeshNode();
        assertNotNull(mesh);

        assertNodeStatus(node, IStatus.OK, null);

        ModelDesc msg = (ModelDesc) this.loader.buildMessage(getLoaderContext(), this.node, new NullProgressMonitor());
        assertEquals("/cube.dae", msg.getMesh());
        assertEquals("/test.material", msg.getMaterial());
        assertEquals(0, msg.getTexturesCount());

        this.node.setTexture("foo.png");
        msg = (ModelDesc) this.loader.buildMessage(getLoaderContext(), this.node, new NullProgressMonitor());
        assertEquals(1, msg.getTexturesCount());
    }

    @Test
    public void testInvalid() throws Exception {
        this.node = this.loader.load(getLoaderContext(), getFile("/invalid_dae.model").getContents());

        // Force loading of mesh
        this.node.setModel(getModel());

        assertNotNull(this.node);
        Node mesh = this.node.getMeshNode();
        assertNotNull(mesh);

        node.updateStatus();
        assertNodeStatus(node, IStatus.ERROR, null);
    }

}

