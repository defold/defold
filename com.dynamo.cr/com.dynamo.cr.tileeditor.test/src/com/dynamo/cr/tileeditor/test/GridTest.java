package com.dynamo.cr.tileeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.eq;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyList;
import static org.mockito.Matchers.anyListOf;
import static org.mockito.Matchers.anyLong;
import static org.mockito.Matchers.anyMap;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.StringReader;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Enumeration;
import java.util.List;

import javax.inject.Singleton;
import javax.vecmath.Point2f;
import javax.vecmath.Vector2f;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IProjectDescription;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
import org.eclipse.core.resources.IWorkspace;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.Rectangle;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.osgi.framework.Bundle;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.tileeditor.core.GridModel;
import com.dynamo.cr.tileeditor.core.GridPresenter;
import com.dynamo.cr.tileeditor.core.IGridView;
import com.dynamo.cr.tileeditor.core.Layer;
import com.dynamo.cr.tileeditor.core.Layer.Cell;
import com.dynamo.cr.tileeditor.core.MapBrush;
import com.dynamo.cr.tileeditor.core.Messages;
import com.dynamo.cr.tileeditor.core.TileSetModel;
import com.dynamo.tile.proto.Tile.TileCell;
import com.dynamo.tile.proto.Tile.TileGrid;
import com.dynamo.tile.proto.Tile.TileGrid.BlendMode;
import com.dynamo.tile.proto.Tile.TileLayer;
import com.google.common.base.Joiner;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.protobuf.TextFormat;

public class GridTest implements IResourceChangeListener {

    private IGridView view;
    private IGridView.Presenter presenter;
    private GridModel model;
    private IOperationHistory history;
    private IUndoContext undoContext;
    private IProject project;
    private IContainer contentRoot;
    private IPropertyModel<GridModel, GridModel> propertyModel;
    private Injector injector;

    class TestModule extends AbstractModule {
        @Override
        protected void configure() {
            bind(IGridView.class).toInstance(view);
            bind(IGridView.Presenter.class).to(GridPresenter.class).in(Singleton.class);
            bind(GridModel.class).in(Singleton.class);

            bind(IOperationHistory.class).to(DefaultOperationHistory.class).in(Singleton.class);
            bind(IUndoContext.class).to(UndoContext.class).in(Singleton.class);

            bind(IContainer.class).toInstance(contentRoot);
        }
    }

    @SuppressWarnings("unchecked")
    @Before
    public void setup() throws CoreException, IOException {
        System.setProperty("java.awt.headless", "true");

        IWorkspace workspace = ResourcesPlugin.getWorkspace();
        this.project = workspace.getRoot().getProject("test");
        if (this.project.exists()) {
            this.project.delete(true, null);
        }
        this.project.create(null);
        this.project.open(null);

        IProjectDescription pd = this.project.getDescription();
        pd.setNatureIds(new String[] { "com.dynamo.cr.editor.core.crnature" });
        this.project.setDescription(pd, null);

        Bundle bundle = Platform.getBundle("com.dynamo.cr.tileeditor.test");
        List<URL> entries = Collections.list(bundle.findEntries("/test", "*", true));
        Collections.sort(entries, new Comparator<URL>() {
            @Override
            public int compare(URL o1, URL o2) {
                return o1.toString().compareTo(o2.toString());
            }
        });
        for (URL url : entries) {
            IPath path = new Path(url.getPath()).removeFirstSegments(1);
            // Create path of url-path and remove first element, ie /test/sounds/ -> /sounds
            if (url.getFile().endsWith("/")) {
                this.project.getFolder(path).create(true, true, null);
            } else {
                InputStream is = url.openStream();
                IFile file = this.project.getFile(path);
                file.create(is, true, null);
                is.close();
            }
        }
        this.contentRoot = EditorUtil.findContentRoot(this.project.getFile("game.project"));

        this.view = mock(IGridView.class);
        TestModule module = new TestModule();
        this.injector = Guice.createInjector(module);
        this.history = this.injector.getInstance(IOperationHistory.class);
        this.undoContext = this.injector.getInstance(IUndoContext.class);
        this.model = this.injector.getInstance(GridModel.class);
        this.presenter = this.injector.getInstance(IGridView.Presenter.class);
        this.propertyModel = (IPropertyModel<GridModel, GridModel>) this.model.getAdapter(IPropertyModel.class);

        workspace.addResourceChangeListener(this);

        when(this.view.getPreviewRect()).thenAnswer(new Answer<Rectangle>() {
            @Override
            public Rectangle answer(InvocationOnMock invocation) throws Throwable {
                return new Rectangle(0, 0, 1, 1);
            }
        });
    }

