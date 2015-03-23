package com.dynamo.cr.guieditor.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashMap;

import javax.vecmath.Vector3d;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.expressions.IEvaluationContext;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.ui.ISources;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mockito;
import org.osgi.framework.Bundle;

import com.dynamo.cr.guieditor.Activator;
import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.commands.AddFont;
import com.dynamo.cr.guieditor.commands.AddGuiBoxNode;
import com.dynamo.cr.guieditor.commands.AddGuiTextNode;
import com.dynamo.cr.guieditor.commands.AddTexture;
import com.dynamo.cr.guieditor.commands.BringForward;
import com.dynamo.cr.guieditor.commands.Delete;
import com.dynamo.cr.guieditor.commands.SendBackward;
import com.dynamo.cr.guieditor.operations.DeleteGuiNodesOperation;
import com.dynamo.cr.guieditor.operations.SelectOperation;
import com.dynamo.cr.guieditor.operations.SetPropertiesOperation;
import com.dynamo.cr.guieditor.render.GuiFontResource;
import com.dynamo.cr.guieditor.render.IGuiRenderer;
import com.dynamo.cr.guieditor.scene.EditorFontDesc;
import com.dynamo.cr.guieditor.scene.EditorTextureDesc;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.GuiScene;
import com.dynamo.cr.guieditor.scene.IGuiSceneListener;
import com.dynamo.cr.guieditor.scene.TextGuiNode;
import com.dynamo.cr.properties.BeanPropertyAccessor;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.render.proto.Font.FontDesc;
import com.google.protobuf.TextFormat;

public class GuiEditorTest {

    private IGuiEditor editor;
    private ExecutionEvent executionEvent;
    private DefaultOperationHistory history;
    private UndoContext undoContext;
    private IProject project;
    private IGuiRenderer renderer;

    public static abstract class Context implements IEvaluationContext, ISelectionChangedListener {

        private ISelection selection;
        private IGuiEditor editor;

        public void init(IGuiEditor editor) {
            this.editor = editor;
            editor.getSelectionProvider().addSelectionChangedListener(this);
        }

        @Override
        public Object getVariable(String name) {
            if (name.equals(ISources.ACTIVE_EDITOR_NAME)) {
                return editor;
            } else if (name.equals(ISources.ACTIVE_CURRENT_SELECTION_NAME)) {
                return selection;
            }
            return null;
        }

        @Override
        public void selectionChanged(SelectionChangedEvent event) {
            selection = event.getSelection();
        }
    }

    void createProject() throws CoreException, IOException {
        project = ResourcesPlugin.getWorkspace().getRoot().getProject("test");
        NullProgressMonitor monitor = new NullProgressMonitor();
        if (project.exists()) {
            project.delete(true, monitor);
        }
        project.create(monitor);
        project.open(monitor);

        Bundle bundle = Platform.getBundle(Activator.PLUGIN_ID);
        Enumeration<URL> entries = bundle.findEntries("/test", "*", true);
        while (entries.hasMoreElements()) {
            URL url = entries.nextElement();
            IPath path = new Path(url.getPath()).removeFirstSegments(1);
            // Create path of url-path and remove first element, ie /test/sounds/ -> /sounds
            if (url.getFile().endsWith("/")) {
                project.getFolder(path).create(true, true, monitor);
            } else {
                InputStream is = url.openStream();
                IFile file = project.getFile(path);
                file.create(is, true, monitor);
                is.close();
            }
        }
    }

    @SuppressWarnings({ "rawtypes", "unchecked" })
    @Before
    public void setUp() throws Exception {
        createProject();

        MockGuiEditor mockEditor = mock(MockGuiEditor.class, Mockito.CALLS_REAL_METHODS);
        renderer = mock(IGuiRenderer.class);
        mockEditor.init(project, renderer);
        history = mockEditor.history;
        undoContext = mockEditor.undoContext;
        editor = mockEditor;
        HashMap params = new HashMap();
        params.put(ISources.ACTIVE_EDITOR_NAME, editor);

        Context context = mock(Context.class, Mockito.CALLS_REAL_METHODS);
        context.init(editor);

        executionEvent = new ExecutionEvent(null, params, null, context);

    }

    int nodeCount() {
        return editor.getScene().getNodeCount();
    }

    int selectCount() {
        return editor.getSelectedNodes().size();
    }

