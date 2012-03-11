package com.dynamo.cr.sceneed.core.test;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.io.IOException;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.CoreException;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeRegistry;

public class NodeTest extends AbstractNodeTest {

    private DummyNode node;
    private DummyNodeLoader loader;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();

        this.loader = new DummyNodeLoader();

        String extension = "dummy";

        // Mock as if dummy was a registered resource type
        IResourceType resourceType = mock(IResourceType.class);
        when(resourceType.getTemplateData()).thenReturn(new byte[0]);

        IResourceTypeRegistry registry = mock(IResourceTypeRegistry.class);
        when(registry.getResourceTypeFromExtension(extension)).thenReturn(resourceType);

        setResourceTypeRegistry(registry);

        this.node = registerAndLoadRoot(DummyNode.class, extension, this.loader);
    }

    @Test
    public void propertyTest() throws ExecutionException {
        setNodeProperty(this.node, "dummyProperty", 1);
    }
}