    @After
    public void teardown() {
        IWorkspace workspace = ResourcesPlugin.getWorkspace();
        workspace.removeResourceChangeListener(this);
    }

    // Helpers

    private void undo() throws ExecutionException {
        this.history.undo(this.undoContext, null, null);
    }

    private void redo() throws ExecutionException {
        this.history.redo(this.undoContext, null, null);
    }

    private void newResourceFile(String path) throws IOException, CoreException {
        IResourceTypeRegistry registry = EditorCorePlugin.getDefault().getResourceTypeRegistry();
        IResourceType type = registry.getResourceTypeFromExtension(path.substring(path.lastIndexOf('.') + 1));
        IFile file = this.contentRoot.getFile(new Path(path));
        InputStream is = new ByteArrayInputStream(type.getTemplateData());
        try {
            if (file.exists()) {
                file.setContents(is, true, true, null);
            } else {
                file.create(is, true, null);
            }
        } finally {
            is.close();
        }
    }

    private void newGrid() throws IOException, CoreException {
        String path = "/test.tilegrid";
        newResourceFile(path);
        IFile file = this.contentRoot.getFile(new Path(path));
        this.presenter.onLoad(file.getContents());
    }

    private TileSetModel newTileSet(String path, String image, int tileSpacing) throws IOException, CoreException {
        newResourceFile(path);
        IFile file = this.contentRoot.getFile(new Path(path));
        TileSetModel tileSetModel = new TileSetModel(this.contentRoot, null, null);
        tileSetModel.load(file.getContents());
        tileSetModel.setImage(image);
        tileSetModel.setTileSpacing(tileSpacing);
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        tileSetModel.save(stream, null);
        file.setContents(new ByteArrayInputStream(stream.toByteArray()), false, true, null);
        return tileSetModel;
    }

    private TileSetModel newMarioTileSet() throws IOException, CoreException {
        return newTileSet("/mario.tileset", "/mario_tileset.png", 1);
    }

    private void setProperty(IPropertyModel<?,?> propertyModel, Object id, Object value) {
        this.model.executeOperation(propertyModel.setPropertyValue(id, value));
    }

    private void setGridProperty(Object id, Object value) throws ExecutionException {
        setProperty(this.propertyModel, id, value);
    }

    @SuppressWarnings("unchecked")
    private void setTileSetProperty(TileSetModel model, Object id, Object value) {
        setProperty((IPropertyModel<TileSetModel, TileSetModel>)model.getAdapter(IPropertyModel.class), id, value);
    }

    @SuppressWarnings("unchecked")
    private void setLayerProperty(Layer layer, Object id, Object value) {
        setProperty((IPropertyModel<Layer, GridModel>)layer.getAdapter(IPropertyModel.class), id, value);
    }

    private String tileSet() {
        return this.model.getTileSource();
    }

    private int cellTile(Layer layer, int x, int y) {
        Cell cell = layer.getCell(x, y);
        if (cell != null) {
            return cell.getTile();
        } else {
            return -1;
        }
    }

    /**
     * Test load
     */
    @SuppressWarnings("unchecked")
    @Test
    public void testNewGrid() throws Exception {
        newGrid();

        assertEquals("", this.model.getTileSource());
        assertEquals(1, this.model.getLayers().size());
        Layer layer = this.model.getLayers().get(0);
        assertEquals("layer1", layer.getId());
        assertEquals(0.0f, layer.getZ(), 0.000001f);
        assertTrue(layer.isVisible());

        verify(this.view, never()).setTileSet(any(BufferedImage.class), anyInt(), anyInt(), anyInt(), anyInt());
        verify(this.view, times(1)).setLayers(anyList());
        verify(this.view, times(1)).setSelectedLayer(any(Layer.class));
        verify(this.view, never()).setCell(anyInt(), anyLong(), any(Cell.class));
        verify(this.view, times(1)).refreshProperties();
        verify(this.view, times(1)).setValidModel(eq(true));
        verify(this.view, never()).setDirty(anyBoolean());
        verify(this.view, times(1)).setPreview(eq(new Point2f(0.0f, 0.0f)), eq(1.0f));
    }

