package com.dynamo.cr.tileeditor.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

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

        verify(this.view, times(1)).refreshProperties();
        verify(this.view, times(1)).refreshOutline();
        verify(this.view, times(1)).refreshEditingView();
    }

    /**
     * Test undo/redo
     * @throws IOException
     */
    @Test
    public void testUndoRedo() throws Exception {
        TileSet tileSet = loadEmptyFile();

        String prevTileSetFile = tileSet.getImage();
        String tileSetFile = "test/mario_tileset.png";

        assertEquals(prevTileSetFile, this.model.getImage());
        assertEquals(prevTileSetFile, this.model.getCollision());

        propertySource.setPropertyValue("image", tileSetFile);
        assertEquals(tileSetFile, this.model.getImage());
        verify(this.view, times(2)).refreshProperties();

        propertySource.setPropertyValue("collision", tileSetFile);
        assertEquals(tileSetFile, this.model.getCollision());
        verify(this.view, times(3)).refreshProperties();

        this.history.undo(this.undoContext, new NullProgressMonitor(), null);

        assertEquals(tileSetFile, this.model.getImage());
        assertEquals(prevTileSetFile, this.model.getCollision());
        verify(this.view, times(4)).refreshProperties();

        this.history.undo(this.undoContext, new NullProgressMonitor(), null);

        assertEquals(prevTileSetFile, this.model.getImage());
        verify(this.view, times(5)).refreshProperties();

        this.history.redo(this.undoContext, new NullProgressMonitor(), null);

        assertEquals(tileSetFile, this.model.getImage());
        assertEquals(prevTileSetFile, this.model.getCollision());
        verify(this.view, times(6)).refreshProperties();

        this.history.redo(this.undoContext, new NullProgressMonitor(), null);

        assertEquals(tileSetFile, this.model.getImage());
        assertEquals(tileSetFile, this.model.getCollision());
        verify(this.view, times(7)).refreshProperties();

        propertySource.setPropertyValue("tileWidth", 16);
        assertEquals(16, this.model.getTileWidth());
        verify(this.view, times(8)).refreshProperties();
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(0, this.model.getTileWidth());
        verify(this.view, times(9)).refreshProperties();
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(16, this.model.getTileWidth());
        verify(this.view, times(10)).refreshProperties();

        propertySource.setPropertyValue("tileHeight", 16);
        assertEquals(16, this.model.getTileHeight());
        verify(this.view, times(11)).refreshProperties();
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(0, this.model.getTileHeight());
        verify(this.view, times(12)).refreshProperties();
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(16, this.model.getTileHeight());
        verify(this.view, times(13)).refreshProperties();

        propertySource.setPropertyValue("tileMargin", 1);
        assertEquals(1, this.model.getTileMargin());
        verify(this.view, times(14)).refreshProperties();
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(0, this.model.getTileMargin());
        verify(this.view, times(15)).refreshProperties();
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(1, this.model.getTileMargin());
        verify(this.view, times(16)).refreshProperties();

        this.history.undo(this.undoContext, new NullProgressMonitor(), null);

        propertySource.setPropertyValue("tileSpacing", 1);
        assertEquals(1, this.model.getTileSpacing());
        verify(this.view, times(18)).refreshProperties();
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(0, this.model.getTileSpacing());
        verify(this.view, times(19)).refreshProperties();
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(1, this.model.getTileSpacing());
        verify(this.view, times(20)).refreshProperties();

        propertySource.setPropertyValue("materialTag", "tile_material");
        assertEquals("tile_material", this.model.getMaterialTag());
        verify(this.view, times(21)).refreshProperties();
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals("tile", this.model.getMaterialTag());
        verify(this.view, times(22)).refreshProperties();
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals("tile_material", this.model.getMaterialTag());
        verify(this.view, times(23)).refreshProperties();

        this.presenter.addCollisionGroup("obstruction");
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("obstruction", this.model.getCollisionGroups().get(0));
        assertEquals("tile", this.model.getCollisionGroups().get(1));
        verify(this.view, times(2)).refreshOutline();
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(1, this.model.getCollisionGroups().size());
        assertEquals("tile", this.model.getCollisionGroups().get(0));
        verify(this.view, times(3)).refreshOutline();
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("obstruction", this.model.getCollisionGroups().get(0));
        verify(this.view, times(4)).refreshOutline();

        // make the remove test below remove from middle of list, better test
        this.presenter.addCollisionGroup("hazard");

        this.presenter.removeCollisionGroup("obstruction");
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("hazard", this.model.getCollisionGroups().get(0));
        assertEquals("tile", this.model.getCollisionGroups().get(1));
        verify(this.view, times(6)).refreshOutline();
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(3, this.model.getCollisionGroups().size());
        assertEquals("obstruction", this.model.getCollisionGroups().get(1));
        verify(this.view, times(7)).refreshOutline();
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("tile", this.model.getCollisionGroups().get(1));
        verify(this.view, times(8)).refreshOutline();

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
        verify(this.view, times(9)).refreshOutline();
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals("hazard", this.model.getCollisionGroups().get(0));
        assertEquals("tile", this.model.getCollisionGroups().get(1));
        for (TileSetModel.ConvexHull hull : tileHulls) {
            assertEquals("tile", hull.getCollisionGroup());
        }
        verify(this.view, times(10)).refreshOutline();
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals("a_group", this.model.getCollisionGroups().get(0));
        assertEquals("hazard", this.model.getCollisionGroups().get(1));
        for (TileSetModel.ConvexHull hull : tileHulls) {
            assertEquals("a_group", hull.getCollisionGroup());
        }
        verify(this.view, times(11)).refreshOutline();

        assertEquals("a_group", this.model.getConvexHulls().get(0).getCollisionGroup());
        this.presenter.setConvexHullCollisionGroup(0, "hazard");
        assertEquals("hazard", this.model.getConvexHulls().get(0).getCollisionGroup());
        verify(this.view, times(5)).refreshEditingView();
        this.history.undo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals("a_group", this.model.getConvexHulls().get(0).getCollisionGroup());
        verify(this.view, times(6)).refreshEditingView();
        this.history.redo(this.undoContext, new NullProgressMonitor(), null);
        assertEquals("hazard", this.model.getConvexHulls().get(0).getCollisionGroup());
        verify(this.view, times(7)).refreshEditingView();
    }

    /**
     * Use Case 1.1
     * @throws IOException
     */
    @Test
    public void testSuperMarioTileSet() throws IOException {
        loadEmptyFile();

        String tileSetFile = "test/mario_tileset.png";

        // set properties (1.1.1)

        propertySource.setPropertyValue("image", tileSetFile);
        assertEquals(tileSetFile, this.model.getImage());
        verify(this.view, times(2)).refreshProperties();

        propertySource.setPropertyValue("tileWidth", 16);
        assertEquals(16, this.model.getTileWidth());
        verify(this.view, times(3)).refreshProperties();

        propertySource.setPropertyValue("tileHeight", 16);
        assertEquals(16, this.model.getTileHeight());
        verify(this.view, times(4)).refreshProperties();

        propertySource.setPropertyValue("tileSpacing", 1);
        assertEquals(1, this.model.getTileSpacing());
        verify(this.view, times(5)).refreshProperties();

        assertEquals(20, this.model.getConvexHulls().size());

        for (TileSetModel.ConvexHull convexHull : this.model.getConvexHulls()) {
            assertEquals(0, convexHull.getCount());
        }

        propertySource.setPropertyValue("collision", tileSetFile);
        assertEquals(tileSetFile, this.model.getCollision());
        verify(this.view, times(6)).refreshProperties();

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
        verify(this.view, times(2)).refreshOutline();

        this.presenter.setConvexHullCollisionGroup(1, "hazard");
        assertEquals("hazard", this.model.getConvexHulls().get(1).getCollisionGroup());
        verify(this.view, times(2)).refreshEditingView();

        // set properties (1.1.5)

        this.presenter.renameCollisionGroup("tile", "obstruction");
        assertEquals("obstruction", this.model.getCollisionGroups().get(1));
        assertEquals("obstruction", this.model.getConvexHulls().get(0).getCollisionGroup());
        verify(this.view, times(3)).refreshOutline();
        verify(this.view, times(3)).refreshEditingView();

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
}