    @Test
    public void testAddGuiBoxNode() throws ExecutionException {
        AddGuiBoxNode command = new AddGuiBoxNode();
        command.execute(executionEvent);

        assertEquals(1, nodeCount());
        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(0, nodeCount());
    }

    @Test
    public void testAddGuiTextNode() throws ExecutionException {
        AddGuiTextNode command = new AddGuiTextNode();
        command.execute(executionEvent);

        assertEquals(1, nodeCount());
        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(0, nodeCount());
    }

    @Test
    public void testDeleteNodeOperation() throws ExecutionException {
        AddGuiBoxNode command = new AddGuiBoxNode();
        command.execute(executionEvent);

        command = new AddGuiBoxNode();
        command.execute(executionEvent);

        command = new AddGuiBoxNode();
        command.execute(executionEvent);

        assertEquals(3, nodeCount());
        GuiNode node1 = editor.getScene().getNode(0);
        GuiNode node2 = editor.getScene().getNode(1);
        GuiNode node3 = editor.getScene().getNode(2);

        DeleteGuiNodesOperation operation = new DeleteGuiNodesOperation(editor, Arrays.asList(node1, node2));
        editor.executeOperation(operation);
        assertEquals(1, nodeCount());

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(3, nodeCount());

        // Check that the order is preserved
        assertEquals(node1, editor.getScene().getNode(0));
        assertEquals(node2, editor.getScene().getNode(1));
        assertEquals(node3, editor.getScene().getNode(2));
    }