    /**
     * Use Case 2.1.1 - Create a Grid
     * @throws Exception
     */
    @Test
    public void testUseCase211() throws Exception {
        newMarioTileSet();
        newGrid();

        verify(this.view, times(1)).setValidModel(eq(true));

        setGridProperty("tileSource", "/mario.tileset");

        assertThat(tileSet(), is("/mario.tileset"));
        verify(this.view, times(1)).setTileSet(any(BufferedImage.class), eq(16), eq(16), eq(0), eq(1));

        undo();
        assertThat(tileSet(), is(""));
        verify(this.view, times(1)).setTileSet((BufferedImage)eq(null), eq(0), eq(0), eq(0), eq(0));

        redo();
        assertThat(tileSet(), is("/mario.tileset"));
        verify(this.view, times(2)).setTileSet(any(BufferedImage.class), eq(16), eq(16), eq(0), eq(1));
    }

    /**
     * Use Case 2.2.1 - Paint Tiles Into Cells
     * @throws Exception
     *
     * TODO: Not complete according to specs
     */
    @SuppressWarnings("unchecked")
    @Test
    public void testUseCase221() throws Exception {
        testUseCase211();

        List<Layer> layers = this.model.getLayers();
        Layer layer = layers.get(0);

        // Paint

        this.presenter.onSelectLayer(layer);

        this.presenter.onSelectTile(1, false, false);
        verify(this.view, times(1)).setBrush(any(MapBrush.class));

        this.presenter.onPaintBegin();

        this.presenter.onPaint(0,1);
        this.presenter.onPaint(0,1);
        assertThat(cellTile(layer, 0, 1), is(1));
        long cellIndex = Layer.toCellIndex(0, 1);
        verify(this.view, times(2)).setCell(eq(0), eq(cellIndex), any(Cell.class));

        this.presenter.onPaint(1,1);
        this.presenter.onPaint(1,1);
        assertThat(cellTile(layer, 1, 1), is(1));
        cellIndex = Layer.toCellIndex(1, 1);
        verify(this.view, times(2)).setCell(eq(0), eq(cellIndex), any(Cell.class));

        this.presenter.onPaintEnd();

        verify(this.view, never()).setCells(eq(0), anyMap());

        // There was a NPE related to this, hence it is tested here
        this.presenter.onSave(new ByteArrayOutputStream(), null);

        undo();
        assertThat(cellTile(layer, 0, 1), is(-1));
        assertThat(cellTile(layer, 1, 1), is(-1));
        verify(this.view, times(1)).setCells(eq(0), anyMap());

        // There was a NPE related to this, hence it is tested here
        this.presenter.onSave(new ByteArrayOutputStream(), null);

        redo();
        assertThat(cellTile(layer, 0, 1), is(1));
        assertThat(cellTile(layer, 1, 1), is(1));
        verify(this.view, times(2)).setCells(eq(0), anyMap());

        // Erase

        this.presenter.onSelectTile(-1, false, false);
        verify(this.view, times(2)).setBrush(any(MapBrush.class));

        this.presenter.onPaintBegin();

        this.presenter.onPaint(0,1);
        this.presenter.onPaint(0,1);
        assertThat(cellTile(layer, 0, 1), is(-1));
        cellIndex = Layer.toCellIndex(0, 1);
        verify(this.view, times(4)).setCell(eq(0), eq(cellIndex), any(Cell.class));

        this.presenter.onPaintEnd();

        verify(this.view, times(2)).setCells(eq(0), anyMap());

        undo();
        assertThat(cellTile(layer, 0, 1), is(1));
        verify(this.view, times(3)).setCells(eq(0), anyMap());

        redo();
        assertThat(cellTile(layer, 0, 1), is(-1));
        verify(this.view, times(4)).setCells(eq(0), anyMap());
    }

