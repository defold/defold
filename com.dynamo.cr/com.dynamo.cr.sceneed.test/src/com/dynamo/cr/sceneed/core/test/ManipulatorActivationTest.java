package com.dynamo.cr.sceneed.core.test;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;

import java.util.List;

import org.hamcrest.core.IsInstanceOf;
import org.junit.Test;

import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.IManipulatorMode;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;

public class ManipulatorActivationTest extends AbstractManipulatorTest {

    @Test
    public void testScaleActivation1() throws Exception {
        IManipulatorMode mode = getManipulatorMode(Activator.SCALE_MODE_ID);
        assertNotNull(mode);
        List<Node> selection = select(new DummySphere());
        Manipulator manipulator = getManipulatorForSelection(mode, selection.toArray(new Object[selection.size()]));
        assertNotNull(manipulator);
        assertThat(manipulator, IsInstanceOf.instanceOf(DummySphereScaleManipulator.class));
    }

    @Test
    public void testScaleActivation2() throws Exception {
        IManipulatorMode mode = getManipulatorMode(Activator.SCALE_MODE_ID);
        assertNotNull(mode);
        List<Node> selection = select(new DummyBox());
        Manipulator manipulator = getManipulatorForSelection(mode, selection.toArray(new Object[selection.size()]));
        assertNull(manipulator);
    }

}
