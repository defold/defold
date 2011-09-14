package com.dynamo.cr.tileeditor.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyFloat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URL;
import java.util.Enumeration;
import java.util.List;

import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IProjectDescription;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.junit.Before;
import org.junit.Test;
import org.osgi.framework.Bundle;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.tileeditor.core.GridModel;
import com.dynamo.cr.tileeditor.core.GridPresenter;
import com.dynamo.cr.tileeditor.core.IGridView;
import com.dynamo.tile.proto.Tile.TileGrid;
import com.dynamo.tile.proto.Tile.TileLayer;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class GridTest {

    private IGridView view;
    private GridModel model;
    private GridPresenter presenter;
    private IOperationHistory history;
    private IUndoContext undoContext;
    private IProject project;
    private NullProgressMonitor monitor;
    private IContainer contentRoot;
    IPropertyModel<GridModel, GridModel> propertyModel;

    @SuppressWarnings("unchecked")
    @Before
    public void setup() throws CoreException, IOException {
        System.setProperty("java.awt.headless", "true");

        project = ResourcesPlugin.getWorkspace().getRoot().getProject("test");
        monitor = new NullProgressMonitor();
        if (project.exists()) {
            project.delete(true, monitor);
        }
        project.create(monitor);
        project.open(monitor);

        IProjectDescription pd = project.getDescription();
        pd.setNatureIds(new String[] { "com.dynamo.cr.editor.core.crnature" });
        project.setDescription(pd, monitor);

        Bundle bundle = Platform.getBundle("com.dynamo.cr.tileeditor.test");
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
        this.contentRoot = EditorUtil.findContentRoot(this.project.getFile("game.project"));

        this.view = mock(IGridView.class);
        this.history = new DefaultOperationHistory();
        this.undoContext = new UndoContext();
        this.model = new GridModel(this.history, this.undoContext);
        this.presenter = new GridPresenter(this.model, this.view);
        propertyModel = (IPropertyModel<GridModel, GridModel>) this.model.getAdapter(IPropertyModel.class);
    }

    private TileGrid loadEmptyFile() throws IOException {
        // new file
        TileGrid.Builder tileGridBuilder = TileGrid.newBuilder()
                .setTileSet("")
                .setCellWidth(0)
                .setCellHeight(0);
        TileLayer.Builder tileLayerBuilder = TileLayer.newBuilder()
                .setId("layer1")
                .setZ(0.0f)
                .setIsVisible(1);
        tileGridBuilder.addLayers(tileLayerBuilder);
        TileGrid tileGrid = tileGridBuilder.build();
        this.presenter.load(tileGrid);
        return tileGrid;
    }

    private void newMarioTileSet(IFile file) throws IOException, CoreException {
        TileSet tileSet = TileSet.newBuilder()
                .setImage("/test/mario_tileset.png")
                .setTileWidth(16)
                .setTileHeight(16)
                .setTileSpacing(0)
                .setTileMargin(1)
                .setCollision("/test/mario_tileset.png")
                .setMaterialTag("tile")
                .addCollisionGroups("default")
                .build();

        byte[] msg = tileSet.toString().getBytes();
        ByteArrayInputStream input = new ByteArrayInputStream(msg);
        file.create(input, true, monitor);
    }

    private TileGrid loadFile(IFile file) throws IOException, CoreException {
        TileGrid.Builder tileGridBuilder = TileGrid.newBuilder();

        InputStreamReader input = new InputStreamReader(file.getContents());
        TextFormat.merge(input, tileGridBuilder);
        input.close();

        TileGrid tileGrid = tileGridBuilder.build();
        this.presenter.load(tileGrid);
        return tileGrid;
    }

    private void newResourceFile(IFile file, String extension) throws CoreException {
        IResourceTypeRegistry registry = EditorCorePlugin.getDefault().getResourceTypeRegistry();
        IResourceType resourceType = registry.getResourceTypeFromExtension(extension);
        Message templateMessage = resourceType.createTemplateMessage();
        byte[] msg = templateMessage.toString().getBytes();
        ByteArrayInputStream input = new ByteArrayInputStream(msg);
        file.create(input, true, monitor);
    }

    private void newGridFile(IFile file) throws CoreException {
        newResourceFile(file, "grid");
    }

    /**
     * Test load
     */
    @SuppressWarnings("unchecked")
    @Test
    public void testLoad() throws IOException {
        TileGrid tileGrid = loadEmptyFile();

        assertEquals(tileGrid.getTileSet(), this.model.getTileSet());
        assertEquals(tileGrid.getCellWidth(), this.model.getCellWidth(), 0.000001f);
        assertEquals(tileGrid.getCellHeight(), this.model.getCellHeight(), 0.000001f);
        assertEquals(1, this.model.getLayers().size());
        GridModel.Layer layer = this.model.getLayers().get(0);
        assertEquals("layer1", layer.getId());
        assertEquals(0.0f, layer.getZ(), 0.000001f);
        assertTrue(layer.isVisible());

        verify(this.view, times(1)).setTileSetProperty(any(String.class));
        verify(this.view, never()).setCellWidthProperty(anyFloat());
        verify(this.view, never()).setCellHeightProperty(anyFloat());
        verify(this.view, times(1)).setLayers(any(List.class));
    }

    /**
     * Test use case 2.1.1
     * @throws Exception
     */
    @Test
    public void testUseCase211() throws Exception {
        IFile marioTileSetFile = contentRoot.getFile(new Path("/mario.tileset"));
        IFile levelFile = contentRoot.getFile(new Path("/level.grid"));

        newMarioTileSet(marioTileSetFile);
        newGridFile(levelFile);

        @SuppressWarnings("unused")
        TileGrid tileGrid = loadFile(levelFile);
        this.model.executeOperation(propertyModel.setPropertyValue("tileSet", "/test/mario.tileset"));
        this.model.executeOperation(propertyModel.setPropertyValue("cellHeight", 16));
        this.model.executeOperation(propertyModel.setPropertyValue("cellWidth", 16));

        assertEquals("/test/mario.tileset", this.model.getTileSet());
        assertEquals(16, this.model.getCellWidth(), 0.0001);
        assertEquals(16, this.model.getCellHeight(), 0.0001);
    }
}