    /**
     * Pick cells and paint them into other cells
     * @throws Exception
     *
     */
    @Test
    public void testCellPicking() throws Exception {
        testUseCase211();

        List<Layer> layers = this.model.getLayers();
        Layer layer = layers.get(0);

        // Paint

        this.presenter.onSelectLayer(layer);

        this.presenter.onSelectTile(1, false, false);
        verify(this.view, times(1)).setBrush(any(MapBrush.class));

        // Paint in cells to be picked
        this.presenter.onPaintBegin();
        this.presenter.onPaint(1, 0);
        this.presenter.onPaint(0, 1);
        this.presenter.onPaintEnd();

        this.presenter.onSelectTile(-1, false, false);
        verify(this.view, times(2)).setBrush(any(MapBrush.class));

        this.presenter.onPaintBegin();
        this.presenter.onPaint(0, 0);
        this.presenter.onPaint(1, 1);
        this.presenter.onPaintEnd();

        this.presenter.onSelectCells(layer, 0, 0, 1, 1);
        verify(this.view, times(3)).setBrush(any(MapBrush.class));

        this.presenter.onPaintBegin();
        this.presenter.onPaint(0, 2);
        this.presenter.onPaintEnd();

        assertThat(cellTile(layer, 0, 2), is(-1));
        assertThat(cellTile(layer, 1, 2), is(1));
        assertThat(cellTile(layer, 0, 3), is(1));
        assertThat(cellTile(layer, 1, 3), is(-1));
    }

    /**
     * Use Case 2.3.1 - Create a Layer
     * @throws Exception
     */
    @Test
    public void testUseCase231() throws Exception {
        testUseCase211();

        presenter.onAddLayer();
        List<Layer> layers = this.model.getLayers();
        assertEquals(2, layers.size());
        Layer layer = layers.get(1);
        assertEquals("layer2", layer.getId());
        assertTrue(layer.isVisible());
        verify(this.view, times(2)).setLayers(anyListOf(Layer.class));
        verify(this.view, times(1)).setSelectedLayer(layers.get(1));

        undo();
        assertEquals(1, this.model.getLayers().size());
        verify(this.view, times(2)).setSelectedLayer(eq((Layer)null));

        redo();
        assertEquals(2, this.model.getLayers().size());
        verify(this.view, times(2)).setSelectedLayer(layers.get(1));

        setLayerProperty(layer, "z", new Float(0.5f));
        assertEquals("layer1", this.model.getLayers().get(0).getId());

        setLayerProperty(layer, "z", new Float(-0.5f));
        assertEquals("layer2", this.model.getLayers().get(0).getId());

        undo();
        assertEquals("layer1", this.model.getLayers().get(0).getId());

        redo();
        assertEquals("layer2", this.model.getLayers().get(0).getId());
    }

    /**
     * Use Case 2.3.2 - Remove a Layer
     * @throws Exception
     */
    @Test
    public void testUseCase232() throws Exception {
        testUseCase231();

        presenter.onSelectLayer(this.model.getLayers().get(1));
        presenter.onRemoveLayer();
        assertEquals(1, this.model.getLayers().size());
        assertEquals("layer2", this.model.getLayers().get(0).getId());

        undo();
        assertEquals(2, this.model.getLayers().size());

        redo();
        assertEquals(1, this.model.getLayers().size());
    }

    /**
     * Testing bug: paint cells into two layers, select previous, layer, undo
     * Expected: cells from last layer should be cleared, not first
     * @throws Exception
     */
    @Test
    public void testPaintMultiple() throws Exception {
        testUseCase211();

        Layer firstLayer = this.model.getLayers().get(0);

        // Paint

        this.presenter.onSelectLayer(firstLayer);

        this.presenter.onSelectTile(1, false, false);

        this.presenter.onPaintBegin();
        this.presenter.onPaint(0,0);
        this.presenter.onPaintEnd();

        this.presenter.onAddLayer();
        Layer secondLayer = this.model.getLayers().get(1);

        this.presenter.onPaintBegin();
        this.presenter.onPaint(0,0);
        this.presenter.onPaintEnd();

        this.presenter.onSelectLayer(firstLayer);

        undo();

        assertThat(cellTile(firstLayer, 0, 0), is(1));
        assertThat(cellTile(secondLayer, 0, 0), is(-1));
    }

