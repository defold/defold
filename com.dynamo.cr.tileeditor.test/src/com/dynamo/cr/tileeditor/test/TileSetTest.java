package com.dynamo.cr.tileeditor.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import java.awt.Color;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.ui.views.properties.IPropertySource;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.tileeditor.core.ITileSetView;
import com.dynamo.cr.tileeditor.core.TileSetModel;
import com.dynamo.cr.tileeditor.core.TileSetPresenter;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.TextFormat;

public class TileSetTest {
    ITileSetView view;
    TileSetModel model;
    TileSetPresenter presenter;
    IOperationHistory history;
    IUndoContext undoContext;
    IPropertySource propertySource;

    @Before
    public void setup() {
        System.setProperty("java.awt.headless", "true");
        this.view = mock(ITileSetView.class);
        this.history = new DefaultOperationHistory();
        this.undoContext = new UndoContext();
        this.model = new TileSetModel(this.history, this.undoContext);
        this.presenter = new TileSetPresenter(this.model, this.view);
        this.propertySource = (IPropertySource) this.model.getAdapter(IPropertySource.class);
    }

    private TileSet loadEmptyFile() throws IOException {
        // new file
        TileSet tileSet = TileSet.newBuilder()
                .setImage("")
                .setTileWidth(0)
                .setTileHeight(0)
                .setTileSpacing(0)
                .setTileMargin(0)
                .setCollision("")
                .setMaterialTag("tile")
                .addCollisionGroups("tile")
                .build();
        this.presenter.load(tileSet);
        return tileSet;
    }

    /**
     * Test load
     * @throws IOException
     */
    @Test
    public void testLoad() throws IOException {
        TileSet tileSet = loadEmptyFile();

        assertEquals(tileSet.getImage(), this.model.getImage());
        assertEquals(tileSet.getTileWidth(), this.model.getTileWidth());
        assertEquals(tileSet.getTileHeight(), this.model.getTileHeight());
        assertEquals(tileSet.getTileMargin(), this.model.getTileMargin());
        assertEquals(tileSet.getTileSpacing(), this.model.getTileSpacing());
        assertEquals(tileSet.getCollision(), this.model.getCollision());
        assertEquals(tileSet.getMaterialTag(), this.model.getMaterialTag());

        verify(this.view, times(1)).setImageProperty(any(String.class));
        verify(this.view, never()).setTileWidthProperty(anyInt());
        verify(this.view, never()).setTileHeightProperty(anyInt());
        verify(this.view, never()).setTileMarginProperty(anyInt());
        verify(this.view, never()).setTileSpacingProperty(anyInt());
        verify(this.view, times(1)).setCollisionProperty(any(String.class));
        verify(this.view, times(1)).setMaterialTagProperty(any(String.class));
    }