    @Test
    public void testSelectOperationDeleteCommand() throws ExecutionException {
        AddGuiBoxNode command = new AddGuiBoxNode();
        command.execute(executionEvent);

        assertEquals(1, nodeCount());
        GuiNode node = editor.getScene().getNode(0);

        SelectOperation operation = new SelectOperation(editor.getSelectionProvider(), new ArrayList<GuiNode>(), Arrays.asList(node));
        editor.executeOperation(operation);
        assertEquals(1, selectCount());

        Delete deleteCommand = new Delete();
        deleteCommand.execute(executionEvent);
        assertEquals(0, nodeCount());
        assertEquals(0, selectCount());

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(1, nodeCount());
        assertEquals(1, selectCount());

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(0, selectCount());
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testSetPosition() throws ExecutionException {
        AddGuiBoxNode command = new AddGuiBoxNode();
        command.execute(executionEvent);

        assertEquals(1, nodeCount());
        GuiNode node = editor.getScene().getNode(0);
        node.setPosition(new Vector3d());

        IPropertyAccessor<?, ? extends IPropertyObjectWorld> tmp = new BeanPropertyAccessor();
        IPropertyAccessor<Object, GuiScene> accessor = (IPropertyAccessor<Object, GuiScene>) tmp;
        SetPropertiesOperation<Vector3d> operation = new SetPropertiesOperation<Vector3d>(node, "position", accessor, new Vector3d(), new Vector3d(1,2,3), editor.getScene());;
        editor.executeOperation(operation);
        assertEquals(node.getPosition(), new Vector3d(1,2,3));

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(node.getPosition(), new Vector3d(0,0,0));
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testSetText() throws ExecutionException {
        AddGuiTextNode command = new AddGuiTextNode();
        command.execute(executionEvent);

        assertEquals(1, nodeCount());
        TextGuiNode node = (TextGuiNode) editor.getScene().getNode(0);
        node.setPosition(new Vector3d());

        IPropertyAccessor<?, ? extends IPropertyObjectWorld> tmp = new BeanPropertyAccessor();
        IPropertyAccessor<Object, GuiScene> accessor = (IPropertyAccessor<Object, GuiScene>) tmp;
        SetPropertiesOperation<String> operation = new SetPropertiesOperation<String>(node, "text", accessor, "", "foo", editor.getScene());;
        editor.executeOperation(operation);
        assertEquals(node.getText(), "foo");

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(node.getText(), "");
    }

    @Test
    public void testAddFont() throws Throwable {
        assertEquals(0, editor.getScene().getFonts().size());

        IFile resource = project.getFile("/fonts/highscore.font");
        assertNotNull(resource);

        AddFont addFont = new AddFont();
        addFont.doExecute(editor, resource);
        assertEquals(1, editor.getScene().getFonts().size());

        EditorFontDesc fontDesc = editor.getScene().getFonts().iterator().next();
        assertTrue(editor.getScene().getRenderResourceCollection().hasTextRenderer(fontDesc.getName()));

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(0, editor.getScene().getFonts().size());
    }

    @Test
    public void testAddTexture() throws Throwable {
        assertEquals(0, editor.getScene().getTextures().size());

        IFile resource = project.getFile("/graphics/ball.png");
        assertNotNull(resource);

        AddTexture addTexture = new AddTexture();
        addTexture.doExecute(editor, resource);
        assertEquals(1, editor.getScene().getTextures().size());

        EditorTextureDesc textureDesc = editor.getScene().getTextures().get(0);
        assertTrue(editor.getScene().getRenderResourceCollection().hasTexture(textureDesc.getName()));


        resource = project.getFile("/graphics/atlas.atlas");
        assertNotNull(resource);

        addTexture.doExecute(editor, resource);
        assertEquals(2, editor.getScene().getTextures().size());

        textureDesc = editor.getScene().getTextures().get(1);
        assertTrue(editor.getScene().getRenderResourceCollection().hasTexture(textureDesc.getName()));

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(1, editor.getScene().getTextures().size());
        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(0, editor.getScene().getTextures().size());
    }

    @Test
    public void testReloadFont() throws Throwable {
        assertEquals(0, editor.getScene().getFonts().size());

        IGuiSceneListener listener = mock(IGuiSceneListener.class);
        editor.getScene().addGuiSceneListener(listener);

        IFile resource = project.getFile("/fonts/highscore.font");
        assertNotNull(resource);

        FontDesc.Builder fontDescBuilder = FontDesc.newBuilder();
        Reader reader = new InputStreamReader(resource.getContents());
        TextFormat.merge(reader, fontDescBuilder);
        reader.close();
        int preFontSize = fontDescBuilder.getSize();

        AddFont addFont = new AddFont();
        addFont.doExecute(editor, resource);
        assertEquals(1, editor.getScene().getFonts().size());

        EditorFontDesc fontDesc = editor.getScene().getFonts().get(0);
        assertTrue(editor.getScene().getRenderResourceCollection().hasTextRenderer(fontDesc.getName()));

        // Double font size
        fontDescBuilder.setSize(preFontSize * 2);
        resource.setContents(new ByteArrayInputStream(fontDescBuilder.build().toString().getBytes()), IResource.FORCE, new NullProgressMonitor());

        // Check that the size is doubled
        GuiFontResource fontResource = editor.getScene().getFonts().get(0).getFontResource();
        assertEquals(preFontSize * 2, fontResource.getSize());

        verify(listener, times(1)).resourcesChanged();

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(0, editor.getScene().getFonts().size());

        editor.getScene().removeGuiSceneListener(listener);
    }

    @Test
    public void testReloadTexture() throws Throwable {
        assertEquals(0, editor.getScene().getTextures().size());

        IGuiSceneListener listener = mock(IGuiSceneListener.class);
        editor.getScene().addGuiSceneListener(listener);

        IFile resource = project.getFile("/graphics/ball.png");
        assertNotNull(resource);

        AddTexture addTexture = new AddTexture();
        addTexture.doExecute(editor, resource);
        assertEquals(1, editor.getScene().getTextures().size());

        EditorTextureDesc textureDesc = editor.getScene().getTextures().iterator().next();
        assertTrue(editor.getScene().getRenderResourceCollection().hasTexture(textureDesc.getName()));

        ByteArrayOutputStream byteOut = new ByteArrayOutputStream();
        IOUtils.copy(resource.getContents(), byteOut);

        // Rewrite texture
        resource.setContents(new ByteArrayInputStream(byteOut.toByteArray()), IResource.FORCE, new NullProgressMonitor());

        verify(listener, times(1)).resourcesChanged();

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(0, editor.getScene().getTextures().size());

        editor.getScene().removeGuiSceneListener(listener);
    }


    GuiNode[] setupForwardBackwardTest() throws Exception {
        AddGuiBoxNode command;

        command = new AddGuiBoxNode();
        command.execute(executionEvent);

        command = new AddGuiBoxNode();
        command.execute(executionEvent);

        command = new AddGuiBoxNode();
        command.execute(executionEvent);

        return new GuiNode[] { editor.getScene().getNode(0), editor.getScene().getNode(1), editor.getScene().getNode(2) };
    }

    void checkNodeOrder(GuiNode[] nodes, int i1, int i2, int i3) throws Exception {
        assertEquals(nodes[i1], editor.getScene().getNode(0));
        assertEquals(nodes[i2], editor.getScene().getNode(1));
        assertEquals(nodes[i3], editor.getScene().getNode(2));

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(nodes[0], editor.getScene().getNode(0));
        assertEquals(nodes[1], editor.getScene().getNode(1));
        assertEquals(nodes[2], editor.getScene().getNode(2));

        history.redo(undoContext, new NullProgressMonitor(), null);
        assertEquals(nodes[i1], editor.getScene().getNode(0));
        assertEquals(nodes[i2], editor.getScene().getNode(1));
        assertEquals(nodes[i3], editor.getScene().getNode(2));

        history.undo(undoContext, new NullProgressMonitor(), null);
        assertEquals(nodes[0], editor.getScene().getNode(0));
        assertEquals(nodes[1], editor.getScene().getNode(1));
        assertEquals(nodes[2], editor.getScene().getNode(2));
    }

    @Test
    public void testBringForward1() throws Exception {
        GuiNode[] nodes = setupForwardBackwardTest();

        SelectOperation operation = new SelectOperation(editor.getSelectionProvider(), new ArrayList<GuiNode>(), Arrays.asList(nodes[1]));
        editor.executeOperation(operation);

        BringForward bringForward = new BringForward();
        bringForward.execute(executionEvent);
        checkNodeOrder(nodes, 0, 2, 1);
    }

    @Test
    public void testBringForward2() throws Exception {
        GuiNode[] nodes = setupForwardBackwardTest();

        SelectOperation operation = new SelectOperation(editor.getSelectionProvider(), new ArrayList<GuiNode>(), Arrays.asList(nodes[0], nodes[2]));
        editor.executeOperation(operation);

        BringForward bringForward = new BringForward();
        bringForward.execute(executionEvent);
        checkNodeOrder(nodes, 1, 0, 2);
    }

    @Test
    public void testBringForward3() throws Exception {
        GuiNode[] nodes = setupForwardBackwardTest();

        SelectOperation operation = new SelectOperation(editor.getSelectionProvider(), new ArrayList<GuiNode>(), Arrays.asList(nodes[0], nodes[1]));
        editor.executeOperation(operation);

        BringForward bringForward = new BringForward();
        bringForward.execute(executionEvent);
        checkNodeOrder(nodes, 2, 0, 1);
    }

    @Test
    public void testBringForwardAll() throws Exception {
        GuiNode[] nodes = setupForwardBackwardTest();

        SelectOperation operation = new SelectOperation(editor.getSelectionProvider(), new ArrayList<GuiNode>(), Arrays.asList(nodes[1], nodes[2], nodes[0]));
        editor.executeOperation(operation);

        BringForward bringForward = new BringForward();
        bringForward.execute(executionEvent);
        checkNodeOrder(nodes, 0, 1, 2);
    }

    @Test
    public void testSendBackward1() throws Exception {
        GuiNode[] nodes = setupForwardBackwardTest();

        SelectOperation operation = new SelectOperation(editor.getSelectionProvider(), new ArrayList<GuiNode>(), Arrays.asList(nodes[1]));
        editor.executeOperation(operation);

        SendBackward sendBackward = new SendBackward();
        sendBackward.execute(executionEvent);
        checkNodeOrder(nodes, 1, 0, 2);
    }

    @Test
    public void testSendBackward2() throws Exception {
        GuiNode[] nodes = setupForwardBackwardTest();

        SelectOperation operation = new SelectOperation(editor.getSelectionProvider(), new ArrayList<GuiNode>(), Arrays.asList(nodes[0], nodes[2]));
        editor.executeOperation(operation);

        SendBackward sendBackward = new SendBackward();
        sendBackward.execute(executionEvent);
        checkNodeOrder(nodes, 0, 2, 1);
    }

    @Test
    public void testSendBackwardAll() throws Exception {
        GuiNode[] nodes = setupForwardBackwardTest();

        SelectOperation operation = new SelectOperation(editor.getSelectionProvider(), new ArrayList<GuiNode>(), Arrays.asList(nodes[0], nodes[1], nodes[2]));
        editor.executeOperation(operation);

        SendBackward sendBackward = new SendBackward();
        sendBackward.execute(executionEvent);
        checkNodeOrder(nodes, 0, 1, 2);
    }
}
