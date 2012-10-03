package com.dynamo.cr.integrationtest;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.ByteArrayInputStream;
import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.jface.viewers.ILabelProvider;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.parted.nodes.AccelerationNode;
import com.dynamo.cr.parted.nodes.EmitterNode;
import com.dynamo.cr.parted.nodes.ParticleFXLoader;
import com.dynamo.cr.parted.nodes.ParticleFXNode;
import com.dynamo.cr.parted.nodes.ParticleFXPresenter;
import com.dynamo.cr.parted.operations.AddModifierOperation;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.Node;
import com.google.protobuf.Message;

public class ParticleTest extends AbstractSceneTest {

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();
    }

    // Tests

    @Test
    public void testLoad() throws Exception {
        INodeType nodeType = getNodeTypeRegistry().getNodeTypeClass(ParticleFXNode.class);
        getPresenter().onLoad(nodeType.getExtension(), new ByteArrayInputStream(nodeType.getResourceType().getTemplateData()));
        ParticleFXNode root = (ParticleFXNode) getModel().getRoot();
        assertTrue(root instanceof ParticleFXNode);
        assertTrue(getModel().getSelection().toList().contains(root));
        verify(getView(), times(1)).setRoot(root);

        Node emitter = getLoaderContext().loadNodeFromTemplate(EmitterNode.class);
        root.addChild(emitter);
        select(emitter);
    }

    @Test
    public void testAddModifier() throws Exception {
        testLoad();

        INodeType pfxNodeType = getNodeTypeRegistry().getNodeTypeClass(ParticleFXNode.class);
        ParticleFXPresenter presenter = (ParticleFXPresenter) pfxNodeType.getPresenter();

        INodeType[] nodeTypes = new INodeType[] { getNodeTypeRegistry().getNodeTypeClass(AccelerationNode.class) };
        int count = 1;
        for (INodeType nodeType : nodeTypes) {
            assertNotNull(nodeType.getResourceType());
            // Setup picking of the type from gui
            when(getPresenterContext().selectFromArray(anyString(), anyString(), any(Object[].class), any(ILabelProvider.class))).thenReturn(nodeType);

            // Perform operation
            presenter.onAddModifier(getPresenterContext(), getLoaderContext());
            verify(getPresenterContext(), times(count++)).executeOperation(any(AddModifierOperation.class));
        }
    }
}