    /**
     * Test undo/redo
     * @throws IOException
     */
    @SuppressWarnings("unchecked")
    @Test
    public void testUndoRedo() throws Exception {
        TileSet tileSet = loadEmptyFile();

        String prevTileSetFile = tileSet.getImage();
        String tileSetFile = "test/mario_tileset.png";

        assertEquals(prevTileSetFile, this.model.getImage());
        assertEquals(prevTileSetFile, this.model.getCollision());

        propertySource.setPropertyValue("image", tileSetFile);
        assertEquals(tileSetFile, this.model.getImage());
        verify(this.view, times(2)).setImageProperty(any(String.class));

        propertySource.setPropertyValue("collision", tileSetFile);
        assertEquals(tileSetFile, this.model.getCollision());
        verify(this.view, times(2)).setCollisionProperty(any(String.class));

        this.history.undo(this.undoContext, new NullProgressMonitor(), null);

        assertEquals(tileSetFile, this.model.getImage());
        assertEquals(prevTileSetFile, this.model.getCollision());
        verify(this.view, times(3)).setCollisionProperty(any(String.class));

        this.history.undo(this.undoContext, new NullProgressMonitor(), null);

        assertEquals(prevTileSetFile, this.model.getImage());
        verify(this.view, times(3)).setImageProperty(any(String.class));

        this.history.redo(this.undoContext, new NullProgressMonitor(), null);

        assertEquals(tileSetFile, this.model.getImage());
        assertEquals(prevTileSetFile, this.model.getCollision());
        verify(this.view, times(4)).setImageProperty(any(String.class));

        this.history.redo(this.undoContext, new NullProgressMonitor(), null);

        assertEquals(tileSetFile, this.model.getImage());
        assertEquals(tileSetFile, this.model.getCollision());
        verify(this.view, times(4)).setCollisionProperty(any(String.class));

        propertySource.setPropertyValue("tileWidth", 16);
        assertEquals(16, this.model.getTileWidth());
        verify(this.view, times(1)).setTileWidthProperty(anyInt());
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(0, this.model.getTileWidth());
        verify(this.view, times(2)).setTileWidthProperty(anyInt());
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(16, this.model.getTileWidth());
        verify(this.view, times(3)).setTileWidthProperty(anyInt());

        propertySource.setPropertyValue("tileHeight", 16);
        assertEquals(16, this.model.getTileHeight());
        verify(this.view, times(1)).setTileHeightProperty(anyInt());
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(0, this.model.getTileHeight());
        verify(this.view, times(2)).setTileHeightProperty(anyInt());
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(16, this.model.getTileHeight());
        verify(this.view, times(3)).setTileHeightProperty(anyInt());

        propertySource.setPropertyValue("tileMargin", 1);
        assertEquals(1, this.model.getTileMargin());
        verify(this.view, times(1)).setTileMarginProperty(anyInt());
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(0, this.model.getTileMargin());
        verify(this.view, times(2)).setTileMarginProperty(anyInt());
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(1, this.model.getTileMargin());
        verify(this.view, times(3)).setTileMarginProperty(anyInt());

        this.history.undo(this.undoContext, new NullProgressMonitor(), null);

        propertySource.setPropertyValue("tileSpacing", 1);
        assertEquals(1, this.model.getTileSpacing());
        verify(this.view, times(1)).setTileSpacingProperty(anyInt());
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(0, this.model.getTileSpacing());
        verify(this.view, times(2)).setTileSpacingProperty(anyInt());
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(1, this.model.getTileSpacing());
        verify(this.view, times(3)).setTileSpacingProperty(anyInt());

        propertySource.setPropertyValue("materialTag", "tile_material");
        assertEquals("tile_material", this.model.getMaterialTag());
        verify(this.view, times(2)).setMaterialTagProperty(any(String.class));
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals("tile", this.model.getMaterialTag());
        verify(this.view, times(3)).setMaterialTagProperty(any(String.class));
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals("tile_material", this.model.getMaterialTag());
        verify(this.view, times(4)).setMaterialTagProperty(any(String.class));

        this.presenter.addCollisionGroup("obstruction");
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("obstruction", this.model.getCollisionGroups().get(0));
        assertEquals("tile", this.model.getCollisionGroups().get(1));
        verify(this.view, times(2)).setCollisionGroups(any(List.class), any(List.class));
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(1, this.model.getCollisionGroups().size());
        assertEquals("tile", this.model.getCollisionGroups().get(0));
        verify(this.view, times(3)).setCollisionGroups(any(List.class), any(List.class));
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("obstruction", this.model.getCollisionGroups().get(0));
        verify(this.view, times(4)).setCollisionGroups(any(List.class), any(List.class));

        // make the remove test below remove from middle of list, better test
        this.presenter.addCollisionGroup("hazard");

        this.presenter.removeCollisionGroup("obstruction");
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("hazard", this.model.getCollisionGroups().get(0));
        assertEquals("tile", this.model.getCollisionGroups().get(1));
        verify(this.view, times(6)).setCollisionGroups(any(List.class), any(List.class));
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(3, this.model.getCollisionGroups().size());
        assertEquals("obstruction", this.model.getCollisionGroups().get(1));
        verify(this.view, times(7)).setCollisionGroups(any(List.class), any(List.class));
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("tile", this.model.getCollisionGroups().get(1));
        verify(this.view, times(8)).setCollisionGroups(any(List.class), any(List.class));

        // for later verification
        List<TileSetModel.ConvexHull> tileHulls = new ArrayList<TileSetModel.ConvexHull>();
        for (TileSetModel.ConvexHull hull : this.model.getConvexHulls()) {
            if (hull.getCollisionGroup().equals("tile")) {
                tileHulls.add(hull);
            }
        }
        assertTrue(tileHulls.size() != 0);

        this.presenter.renameCollisionGroup("tile", "a_group");
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("a_group", this.model.getCollisionGroups().get(0));
        assertEquals("hazard", this.model.getCollisionGroups().get(1));
        for (TileSetModel.ConvexHull hull : tileHulls) {
            assertEquals("a_group", hull.getCollisionGroup());
        }
        verify(this.view, times(9)).setCollisionGroups(any(List.class), any(List.class));
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals("hazard", this.model.getCollisionGroups().get(0));
        assertEquals("tile", this.model.getCollisionGroups().get(1));
        for (TileSetModel.ConvexHull hull : tileHulls) {
            assertEquals("tile", hull.getCollisionGroup());
        }
        verify(this.view, times(10)).setCollisionGroups(any(List.class), any(List.class));
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals("a_group", this.model.getCollisionGroups().get(0));
        assertEquals("hazard", this.model.getCollisionGroups().get(1));
        for (TileSetModel.ConvexHull hull : tileHulls) {
            assertEquals("a_group", hull.getCollisionGroup());
        }
        verify(this.view, times(11)).setCollisionGroups(any(List.class), any(List.class));

        // Renaming the group will call this for every operation
        verify(this.view, times(3)).setTileHullColor(eq(0), any(Color.class));

        assertEquals("a_group", this.model.getConvexHulls().get(0).getCollisionGroup());
        this.presenter.setConvexHullCollisionGroup(0, "hazard");
        assertEquals("hazard", this.model.getConvexHulls().get(0).getCollisionGroup());
        verify(this.view, times(4)).setTileHullColor(eq(0), any(Color.class));
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals("a_group", this.model.getConvexHulls().get(0).getCollisionGroup());
        verify(this.view, times(5)).setTileHullColor(eq(0), any(Color.class));
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals("hazard", this.model.getConvexHulls().get(0).getCollisionGroup());
        verify(this.view, times(6)).setTileHullColor(eq(0), any(Color.class));
    }


