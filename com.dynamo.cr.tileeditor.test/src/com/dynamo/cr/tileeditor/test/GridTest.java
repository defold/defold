package com.dynamo.cr.tileeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Matchers.anyFloat;
import static org.mockito.Matchers.anyList;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import java.io.ByteArrayInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.Enumeration;

import javax.inject.Singleton;

import org.eclipse.core.commands.ExecutionException;
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
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.eclipse.osgi.util.NLS;
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
import com.dynamo.cr.tileeditor.core.ILogger;
import com.dynamo.cr.tileeditor.core.Messages;
import com.dynamo.cr.tileeditor.core.TileSetModel;
import com.google.common.base.Joiner;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class GridTest {

    private IGridView view;
    private IGridView.Presenter presenter;
    private GridModel model;
    private IOperationHistory history;
    private IProject project;
    private IContainer contentRoot;
    private IPropertyModel<GridModel, GridModel> propertyModel;
    private Injector injector;

    static class TestLogger implements ILogger {

        @Override
        public void logException(Throwable exception) {
            throw new UnsupportedOperationException(exception);
        }

    }

    class TestModule extends AbstractModule {
        @Override
        protected void configure() {
            bind(IGridView.class).toInstance(view);
            bind(IGridView.Presenter.class).to(GridPresenter.class).in(Singleton.class);
            bind(GridModel.class).in(Singleton.class);

            bind(IOperationHistory.class).to(DefaultOperationHistory.class).in(Singleton.class);
            bind(IUndoContext.class).to(UndoContext.class).in(Singleton.class);

            bind(ILogger.class).to(TestLogger.class);

            bind(IContainer.class).toInstance(contentRoot);
        }
    }

    @SuppressWarnings("unchecked")
    @Before
    public void setup() throws CoreException, IOException {
        System.setProperty("java.awt.headless", "true");

        this.project = ResourcesPlugin.getWorkspace().getRoot().getProject("test");
        if (this.project.exists()) {
            this.project.delete(true, null);
        }
        this.project.create(null);
        this.project.open(null);

        IProjectDescription pd = this.project.getDescription();
        pd.setNatureIds(new String[] { "com.dynamo.cr.editor.core.crnature" });
        this.project.setDescription(pd, null);

        Bundle bundle = Platform.getBundle("com.dynamo.cr.tileeditor.test");
        Enumeration<URL> entries = bundle.findEntries("/test", "*", true);
        while (entries.hasMoreElements()) {
            URL url = entries.nextElement();
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
        this.model = this.injector.getInstance(GridModel.class);
        this.presenter = this.injector.getInstance(IGridView.Presenter.class);
        this.propertyModel = (IPropertyModel<GridModel, GridModel>) this.model.getAdapter(IPropertyModel.class);
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
        String path = "/test.grid";
        newResourceFile(path);
        IFile file = this.contentRoot.getFile(new Path(path));
        this.presenter.onLoad(file.getContents());
    }

    private void newTileSet(String path, String image) throws IOException, CoreException {
        newResourceFile(path);
        IFile file = this.contentRoot.getFile(new Path(path));
        TileSetModel tileSetModel = new TileSetModel(this.contentRoot, new DefaultOperationHistory(), new UndoContext());
        tileSetModel.load(file.getContents());
        tileSetModel.setImage(image);
        OutputStream os = new FileOutputStream(file.getLocation().toFile());
        try {
            tileSetModel.save(os, null);
        } finally {
            os.close();
        }
    }

    private void newMarioTileSet() throws IOException, CoreException {
        newTileSet("/mario.tileset", "/mario_tileset.png");
    }

    private void setGridProperty(Object id, Object value) throws ExecutionException {
        this.history.execute(this.propertyModel.setPropertyValue(id, value), null, null);
    }

    private String tileSet() {
        return this.model.getTileSet();
    }

    /**
     * Test load
     */
    @SuppressWarnings("unchecked")
    @Test
    public void testLoad() throws Exception {
        newGrid();

        assertEquals("", this.model.getTileSet());
        assertEquals(16.0f, this.model.getCellWidth(), 0.000001f);
        assertEquals(16.0f, this.model.getCellHeight(), 0.000001f);
        assertEquals(1, this.model.getLayers().size());
        GridModel.Layer layer = this.model.getLayers().get(0);
        assertEquals("layer1", layer.getId());
        assertEquals(0.0f, layer.getZ(), 0.000001f);
        assertTrue(layer.isVisible());

        verify(this.view, times(1)).setTileSetProperty(any(String.class));
        verify(this.view, times(1)).setCellWidthProperty(anyFloat());
        verify(this.view, times(1)).setCellHeightProperty(anyFloat());
        verify(this.view, times(1)).setLayers(anyList());
    }

    /**
     * Test use case 2.1.1
     * @throws Exception
     */
    @Test
    public void testUseCase211() throws Exception {
        newMarioTileSet();
        newGrid();

        setGridProperty("tileSet", "/test/mario.tileset");
        setGridProperty("cellHeight", 16);
        setGridProperty("cellWidth", 16);

        assertThat(tileSet(), is("/test/mario.tileset"));
        assertEquals(16, this.model.getCellWidth(), 0.0001);
        assertEquals(16, this.model.getCellHeight(), 0.0001);

        this.presenter.onTileSelected(1);
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

    /**
     * Message 2.1 - Tile set not specified
     * @throws IOException
     * @throws CoreException
     */
    @Test
    public void testMessage21() throws Exception {
        newGrid();
        newMarioTileSet();

        assertTrue(!propertyModel.getPropertyStatus("tileSet").isOK());
        assertMessage(Messages.GridModel_ResourceValidator_tileSet_NOT_SPECIFIED, "tileSet");
        verify(this.view, times(1)).refreshProperties();

        setGridProperty("tileSet", "/does_not_exists.tileset");
        assertMessage(NLS.bind(Messages.GridModel_ResourceValidator_tileSet_NOT_FOUND, "/does_not_exists.tileset"), "tileSet");

        setGridProperty("tileSet", "/mario.tileset");
        assertTrue(propertyModel.getPropertyStatus("tileSet").isOK());
        verify(this.view, times(3)).refreshProperties();
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

        assertTrue(!propertyModel.getPropertyStatus("tileSet").isOK());
        verify(this.view, times(1)).refreshProperties();
        verify(this.view, never()).setValid(anyBoolean());

        String invalidPath = "/test";
        setGridProperty("tileSet", invalidPath);
        assertTrue(!propertyModel.getPropertyStatus("tileSet").isOK());
        assertMessage(NLS.bind(Messages.GridModel_ResourceValidator_tileSet_NOT_FOUND, invalidPath), "tileSet");

        verify(this.view, times(2)).refreshProperties();
        verify(this.view, times(1)).setValid(eq(false));

        setGridProperty("tileSet", "/mario.tileset");
        assertTrue(propertyModel.getPropertyStatus("tileSet").isOK());
        verify(this.view, times(3)).refreshProperties();
        verify(this.view, times(1)).setValid(eq(true));
    }

    /**
     * Message 2.3 - Invalid tile set
     * @throws IOException
     * @throws CoreException
     */
    @Test
    public void testMessage23() throws Exception {
        newTileSet("/missing_image.tileset", "/test/does_not_exists.png");
        newMarioTileSet();
        newGrid();

        assertTrue(!propertyModel.getPropertyStatus("tileSet").isOK());
        setGridProperty("tileSet", "/missing_image.tileset");
        assertEquals(Messages.GRID_INVALID_TILESET, propertyModel.getPropertyStatus("tileSet").getMessage());
        assertTrue(!propertyModel.getPropertyStatus("tileSet").isOK());

        setGridProperty("tileSet", "/mario.tileset");
        assertTrue(propertyModel.getPropertyStatus("tileSet").isOK());
    }

    /**
     * Message 2.4/2.5 - Invalid cell width/height
     * @throws IOException
     * @throws CoreException
     */
    @Test
    public void testMessage24_25() throws Exception {
        newGrid();

        assertTrue(propertyModel.getPropertyStatus("cellWidth").isOK());
        setGridProperty("cellWidth", -1);
        assertTrue(!propertyModel.getPropertyStatus("cellWidth").isOK());
        assertEquals("Value out of range", propertyModel.getPropertyStatus("cellWidth").getMessage());
        setGridProperty("cellWidth", 32);
        assertTrue(propertyModel.getPropertyStatus("cellWidth").isOK());

        assertTrue(propertyModel.getPropertyStatus("cellHeight").isOK());
        setGridProperty("cellHeight", -1);
        assertTrue(!propertyModel.getPropertyStatus("cellHeight").isOK());
        assertEquals("Value out of range", propertyModel.getPropertyStatus("cellHeight").getMessage());

        setGridProperty("cellHeight", 32);
        assertTrue(propertyModel.getPropertyStatus("cellHeight").isOK());
    }

    /**
     * Message 2.6 - Duplicated layer ids
     * @throws IOException
     * @throws CoreException
     */
    @Test
    public void testMessage26() throws IOException, CoreException {
        /*
         * TODO
        loadEmptyFile();

        int code = Activator.STATUS_GRID_DUPLICATED_LAYER_IDS;
        List<GridModel.Layer> layers = new ArrayList<GridModel.Layer>();
        GridModel.Layer layer1 = new GridModel.Layer();
        layer1.setId("layer1");
        GridModel.Layer layer1_dup = new GridModel.Layer();
        layer1_dup.setId("layer1");

        layers.add(layer1);

        assertTrue(!this.model.hasPropertyStatus("layers", code));
        this.model.executeOperation(propertyModel.setPropertyValue("layers", layers));
        assertTrue(!this.model.hasPropertyStatus("layers", code));

        layers.add(layer1_dup);
        this.model.executeOperation(propertyModel.setPropertyValue("layers", layers));
        assertEquals(Activator.getStatusMessage(code), this.model.getPropertyStatus("layers", code).getMessage());
        assertTrue(this.model.hasPropertyStatus("layers", code));

        layers.remove(0);
        this.model.executeOperation(propertyModel.setPropertyValue("layers", layers));
        assertTrue(!this.model.hasPropertyStatus("layers", code));*/
    }
}
