package com.dynamo.cr.sceneed.test;

import java.io.ByteArrayInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.jface.viewers.StructuredSelection;
import org.junit.Before;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertTrue;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.dynamo.cr.sceneed.core.INodeView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodePresenter;
import com.dynamo.cr.sceneed.gameobject.ComponentNode;
import com.dynamo.cr.sceneed.gameobject.ComponentPresenter;
import com.dynamo.cr.sceneed.gameobject.GameObjectNode;
import com.dynamo.cr.sceneed.gameobject.GameObjectPresenter;
import com.dynamo.cr.sceneed.gameobject.SpriteNode;
import com.google.inject.Module;
import com.google.inject.Singleton;

public class GameObjectTest extends AbstractTest {

    class TestModule extends GenericTestModule {
        @Override
        protected void configure() {
            super.configure();
            bind(GameObjectPresenter.class).in(Singleton.class);
        }
    }

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();
        this.manager.registerNodeType("go", GameObjectNode.class, this.injector.getInstance(GameObjectPresenter.class), null);
        this.manager.registerNodeType("sprite", SpriteNode.class, this.injector.getInstance(ComponentPresenter.class), null);
    }

    // Tests

    @Test
    public void testLoad() throws Exception {
        String ddf = "";
        GameObjectPresenter presenter = (GameObjectPresenter)this.manager.getPresenter(GameObjectNode.class);
        presenter.onLoad("go", new ByteArrayInputStream(ddf.getBytes()));
        Node root = this.model.getRoot();
        assertTrue(root instanceof GameObjectNode);
        assertTrue(this.model.getSelection().toList().contains(root));
        verify(this.view, times(1)).setRoot(root);
        verifySelection();
        verifyNoClean();
        verifyNoDirty();
    }

    @Test
    public void testAddComponent() throws Exception {
        testLoad();

        when(this.view.selectComponentType()).thenReturn("sprite");

        GameObjectNode node = (GameObjectNode)this.model.getRoot();
        GameObjectPresenter presenter = (GameObjectPresenter)this.manager.getPresenter(GameObjectNode.class);
        presenter.onAddComponent();
        assertEquals(1, node.getChildren().size());
        assertEquals("sprite", node.getChildren().get(0).toString());
        verifyUpdate(node);
        verifySelection();
        verifyDirty();

        undo();
        assertEquals(0, node.getChildren().size());
        verifyUpdate(node);
        verifySelection();
        verifyClean();

        redo();
        assertEquals(1, node.getChildren().size());
        assertEquals("sprite", node.getChildren().get(0).toString());
        verifyUpdate(node);
        verifySelection();
        verifyDirty();
    }

    @Test
    public void testAddComponentFromFile() throws Exception {
        testLoad();

        when(this.view.selectComponentFromFile()).thenReturn("/test.sprite");

        GameObjectNode node = (GameObjectNode)this.model.getRoot();
        GameObjectPresenter presenter = (GameObjectPresenter)this.manager.getPresenter(GameObjectNode.class);
        presenter.onAddComponentFromFile();
        assertEquals(1, node.getChildren().size());
        assertTrue(node.getChildren().get(0).getChildren().get(0) instanceof SpriteNode);
        verifyUpdate(node);
        verifySelection();

        undo();
        assertEquals(0, node.getChildren().size());
        verifyUpdate(node);
        verifySelection();

        redo();
        assertEquals(1, node.getChildren().size());
        assertTrue(node.getChildren().get(0).getChildren().get(0) instanceof SpriteNode);
        verifyUpdate(node);
        verifySelection();
    }

    @Test
    public void testRemoveComponent() throws Exception {
        testAddComponent();

        GameObjectNode node = (GameObjectNode)this.model.getRoot();
        ComponentNode component = (ComponentNode)node.getChildren().get(0);
        GameObjectPresenter presenter = (GameObjectPresenter)this.manager.getPresenter(GameObjectNode.class);

        presenter.onRemoveComponent();
        assertEquals(0, node.getChildren().size());
        assertEquals(null, component.getParent());
        verifyUpdate(node);
        verifySelection();

        undo();
        assertEquals(1, node.getChildren().size());
        assertEquals(component, node.getChildren().get(0));
        assertEquals(node, component.getParent());
        verifyUpdate(node);
        verifySelection();

        redo();
        assertEquals(0, node.getChildren().size());
        assertEquals(null, component.getParent());
        verifyUpdate(node);
        verifySelection();
    }

    @Test
    public void testSelection() throws Exception {
        testAddComponent();

        INodeView.Presenter presenter = this.manager.getDefaultPresenter();
        presenter.onSelect(new StructuredSelection(this.model.getRoot().getChildren().get(0)));
        verifyNoSelection();
        presenter.onSelect(new StructuredSelection(this.model.getRoot()));
        verifySelection();
    }

    @Test
    public void testSetId() throws Exception {
        testAddComponent();

        ComponentNode component = (ComponentNode)this.model.getRoot().getChildren().get(0);
        String oldId = component.getId();
        String newId = "sprite1";

        setNodeProperty(component, "id", newId);
        assertEquals(newId, component.getId());
        verifyUpdate(component);

        undo();
        assertEquals(oldId, component.getId());
        verifyUpdate(component);

        redo();
        assertEquals(newId, component.getId());
        verifyUpdate(component);
    }

    @Test
    public void testSave() throws Exception {
        testAddComponent();

        String path = "/test.go";
        IFile file = this.contentRoot.getFile(new Path(path));
        if (file.exists()) {
            file.delete(true, null);
        }
        OutputStream os = new FileOutputStream(file.getLocation().toFile());
        NodePresenter presenter = this.manager.getDefaultPresenter();
        Node root = this.model.getRoot();
        presenter.onSave(os, null);
        file = this.contentRoot.getFile(new Path(path));
        this.model.setRoot(null);
        file.refreshLocal(0, null);
        presenter.onLoad(file.getFileExtension(), file.getContents());
        assertNotSame(root, this.model.getRoot());
        assertEquals(root.getClass(), this.model.getRoot().getClass());
        assertNotSame(0, this.model.getRoot().getChildren().size());
        assertEquals(root.getChildren().size(), this.model.getRoot().getChildren().size());
    }

    @Test
    public void testComponentMessages() throws Exception {
        testAddComponent();

        ComponentNode component = (ComponentNode)this.model.getRoot().getChildren().get(0);
        assertEquals(IStatus.OK, getNodePropertyStatus(component, "id").getSeverity());

        setNodeProperty(component, "id", "");
        assertEquals(IStatus.ERROR, getNodePropertyStatus(component, "id").getSeverity());

        setNodeProperty(component, "id", "sprite");
        assertEquals(IStatus.OK, getNodePropertyStatus(component, "id").getSeverity());

        GameObjectPresenter presenter = (GameObjectPresenter)this.manager.getPresenter(GameObjectNode.class);
        presenter.onAddComponent();

        component = (ComponentNode)this.model.getRoot().getChildren().get(1);
        assertEquals(IStatus.ERROR, getNodePropertyStatus(component, "id").getSeverity());
    }

    @Override
    Module getModule() {
        return new TestModule();
    }

}