    /**
     * Use Case 2.4.1 - Reload Tile Set Image
     * @throws Exception
     */
    @Test
    public void testUseCase241() throws Exception {
        testUseCase211();

        verify(this.view, times(3)).setTileSet(any(BufferedImage.class), anyInt(), anyInt(), anyInt(), anyInt());

        IFile originalImage = this.project.getFile("/mario_tileset.png");
        IFile newImage = this.project.getFile("/mario_half_tileset.png");
        originalImage.setContents(newImage.getContents(), false, true, null);

        verify(this.view, times(4)).setTileSet(any(BufferedImage.class), anyInt(), anyInt(), anyInt(), anyInt());

        originalImage.setContents(originalImage.getHistory(null)[0], false, false, null);

        verify(this.view, times(5)).setTileSet(any(BufferedImage.class), anyInt(), anyInt(), anyInt(), anyInt());
    }

    /**
     * Use Case 2.4.2 - Reload Tile Set
     * @throws Exception
     */
    @Test
    public void testUseCase242() throws Exception {
        testUseCase211();

        verify(this.view, times(3)).setTileSet(any(BufferedImage.class), anyInt(), anyInt(), anyInt(), anyInt());

        newTileSet("/mario.tileset", "/mario_half_tileset.png", 1);

        // Twice (5) because the above function first creates the file from template-data, then write the specified content
        verify(this.view, times(5)).setTileSet(any(BufferedImage.class), anyInt(), anyInt(), anyInt(), anyInt());
    }

    /**
     * Use Case 2.5.1 - Pan/Zoom Camera
     * @throws Exception
     */
    @Test
    public void testUseCase251() throws Exception {
        when(view.getPreviewRect()).thenReturn(new Rectangle(0, 0, 600, 400));

        this.presenter.onPreviewPan(10, 10);
        verify(this.view, times(1)).setPreview(eq(new Point2f(-10.0f, 10.0f)), eq(1.0f));

        double d = -0.05;
        this.presenter.onPreviewZoom(d);
        float scale = 1.0f + (float)d;
        verify(this.view, times(1)).setPreview(eq(new Point2f(-10.0f, 10.0f)), eq(scale, 0.001f));

    }

    /**
     * Use Case 2.5.2 - Frame Camera
     * @throws Exception
     */
    @Test
    public void testUseCase252() throws Exception {
        testUseCase221();
        testUseCase251();

        this.presenter.onPreviewFrame();
        int tileWidth = this.model.getTileSetModel().getTileWidth();
        int tileHeight = this.model.getTileSetModel().getTileHeight();

        Rectangle clientRect = this.view.getPreviewRect();
        Vector2f clientDim = new Vector2f(clientRect.width, clientRect.height);
        clientDim.scale(0.8f);
        float zoom = Math.min(clientDim.getX() / tileWidth, clientDim.getY() / tileHeight);
        verify(this.view, times(1)).setPreview(eq(new Point2f(tileWidth * 1.5f, tileHeight * 1.5f)), eq(zoom, 0.001f));
    }

    /**
     * Use Case 2.5.3 - Reset Camera
     * @throws Exception
     */
    @Test
    public void testUseCase253() throws Exception {
        testUseCase221();
        testUseCase251();

        this.presenter.onPreviewResetZoom();
        verify(this.view, times(2)).setPreview(eq(new Point2f(-10.0f, 10.0f)), eq(1.0f, 0.001f));
    }

    private void assertMessage(String expectedMessage, String property) {
        IStatus status = propertyModel.getPropertyStatus(property);

        ArrayList<String> foundMessages = new ArrayList<String>();
        if (status.isMultiStatus()) {
            for (IStatus s : status.getChildren()) {
                foundMessages.add("'" + s.getMessage() + "'");
                if (s.getMessage().equals(expectedMessage))
                    return;
            }
        } else {
            foundMessages.add("'" + status.getMessage() + "'");
            if (status.getMessage().equals(expectedMessage))
                return;
        }

        String found = Joiner.on(",").join(foundMessages);
        assertTrue(String.format("Expected message '%s' not present in status. Found %s", expectedMessage, found), false);
    }

    private void assertMessageSeverity(int severity, String property) {
        IStatus status = this.propertyModel.getPropertyStatus(property);
        int maxSeverity = -1;
        if (status.isMultiStatus()) {
            for (IStatus s : status.getChildren()) {
                if (s.getSeverity() > maxSeverity) {
                    maxSeverity = s.getSeverity();
                }
            }
        } else {
            if (status.getSeverity() > maxSeverity) {
                maxSeverity = status.getSeverity();
            }
        }
        assertEquals(severity, maxSeverity);
    }

