package com.dynamo.cr.sceneed.core.test;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.sceneed.core.Activator;
import com.dynamo.cr.sceneed.core.IManipulator;
import com.dynamo.cr.sceneed.core.IManipulatorMode;
import com.dynamo.cr.sceneed.core.IManipulatorRegistry;

public class ManipulatorTest {

    private IManipulatorRegistry manipulatorRegistry;

    @Before
    public void setup() throws CoreException, IOException {
        this.manipulatorRegistry = Activator.getDefault().getManipulatorRegistry();
    }

    @Test
    public void testManipulatorSelection1() throws Exception {
        IManipulatorMode sizeMode = manipulatorRegistry.getMode("com.dynamo.cr.sceneed.core.manipulators.size-mode");
        assertNotNull(sizeMode);
        List<IDummyShape> selection = new ArrayList<IDummyShape>();
        selection.add(new DummySphere());
        IManipulator manipulator = manipulatorRegistry.getManipulatorForSelection(sizeMode, selection.toArray(new Object[selection.size()]));
        assertNotNull(manipulator);
        assertThat(manipulator, instanceOf(DummySphereSizeManipulator.class));
    }

    @Test
    public void testManipulatorSelection2() throws Exception {
        IManipulatorMode sizeMode = manipulatorRegistry.getMode("com.dynamo.cr.sceneed.core.manipulators.size-mode");
        assertNotNull(sizeMode);
        List<IDummyShape> selection = new ArrayList<IDummyShape>();
        selection.add(new DummyBox());
        IManipulator manipulator = manipulatorRegistry.getManipulatorForSelection(sizeMode, selection.toArray(new Object[selection.size()]));
        assertNull(manipulator);
    }
}