    /**
     * Use Case 1.1
     * @throws IOException
     */
    @SuppressWarnings("unchecked")
    @Test
    public void testSuperMarioTileSet() throws IOException {
        loadEmptyFile();

        String tileSetFile = "test/mario_tileset.png";

        // set properties (1.1.1)

        propertySource.setPropertyValue("image", tileSetFile);
        assertEquals(tileSetFile, this.model.getImage());
        verify(this.view, times(2)).setImageProperty(any(String.class));

        propertySource.setPropertyValue("tileWidth", 16);
        assertEquals(16, this.model.getTileWidth());
        verify(this.view, times(1)).setTileWidthProperty(anyInt());

        propertySource.setPropertyValue("tileHeight", 16);
        assertEquals(16, this.model.getTileHeight());
        verify(this.view, times(1)).setTileHeightProperty(anyInt());

        propertySource.setPropertyValue("tileSpacing", 1);
        assertEquals(1, this.model.getTileSpacing());
        verify(this.view, times(1)).setTileSpacingProperty(anyInt());

        assertEquals(20, this.model.getConvexHulls().size());

        for (TileSetModel.ConvexHull convexHull : this.model.getConvexHulls()) {
            assertEquals(0, convexHull.getCount());
        }

        propertySource.setPropertyValue("collision", tileSetFile);
        assertEquals(tileSetFile, this.model.getCollision());
        verify(this.view, times(2)).setCollisionProperty(any(String.class));

        assertEquals(tileSetFile, this.model.getCollision());
        for (int i = 0; i < 10; ++i) {
            assertEquals(8, this.model.getConvexHulls().get(i).getCount());
        }
        for (int i = 10; i < 14; ++i) {
            assertEquals(4, this.model.getConvexHulls().get(i).getCount());
        }
        for (int i = 14; i < 20; ++i) {
            assertEquals(8, this.model.getConvexHulls().get(i).getCount());
        }

        // set properties (1.1.4)

        assertEquals(1, this.model.getCollisionGroups().size());

        this.presenter.addCollisionGroup("hazard");
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("hazard", this.model.getCollisionGroups().get(0));
        assertEquals("tile", this.model.getCollisionGroups().get(1));
        verify(this.view, times(2)).setCollisionGroups(any(List.class), any(List.class));

        this.presenter.setConvexHullCollisionGroup(1, "hazard");
        assertEquals("hazard", this.model.getConvexHulls().get(1).getCollisionGroup());
        verify(this.view, times(1)).setTileHullColor(eq(1), any(Color.class));

        // set properties (1.1.5)

        this.presenter.renameCollisionGroup("tile", "obstruction");
        assertEquals("obstruction", this.model.getCollisionGroups().get(1));
        assertEquals("obstruction", this.model.getConvexHulls().get(0).getCollisionGroup());
        verify(this.view, times(3)).setCollisionGroups(any(List.class), any(List.class));
        verify(this.view, times(1)).setTileHullColor(eq(0), any(Color.class));

        // saving

        String newTileSetPath = "test/tmp.tileset";
        File newTileSetFile = new File(newTileSetPath);

        FileOutputStream outputStream = new FileOutputStream(newTileSetFile);
        this.presenter.save(outputStream, new NullProgressMonitor());
        TileSet.Builder tileSetBuilder = TileSet.newBuilder();
        TextFormat.merge(new InputStreamReader(new FileInputStream(newTileSetPath)), tileSetBuilder);
        TileSet tileSet = tileSetBuilder.build();
        newTileSetFile = new File(newTileSetPath);
        newTileSetFile.delete();
        TileSetModel newModel = new TileSetModel(this.history, this.undoContext);
        newModel.load(tileSet);
        assertEquals(this.model.getImage(), newModel.getImage());
        assertEquals(this.model.getTileWidth(), newModel.getTileWidth());
        assertEquals(this.model.getTileHeight(), newModel.getTileHeight());
        assertEquals(this.model.getTileMargin(), newModel.getTileMargin());
        assertEquals(this.model.getTileSpacing(), newModel.getTileSpacing());
        assertEquals(this.model.getCollision(), newModel.getCollision());
        assertEquals(this.model.getMaterialTag(), newModel.getMaterialTag());
        assertEquals(this.model.getConvexHulls(), newModel.getConvexHulls());
        assertEquals(this.model.getConvexHullPoints().length, newModel.getConvexHullPoints().length);
        for (int i = 0; i < this.model.getConvexHullPoints().length; ++i) {
            assertEquals(this.model.getConvexHullPoints()[i], newModel.getConvexHullPoints()[i], 0.000001);
        }
        assertEquals(this.model.getCollisionGroups(), newModel.getCollisionGroups());
    }

    /**
     * Tag 1.1
     * @throws IOException
     */
    @SuppressWarnings("unchecked")
    @Test
    public void testTag11() throws IOException {
        loadEmptyFile();

        assertTrue(this.model.hasPropertyAnnotation("image", TileSetModel.TAG_11));
        verify(this.view, times(1)).setImageTags(any(Set.class));

        propertySource.setPropertyValue("image", "test");
        assertTrue(!this.model.hasPropertyAnnotation("image", TileSetModel.TAG_11));
        verify(this.view, times(2)).setImageTags(any(Set.class));
    }
}