    /**
     * Message 2.1 - Tile set not specified
     * @throws IOException
     * @throws CoreException
     */
    @Test
    public void testMessage21() throws Exception {
        newGrid();
        newMarioTileSet();

        assertTrue(!propertyModel.getPropertyStatus("tileSource").isOK());
        assertMessage(Messages.GridModel_tileSource_EMPTY, "tileSource");
        assertMessageSeverity(IStatus.INFO, "tileSource");
        verify(this.view, times(1)).refreshProperties();

        setGridProperty("tileSource", "/mario.tileset");
        assertTrue(propertyModel.getPropertyStatus("tileSource").isOK());
        verify(this.view, times(2)).refreshProperties();
    }

    /**
     * Message 2.2 - Tile set not found
     * @throws IOException
     * @throws CoreException
     */
    @Test
    public void testMessage22() throws Exception {
        newMarioTileSet();
        newGrid();

        assertTrue(!propertyModel.getPropertyStatus("tileSource").isOK());
        verify(this.view, times(1)).refreshProperties();
        verify(this.view, times(1)).setValidModel(eq(true));

        String invalidPath = "/test";
        setGridProperty("tileSource", invalidPath);
        assertTrue(!propertyModel.getPropertyStatus("tileSource").isOK());
        assertMessage(NLS.bind(Messages.GridModel_tileSource_NOT_FOUND, invalidPath), "tileSource");

        verify(this.view, times(2)).refreshProperties();
        verify(this.view, times(1)).setValidModel(eq(false));

        setGridProperty("tileSource", "/mario.tileset");
        assertTrue(propertyModel.getPropertyStatus("tileSource").isOK());
        verify(this.view, times(3)).refreshProperties();
        verify(this.view, times(2)).setValidModel(eq(true));
    }

    private void saveTileSet(IFile file, TileSetModel tileSetModel) throws Exception {
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        tileSetModel.save(stream, null);
        file.setContents(new ByteArrayInputStream(stream.toByteArray()), false, true, null);
    }

    private void assertInvalidTileSet(IFile file, Object id, Object value) throws Exception {
        TileSetModel tileSetModel = this.model.getTileSetModel();
        setTileSetProperty(tileSetModel, id, value);

        saveTileSet(file, tileSetModel);

        assertEquals(Messages.GRID_INVALID_TILESET, this.propertyModel.getPropertyStatus("tileSource").getMessage());
        assertTrue(!this.propertyModel.getPropertyStatus("tileSource").isOK());

        undo();

        saveTileSet(file, tileSetModel);

        assertTrue(this.propertyModel.getPropertyStatus("tileSource").isOK());
    }

    /**
     * Message 2.3 - Invalid tile set
     * @throws IOException
     * @throws CoreException
     */
    @Test
    public void testMessage23() throws Throwable {
        newMarioTileSet();
        newGrid();

        String tileSetPath = "/mario.tileset";

        assertTrue(!propertyModel.getPropertyStatus("tileSource").isOK());
        setGridProperty("tileSource", tileSetPath);
        assertTrue(propertyModel.getPropertyStatus("tileSource").isOK());

        IFile tileSetFile = this.contentRoot.getFile(new Path(tileSetPath));

        // no image and no collision
        assertInvalidTileSet(tileSetFile, "image", "");

        // image not found
        assertInvalidTileSet(tileSetFile, "image", "non_existing.png");

        // collision not found
        assertInvalidTileSet(tileSetFile, "collision", "non_existing.png");

        // image/collision different dimensions
        assertInvalidTileSet(tileSetFile, "collision", "/mario_half_tileset.png");

        // invalid tile width
        assertInvalidTileSet(tileSetFile, "tileWidth", new Integer(-1));

        // invalid tile height
        assertInvalidTileSet(tileSetFile, "tileHeight", new Integer(-1));

        // total tile width
        assertInvalidTileSet(tileSetFile, "tileWidth", new Integer(10000));

        // total tile height
        assertInvalidTileSet(tileSetFile, "tileHeight", new Integer(10000));

        // empty material
        assertInvalidTileSet(tileSetFile, "materialTag", "");

        // invalid tile margin
        assertInvalidTileSet(tileSetFile, "tileMargin", new Integer(-1));

        // invalid tile spacing
        assertInvalidTileSet(tileSetFile, "tileSpacing", new Integer(-1));
    }

    /**
     * Message 2.4 - Duplicated layer ids
     * @throws Exception
     */
    @Test
    public void testMessage24() throws Exception {
        testUseCase231();

        Layer layer = this.model.getLayers().get(0);
        @SuppressWarnings("unchecked")
        IPropertyModel<Layer, GridModel> propertyModel = (IPropertyModel<Layer, GridModel>)layer.getAdapter(IPropertyModel.class);

        assertTrue(propertyModel.getPropertyStatus("id").isOK());
        verify(this.view, times(17)).refreshProperties();

        setProperty(propertyModel, "id", "layer1");
        assertEquals(Messages.GRID_DUPLICATED_LAYER_IDS, propertyModel.getPropertyStatus("id").getMessage());
        verify(this.view, times(18)).refreshProperties();
    }

    @Test
    public void testDirty() throws Exception {
        int dirtyTrueCount = 0;
        int dirtyFalseCount = 0;

        newMarioTileSet();
        newGrid();
        assertThat(presenter.isDirty(), is(false));
        verify(this.view, times(dirtyFalseCount)).setDirty(false);

        setGridProperty("tileSource", "/mario.tileset");
        assertThat(presenter.isDirty(), is(true));
        verify(this.view, times(++dirtyTrueCount)).setDirty(true);

        presenter.onSave(new ByteArrayOutputStream(), new NullProgressMonitor());
        assertThat(presenter.isDirty(), is(false));
        verify(this.view, times(++dirtyFalseCount)).setDirty(false);

        this.presenter.onSelectLayer(this.model.getLayers().get(0));

        this.presenter.onSelectTile(1, false, false);
        assertThat(presenter.isDirty(), is(false));
        verify(this.view, times(dirtyFalseCount)).setDirty(false);

        this.presenter.onPaintBegin();
        this.presenter.onPaint(0,1);
        assertThat(presenter.isDirty(), is(false));
        verify(this.view, times(dirtyFalseCount)).setDirty(false);
        this.presenter.onPaintEnd();
        assertThat(presenter.isDirty(), is(true));
        verify(this.view, times(++dirtyTrueCount)).setDirty(true);

        undo();
        verify(this.view, times(++dirtyFalseCount)).setDirty(false);
        assertThat(presenter.isDirty(), is(false));
    }

    @Test
    public void testLoadSave() throws Exception {
        TileGrid tileGrid  = TileGrid.newBuilder()
                .setTileSet("my.tileset")
                .setMaterial("/builtins/materials/tile_map.material")
                .setBlendMode(BlendMode.BLEND_MODE_ALPHA)
                .addTextures("my.tileset")
                .addLayers(TileLayer.newBuilder()
                        .setId("some_id")
                        .setIsVisible(1)
                        .setZ(123)
                        .addCell(TileCell.newBuilder().setTile(12).setX(1).setY(2).setHFlip(0).setVFlip(1))
                        .build())
                        .build();

        String tileSetFileData = TextFormat.printToString(tileGrid);

        this.presenter.onLoad(new ByteArrayInputStream(tileSetFileData.getBytes()));
        ByteArrayOutputStream os = new ByteArrayOutputStream();
        this.presenter.onSave(os, new NullProgressMonitor());

        TileGrid.Builder loadedTileGridBuilder = TileGrid.newBuilder();
        TextFormat.merge(new StringReader(new String(os.toByteArray())), loadedTileGridBuilder);
        TileGrid loadedTileGrid = loadedTileGridBuilder.build();
        assertEquals(tileGrid, loadedTileGrid);
    }

    @Override
    public void resourceChanged(IResourceChangeEvent event) {
        if (this.presenter != null) {
            try {
                this.presenter.onResourceChanged(event);
            } catch (Throwable e) {
                throw new RuntimeException(e);
            }
        }
    }

    @Test
    public void testIndexed() throws Exception {
        String image = "/indexed_1x4_16.png";
        TileSetModel tileSet = newTileSet("/mario.tileset", image, 0);
        tileSet.setCollision(image);
        assertEquals(tileSet.getConvexHulls().size(), 4);
    }
}
