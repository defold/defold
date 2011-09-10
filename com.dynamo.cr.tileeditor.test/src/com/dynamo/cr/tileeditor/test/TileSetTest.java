package com.dynamo.cr.tileeditor.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyListOf;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;

import javax.vecmath.Vector3f;

import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.osgi.util.NLS;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.tileeditor.core.ITileSetView;
import com.dynamo.cr.tileeditor.core.Tag;
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
    IPropertyModel<TileSetModel, TileSetModel> propertyModel;

    @SuppressWarnings("unchecked")
    @Before
    public void setup() {
        System.setProperty("java.awt.headless", "true");
        this.view = mock(ITileSetView.class);
        this.history = new DefaultOperationHistory();
        this.undoContext = new UndoContext();
        this.model = new TileSetModel(this.history, this.undoContext);
        this.presenter = new TileSetPresenter(this.model, this.view);
        this.propertyModel = (IPropertyModel<TileSetModel, TileSetModel>) this.model.getAdapter(IPropertyModel.class);
    }

    private TileSet loadEmptyFile() throws IOException {
        // new file
        TileSet tileSet = TileSet.newBuilder()
                .setImage("")
                .setTileWidth(16)
                .setTileHeight(16)
                .setTileSpacing(0)
                .setTileMargin(0)
                .setCollision("")
                .setMaterialTag("tile")
                .addCollisionGroups("foreground")
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
        assertEquals(1, this.model.getCollisionGroups().size());
        assertEquals(tileSet.getCollisionGroups(0), this.model.getCollisionGroups().get(0));

        verify(this.view, times(1)).setImageProperty(any(String.class));
        verify(this.view, times(1)).setTileWidthProperty(anyInt());
        verify(this.view, times(1)).setTileHeightProperty(anyInt());
        verify(this.view, never()).setTileMarginProperty(anyInt());
        verify(this.view, never()).setTileSpacingProperty(anyInt());
        verify(this.view, times(1)).setCollisionProperty(any(String.class));
        verify(this.view, times(1)).setMaterialTagProperty(any(String.class));
        verify(this.view, times(1)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
    }

    /**
     * Use Case 1.1.1 - Create the Tile Set
     * 
     * @throws IOException
     */
    @Test
    public void testUseCase111() throws Exception {
        TileSet emptyTileSet = loadEmptyFile();

        String tileSetFile = "test/mario_tileset.png";

        // image

        assertEquals(emptyTileSet.getImage(), this.model.getImage());
        verify(this.view, times(1)).setImageProperty(any(String.class));
        this.model.executeOperation(propertyModel.setPropertyValue("image", tileSetFile));
        assertEquals(tileSetFile, this.model.getImage());
        verify(this.view, times(2)).setImageProperty(any(String.class));
        this.history.undo(this.undoContext, null, null);
        assertEquals(emptyTileSet.getImage(), this.model.getImage());
        verify(this.view, times(3)).setImageProperty(any(String.class));
        this.history.redo(this.undoContext, null, null);
        assertEquals(tileSetFile, this.model.getImage());
        verify(this.view, times(4)).setImageProperty(any(String.class));

        // tile width

        assertEquals(emptyTileSet.getTileWidth(), this.model.getTileWidth());
        verify(this.view, times(1)).setTileWidthProperty(anyInt());
        this.model.executeOperation(propertyModel.setPropertyValue("tileWidth", 17));
        assertEquals(17, this.model.getTileWidth());
        verify(this.view, times(2)).setTileWidthProperty(anyInt());
        this.history.undo(this.undoContext, null, null);
        assertEquals(emptyTileSet.getTileWidth(), this.model.getTileWidth());
        verify(this.view, times(3)).setTileWidthProperty(anyInt());
        this.history.redo(this.undoContext, null, null);
        assertEquals(17, this.model.getTileWidth());
        verify(this.view, times(4)).setTileWidthProperty(anyInt());
        this.history.undo(this.undoContext, null, null);

        // tile height

        assertEquals(emptyTileSet.getTileHeight(), this.model.getTileHeight());
        verify(this.view, times(1)).setTileHeightProperty(anyInt());
        this.model.executeOperation(propertyModel.setPropertyValue("tileHeight", 17));
        assertEquals(17, this.model.getTileHeight());
        verify(this.view, times(2)).setTileHeightProperty(anyInt());
        this.history.undo(this.undoContext, null, null);
        assertEquals(emptyTileSet.getTileHeight(), this.model.getTileHeight());
        verify(this.view, times(3)).setTileHeightProperty(anyInt());
        this.history.redo(this.undoContext, null, null);
        assertEquals(17, this.model.getTileHeight());
        verify(this.view, times(4)).setTileHeightProperty(anyInt());
        this.history.undo(this.undoContext, null, null);

        // tile margin

        assertEquals(emptyTileSet.getTileMargin(), this.model.getTileMargin());
        verify(this.view, never()).setTileMarginProperty(anyInt());
        this.model.executeOperation(propertyModel.setPropertyValue("tileMargin", 1));
        assertEquals(1, this.model.getTileMargin());
        verify(this.view, times(1)).setTileMarginProperty(anyInt());
        this.history.undo(this.undoContext, null, null);
        assertEquals(0, this.model.getTileMargin());
        verify(this.view, times(2)).setTileMarginProperty(anyInt());
        this.history.redo(this.undoContext, null, null);
        assertEquals(1, this.model.getTileMargin());
        verify(this.view, times(3)).setTileMarginProperty(anyInt());

        // reset since we don't want any tile margin
        this.history.undo(this.undoContext, null, null);

        // tile spacing

        assertEquals(emptyTileSet.getTileSpacing(), this.model.getTileSpacing());
        verify(this.view, never()).setTileSpacingProperty(anyInt());
        this.model.executeOperation(propertyModel.setPropertyValue("tileSpacing", 1));
        assertEquals(1, this.model.getTileSpacing());
        verify(this.view, times(1)).setTileSpacingProperty(anyInt());
        this.history.undo(this.undoContext, null, null);
        assertEquals(0, this.model.getTileSpacing());
        verify(this.view, times(2)).setTileSpacingProperty(anyInt());
        this.history.redo(this.undoContext, null, null);
        assertEquals(1, this.model.getTileSpacing());
        verify(this.view, times(3)).setTileSpacingProperty(anyInt());

        // collision

        assertEquals(20, this.model.getConvexHulls().size());
        for (TileSetModel.ConvexHull convexHull : this.model.getConvexHulls()) {
            assertEquals(0, convexHull.getCount());
        }
        assertEquals(emptyTileSet.getCollision(), this.model.getCollision());
        verify(this.view, times(1)).setCollisionProperty(any(String.class));
        this.model.executeOperation(propertyModel.setPropertyValue("collision", tileSetFile));
        assertEquals(tileSetFile, this.model.getCollision());
        verify(this.view, times(2)).setCollisionProperty(any(String.class));
        this.history.undo(this.undoContext, null, null);
        assertEquals(emptyTileSet.getCollision(), this.model.getCollision());
        verify(this.view, times(3)).setCollisionProperty(any(String.class));
        this.history.redo(this.undoContext, null, null);

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

        // material tag

        assertEquals(emptyTileSet.getMaterialTag(), this.model.getMaterialTag());
        verify(this.view, times(1)).setMaterialTagProperty(any(String.class));
        this.model.executeOperation(propertyModel.setPropertyValue("materialTag", "my_material"));
        assertEquals("my_material", this.model.getMaterialTag());
        verify(this.view, times(2)).setMaterialTagProperty(any(String.class));
        this.history.undo(this.undoContext, null, null);
        assertEquals(emptyTileSet.getMaterialTag(), this.model.getMaterialTag());
        verify(this.view, times(3)).setMaterialTagProperty(any(String.class));
        this.history.redo(this.undoContext, null, null);
        assertEquals("my_material", this.model.getMaterialTag());
        verify(this.view, times(4)).setMaterialTagProperty(any(String.class));

        // reset since we don't want to edit material tag
        this.history.undo(this.undoContext, null, null);
    }

    /**
     * Use Case 1.1.4 - Add a Collision Group
     * 
     * @throws IOException
     */
    @Test
    public void testUseCase114() throws Exception {

        // requirement
        testUseCase111();

        // preconditions
        assertEquals(1, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals(0, this.model.getSelectedCollisionGroups().length);
        verify(this.view, times(1)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));

        // add the group
        this.presenter.addCollisionGroup("hazad");
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("hazad", this.model.getCollisionGroups().get(1));
        assertEquals(1, this.model.getSelectedCollisionGroups().length);
        assertEquals("hazad", this.model.getSelectedCollisionGroups()[0]);
        verify(this.view, times(2)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        this.history.undo(this.undoContext, null, null);
        assertEquals(1, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals(0, this.model.getSelectedCollisionGroups().length);
        verify(this.view, times(3)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        this.history.redo(this.undoContext, null, null);
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("hazad", this.model.getCollisionGroups().get(1));
        assertEquals(1, this.model.getSelectedCollisionGroups().length);
        assertEquals("hazad", this.model.getSelectedCollisionGroups()[0]);
        verify(this.view, times(4)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));

        // preconditions
        assertEquals("", this.model.getConvexHulls().get(1).getCollisionGroup());
        assertEquals("", this.model.getConvexHulls().get(2).getCollisionGroup());
        verify(this.view, never()).setTileHullColor(eq(1), any(Color.class));
        verify(this.view, never()).setTileHullColor(eq(2), any(Color.class));

        // simulate painting
        this.presenter.beginSetConvexHullCollisionGroup("hazad");
        this.presenter.setConvexHullCollisionGroup(1);
        this.presenter.setConvexHullCollisionGroup(1);
        this.presenter.setConvexHullCollisionGroup(2);
        this.presenter.setConvexHullCollisionGroup(2);
        this.presenter.endSetConvexHullCollisionGroup();
        assertEquals("hazad", this.model.getConvexHulls().get(1).getCollisionGroup());
        assertEquals("hazad", this.model.getConvexHulls().get(2).getCollisionGroup());
        verify(this.view, times(1)).setTileHullColor(eq(1), any(Color.class));
        verify(this.view, times(1)).setTileHullColor(eq(2), any(Color.class));
        this.history.undo(this.undoContext, null, null);
        assertEquals("", this.model.getConvexHulls().get(1).getCollisionGroup());
        assertEquals("", this.model.getConvexHulls().get(2).getCollisionGroup());
        verify(this.view, times(2)).setTileHullColor(eq(1), any(Color.class));
        verify(this.view, times(2)).setTileHullColor(eq(2), any(Color.class));
        this.history.redo(this.undoContext, null, null);
        assertEquals("hazad", this.model.getConvexHulls().get(1).getCollisionGroup());
        assertEquals("hazad", this.model.getConvexHulls().get(2).getCollisionGroup());
        verify(this.view, times(3)).setTileHullColor(eq(1), any(Color.class));
        verify(this.view, times(3)).setTileHullColor(eq(2), any(Color.class));
    }

    /**
     * Use Case 1.1.5 - Rename Collision Group
     * 
     * @throws IOException
     */
    @Test
    public void testUseCase115() throws Exception {

        // requirement
        testUseCase114();

        // preconditions
        assertEquals("hazad", this.model.getCollisionGroups().get(1));
        assertEquals("hazad", this.model.getConvexHulls().get(1).getCollisionGroup());
        verify(this.view, times(4)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(3)).setTileHullColor(eq(1), any(Color.class));

        this.presenter.selectCollisionGroups(new String[] {"hazad"});
        assertEquals(1, this.model.getSelectedCollisionGroups().length);
        assertEquals("hazad", this.model.getSelectedCollisionGroups()[0]);

        // test
        this.presenter.renameSelectedCollisionGroups(new String[] {"hazard"});
        assertEquals("hazard", this.model.getCollisionGroups().get(1));
        assertEquals("hazard", this.model.getConvexHulls().get(1).getCollisionGroup());
        verify(this.view, times(5)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(4)).setTileHullColor(eq(1), any(Color.class));
        this.history.undo(this.undoContext, null, null);
        assertEquals("hazad", this.model.getCollisionGroups().get(1));
        assertEquals("hazad", this.model.getConvexHulls().get(1).getCollisionGroup());
        verify(this.view, times(6)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(5)).setTileHullColor(eq(1), any(Color.class));
        this.history.redo(this.undoContext, null, null);
        assertEquals("hazard", this.model.getCollisionGroups().get(1));
        assertEquals("hazard", this.model.getConvexHulls().get(1).getCollisionGroup());
        verify(this.view, times(7)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(6)).setTileHullColor(eq(1), any(Color.class));
    }

    /**
     * Use Case 1.1.6 - Rename Collision Group to Existing Group
     * 
     * @throws IOException
     */
    @Test
    public void testUseCase116() throws Exception {

        // requirements
        testUseCase111();

        // add group
        this.presenter.addCollisionGroup("obstruction");

        // paint
        this.presenter.beginSetConvexHullCollisionGroup("obstruction");
        this.presenter.setConvexHullCollisionGroup(2);
        this.presenter.endSetConvexHullCollisionGroup();

        // add another group
        this.presenter.addCollisionGroup("obstuction");

        // paint
        this.presenter.beginSetConvexHullCollisionGroup("obstuction");
        this.presenter.setConvexHullCollisionGroup(3);
        this.presenter.endSetConvexHullCollisionGroup();

        // preconditions
        assertEquals(3, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("obstruction", this.model.getCollisionGroups().get(1));
        assertEquals("obstuction", this.model.getCollisionGroups().get(2));
        assertEquals("obstruction", this.model.getConvexHulls().get(2).getCollisionGroup());
        assertEquals("obstuction", this.model.getConvexHulls().get(3).getCollisionGroup());
        assertEquals(1, this.model.getSelectedCollisionGroups().length);
        assertEquals("obstuction", this.model.getSelectedCollisionGroups()[0]);
        verify(this.view, times(3)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(1)).setTileHullColor(eq(2), any(Color.class));
        verify(this.view, times(1)).setTileHullColor(eq(3), any(Color.class));

        // test
        this.presenter.renameSelectedCollisionGroups(new String[] {"obstruction"});
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("obstruction", this.model.getCollisionGroups().get(1));
        assertEquals("obstruction", this.model.getConvexHulls().get(2).getCollisionGroup());
        assertEquals("obstruction", this.model.getConvexHulls().get(3).getCollisionGroup());
        assertEquals(1, this.model.getSelectedCollisionGroups().length);
        assertEquals("obstruction", this.model.getSelectedCollisionGroups()[0]);
        verify(this.view, times(4)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(2)).setTileHullColor(eq(3), any(Color.class));

        // undo
        this.history.undo(this.undoContext, null, null);
        assertEquals(3, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("obstruction", this.model.getCollisionGroups().get(1));
        assertEquals("obstuction", this.model.getCollisionGroups().get(2));
        assertEquals("obstruction", this.model.getConvexHulls().get(2).getCollisionGroup());
        assertEquals("obstuction", this.model.getConvexHulls().get(3).getCollisionGroup());
        assertEquals(1, this.model.getSelectedCollisionGroups().length);
        assertEquals("obstuction", this.model.getSelectedCollisionGroups()[0]);
        verify(this.view, times(5)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(3)).setTileHullColor(eq(3), any(Color.class));

        // redo
        this.history.redo(this.undoContext, null, null);
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("obstruction", this.model.getCollisionGroups().get(1));
        assertEquals("obstruction", this.model.getConvexHulls().get(2).getCollisionGroup());
        assertEquals("obstruction", this.model.getConvexHulls().get(3).getCollisionGroup());
        assertEquals(1, this.model.getSelectedCollisionGroups().length);
        assertEquals("obstruction", this.model.getSelectedCollisionGroups()[0]);
        verify(this.view, times(6)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(4)).setTileHullColor(eq(3), any(Color.class));
    }

    /**
     * Use Case 1.1.7 - Rename Collision Groups to Single Group
     * 
     * @throws IOException
     */
    @Test
    public void testUseCase117() throws Exception {

        // requirements
        testUseCase111();

        // add group
        this.presenter.addCollisionGroup("obstruction");

        // paint
        this.presenter.beginSetConvexHullCollisionGroup("obstruction");
        this.presenter.setConvexHullCollisionGroup(2);
        this.presenter.endSetConvexHullCollisionGroup();

        // add another group
        this.presenter.addCollisionGroup("obstuction");

        // paint
        this.presenter.beginSetConvexHullCollisionGroup("obstuction");
        this.presenter.setConvexHullCollisionGroup(3);
        this.presenter.endSetConvexHullCollisionGroup();

        this.presenter.selectCollisionGroups(new String[] {"obstuction", "obstruction"});

        // preconditions
        assertEquals(3, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("obstruction", this.model.getCollisionGroups().get(1));
        assertEquals("obstuction", this.model.getCollisionGroups().get(2));
        assertEquals("obstruction", this.model.getConvexHulls().get(2).getCollisionGroup());
        assertEquals("obstuction", this.model.getConvexHulls().get(3).getCollisionGroup());
        assertEquals(2, this.model.getSelectedCollisionGroups().length);
        assertEquals("obstuction", this.model.getSelectedCollisionGroups()[0]);
        assertEquals("obstruction", this.model.getSelectedCollisionGroups()[1]);
        verify(this.view, times(3)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(1)).setTileHullColor(eq(2), any(Color.class));
        verify(this.view, times(1)).setTileHullColor(eq(3), any(Color.class));

        // test
        this.presenter.renameSelectedCollisionGroups(new String[] {"obstruction2", "obstruction2"});
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("obstruction2", this.model.getCollisionGroups().get(1));
        assertEquals("obstruction2", this.model.getConvexHulls().get(2).getCollisionGroup());
        assertEquals("obstruction2", this.model.getConvexHulls().get(3).getCollisionGroup());
        assertEquals(1, this.model.getSelectedCollisionGroups().length);
        assertEquals("obstruction2", this.model.getSelectedCollisionGroups()[0]);
        verify(this.view, times(4)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(2)).setTileHullColor(eq(3), any(Color.class));

        // undo
        this.history.undo(this.undoContext, null, null);
        assertEquals(3, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("obstruction", this.model.getCollisionGroups().get(1));
        assertEquals("obstuction", this.model.getCollisionGroups().get(2));
        assertEquals("obstruction", this.model.getConvexHulls().get(2).getCollisionGroup());
        assertEquals("obstuction", this.model.getConvexHulls().get(3).getCollisionGroup());
        assertEquals(2, this.model.getSelectedCollisionGroups().length);
        assertEquals("obstuction", this.model.getSelectedCollisionGroups()[0]);
        assertEquals("obstruction", this.model.getSelectedCollisionGroups()[1]);
        verify(this.view, times(6)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(3)).setTileHullColor(eq(3), any(Color.class));

        // redo
        this.history.redo(this.undoContext, null, null);
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("obstruction2", this.model.getCollisionGroups().get(1));
        assertEquals("obstruction2", this.model.getConvexHulls().get(2).getCollisionGroup());
        assertEquals("obstruction2", this.model.getConvexHulls().get(3).getCollisionGroup());
        assertEquals(1, this.model.getSelectedCollisionGroups().length);
        assertEquals("obstruction2", this.model.getSelectedCollisionGroups()[0]);
        verify(this.view, times(7)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(4)).setTileHullColor(eq(3), any(Color.class));
    }

    /**
     * Use Case 1.1.9 - Remove Collision Group
     * 
     * @throws IOException
     */
    @Test
    public void testUseCase119() throws Exception {

        // requirements
        testUseCase114();

        // add group, to test group order
        this.presenter.addCollisionGroup("obstruction");

        // preconditions
        assertEquals(3, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("hazad", this.model.getCollisionGroups().get(1));
        assertEquals("obstruction", this.model.getCollisionGroups().get(2));
        assertEquals("hazad", this.model.getConvexHulls().get(1).getCollisionGroup());
        assertEquals(1, this.model.getSelectedCollisionGroups().length);
        assertEquals("obstruction", this.model.getSelectedCollisionGroups()[0]);
        verify(this.view, times(5)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(3)).setTileHullColor(eq(1), any(Color.class));

        this.presenter.selectCollisionGroups(new String[] {"hazad"});
        assertEquals(1, this.model.getSelectedCollisionGroups().length);
        assertEquals("hazad", this.model.getSelectedCollisionGroups()[0]);
        this.presenter.removeSelectedCollisionGroups();
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("obstruction", this.model.getCollisionGroups().get(1));
        assertEquals("", this.model.getConvexHulls().get(1).getCollisionGroup());
        assertEquals(0, this.model.getSelectedCollisionGroups().length);
        verify(this.view, times(6)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(4)).setTileHullColor(eq(1), any(Color.class));

        this.history.undo(this.undoContext, null, null);
        assertEquals(3, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("hazad", this.model.getCollisionGroups().get(1));
        assertEquals("obstruction", this.model.getCollisionGroups().get(2));
        assertEquals("hazad", this.model.getConvexHulls().get(1).getCollisionGroup());
        assertEquals(1, this.model.getSelectedCollisionGroups().length);
        assertEquals("hazad", this.model.getSelectedCollisionGroups()[0]);
        verify(this.view, times(7)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(5)).setTileHullColor(eq(1), any(Color.class));

        this.history.redo(this.undoContext, null, null);
        assertEquals(2, this.model.getCollisionGroups().size());
        assertEquals("foreground", this.model.getCollisionGroups().get(0));
        assertEquals("obstruction", this.model.getCollisionGroups().get(1));
        assertEquals("", this.model.getConvexHulls().get(1).getCollisionGroup());
        assertEquals(0, this.model.getSelectedCollisionGroups().length);
        verify(this.view, times(8)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(6)).setTileHullColor(eq(1), any(Color.class));
    }

    /**
     * Use Case 1.1.8 - Save
     * 
     * @throws IOException
     */
    @Test
    public void testUseCase118() throws Exception {

        // requires
        testUseCase114();

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
     * Message 1.1 - Image not specified
     * @throws IOException
     */
    @Test
    public void testMessage11() throws IOException {
        loadEmptyFile();

        assertTrue(this.model.hasPropertyAnnotation("image", TileSetModel.TAG_1));
        assertEquals(TileSetModel.TAG_1.getMessage(), this.model.getPropertyTag("image", TileSetModel.TAG_1).getMessage());
        verify(this.view, times(1)).setImageTags(anyListOf(Tag.class));

        this.model.executeOperation(propertyModel.setPropertyValue("image", "test/mario_tileset.png"));
        assertTrue(!this.model.hasPropertyAnnotation("image", TileSetModel.TAG_1));
        verify(this.view, times(2)).setImageTags(anyListOf(Tag.class));
    }

    /**
     * Message 1.2- Image not found
     * @throws IOException
     */
    @Test
    public void testMessage12() throws IOException {
        loadEmptyFile();

        assertTrue(!this.model.hasPropertyAnnotation("image", TileSetModel.TAG_2));

        String invalidPath = "test";
        this.model.executeOperation(propertyModel.setPropertyValue("image", invalidPath));
        assertTrue(this.model.hasPropertyAnnotation("image", TileSetModel.TAG_2));
        assertEquals(NLS.bind(TileSetModel.TAG_2.getMessage(), invalidPath), this.model.getPropertyTag("image", TileSetModel.TAG_2).getMessage());
        verify(this.view, times(3)).setImageTags(anyListOf(Tag.class));

        this.model.executeOperation(propertyModel.setPropertyValue("image", "test/mario_tileset.png"));
        assertTrue(!this.model.hasPropertyAnnotation("image", TileSetModel.TAG_2));
        verify(this.view, times(4)).setImageTags(anyListOf(Tag.class));
    }

    /**
     * Message 1.3 - Collision image not found
     * @throws IOException
     */
    @Test
    public void testMessage13() throws IOException {
        loadEmptyFile();

        assertTrue(!this.model.hasPropertyAnnotation("collision", TileSetModel.TAG_3));

        String invalidPath = "test";
        this.model.executeOperation(propertyModel.setPropertyValue("collision", invalidPath));
        assertTrue(this.model.hasPropertyAnnotation("collision", TileSetModel.TAG_3));
        assertEquals(NLS.bind(TileSetModel.TAG_3.getMessage(), invalidPath), this.model.getPropertyTag("collision", TileSetModel.TAG_3).getMessage());
        verify(this.view, times(1)).setCollisionTags(anyListOf(Tag.class));

        this.model.executeOperation(propertyModel.setPropertyValue("collision", "test/mario_tileset.png"));
        assertTrue(!this.model.hasPropertyAnnotation("collision", TileSetModel.TAG_3));
        verify(this.view, times(2)).setCollisionTags(anyListOf(Tag.class));
    }

    /**
     * Message 1.4 - Image and collision image have different dimensions
     * @throws IOException
     */
    @Test
    public void testMessage14() throws IOException {
        loadEmptyFile();

        assertTrue(!this.model.hasPropertyAnnotation("image", TileSetModel.TAG_4));
        assertTrue(!this.model.hasPropertyAnnotation("collision", TileSetModel.TAG_4));

        this.model.executeOperation(propertyModel.setPropertyValue("image", "test/mario_tileset.png"));
        this.model.executeOperation(propertyModel.setPropertyValue("collision", "test/mario_half_tileset.png"));
        assertTrue(this.model.hasPropertyAnnotation("image", TileSetModel.TAG_4));
        String message = NLS.bind(TileSetModel.TAG_4.getMessage(), new Object[] {84, 67, 84, 33});
        assertEquals(message, this.model.getPropertyTag("image", TileSetModel.TAG_4).getMessage());
        assertTrue(this.model.hasPropertyAnnotation("collision", TileSetModel.TAG_4));
        assertEquals(message, this.model.getPropertyTag("collision", TileSetModel.TAG_4).getMessage());
        verify(this.view, times(3)).setImageTags(anyListOf(Tag.class));
        verify(this.view, times(1)).setCollisionTags(anyListOf(Tag.class));

        this.model.executeOperation(propertyModel.setPropertyValue("collision", "test/mario_tileset.png"));
        assertTrue(!this.model.hasPropertyAnnotation("image", TileSetModel.TAG_4));
        assertTrue(!this.model.hasPropertyAnnotation("collision", TileSetModel.TAG_4));
        verify(this.view, times(4)).setImageTags(anyListOf(Tag.class));
        verify(this.view, times(2)).setCollisionTags(anyListOf(Tag.class));
    }

    /**
     * Message 1.5 - Invalid tile width
     * @throws IOException
     */
    @Test
    public void testMessage15() throws IOException {
        loadEmptyFile();

        assertTrue(!this.model.hasPropertyAnnotation("tileWidth", TileSetModel.TAG_5));

        this.model.executeOperation(propertyModel.setPropertyValue("tileWidth", 0));
        assertTrue(this.model.hasPropertyAnnotation("tileWidth", TileSetModel.TAG_5));
        assertEquals(TileSetModel.TAG_5.getMessage(), this.model.getPropertyTag("tileWidth", TileSetModel.TAG_5).getMessage());
        verify(this.view, times(1)).setTileWidthTags(anyListOf(Tag.class));

        this.model.executeOperation(propertyModel.setPropertyValue("tileWidth", 16));
        assertTrue(!this.model.hasPropertyAnnotation("tileWidth", TileSetModel.TAG_5));
        verify(this.view, times(2)).setTileWidthTags(anyListOf(Tag.class));
    }

    /**
     * Message 1.6 - Invalid tile height
     * @throws IOException
     */
    @Test
    public void testMessage16() throws IOException {
        loadEmptyFile();

        assertTrue(!this.model.hasPropertyAnnotation("tileHeight", TileSetModel.TAG_6));

        this.model.executeOperation(propertyModel.setPropertyValue("tileHeight", 0));
        assertTrue(this.model.hasPropertyAnnotation("tileHeight", TileSetModel.TAG_6));
        assertEquals(TileSetModel.TAG_6.getMessage(), this.model.getPropertyTag("tileHeight", TileSetModel.TAG_6).getMessage());
        verify(this.view, times(1)).setTileHeightTags(anyListOf(Tag.class));

        this.model.executeOperation(propertyModel.setPropertyValue("tileHeight", 16));
        assertTrue(!this.model.hasPropertyAnnotation("tileHeight", TileSetModel.TAG_6));
        verify(this.view, times(2)).setTileHeightTags(anyListOf(Tag.class));
    }

    /**
     * Message 1.7 - Total tile width is greater than image width
     * @throws IOException
     */
    @Test
    public void testMessage17() throws IOException {
        loadEmptyFile();

        this.model.executeOperation(propertyModel.setPropertyValue("image", "test/mario_tileset.png"));

        assertTrue(!this.model.hasPropertyAnnotation("image", TileSetModel.TAG_7));
        assertTrue(!this.model.hasPropertyAnnotation("tileWidth", TileSetModel.TAG_7));

        String message = NLS.bind(TileSetModel.TAG_7.getMessage(), new Object[] {86, 84});
        this.model.executeOperation(propertyModel.setPropertyValue("tileWidth", 85));
        this.model.executeOperation(propertyModel.setPropertyValue("tileMargin", 1));
        assertTrue(this.model.hasPropertyAnnotation("image", TileSetModel.TAG_7));
        assertEquals(message, this.model.getPropertyTag("image", TileSetModel.TAG_7).getMessage());
        verify(this.view, times(4)).setImageTags(anyListOf(Tag.class));
        assertTrue(this.model.hasPropertyAnnotation("tileWidth", TileSetModel.TAG_7));
        assertEquals(message, this.model.getPropertyTag("tileWidth", TileSetModel.TAG_7).getMessage());
        verify(this.view, times(2)).setTileWidthTags(anyListOf(Tag.class));
        assertTrue(this.model.hasPropertyAnnotation("tileMargin", TileSetModel.TAG_7));
        assertEquals(message, this.model.getPropertyTag("tileMargin", TileSetModel.TAG_7).getMessage());
        verify(this.view, times(1)).setTileMarginTags(anyListOf(Tag.class));

        message = NLS.bind(TileSetModel.TAG_7.getMessage(), new Object[] {85, 84});
        this.model.executeOperation(propertyModel.setPropertyValue("tileMargin", 0));
        assertTrue(this.model.hasPropertyAnnotation("image", TileSetModel.TAG_7));
        assertEquals(message, this.model.getPropertyTag("image", TileSetModel.TAG_7).getMessage());
        verify(this.view, times(5)).setImageTags(anyListOf(Tag.class));
        assertTrue(this.model.hasPropertyAnnotation("tileWidth", TileSetModel.TAG_7));
        assertEquals(message, this.model.getPropertyTag("tileWidth", TileSetModel.TAG_7).getMessage());
        verify(this.view, times(3)).setTileWidthTags(anyListOf(Tag.class));
        assertTrue(!this.model.hasPropertyAnnotation("tileMargin", TileSetModel.TAG_7));
        verify(this.view, times(2)).setTileMarginTags(anyListOf(Tag.class));

        this.model.executeOperation(propertyModel.setPropertyValue("tileWidth", 16));
        assertTrue(!this.model.hasPropertyAnnotation("image", TileSetModel.TAG_7));
        verify(this.view, times(6)).setImageTags(anyListOf(Tag.class));
        assertTrue(!this.model.hasPropertyAnnotation("tileWidth", TileSetModel.TAG_7));
        verify(this.view, times(4)).setTileWidthTags(anyListOf(Tag.class));
        assertTrue(!this.model.hasPropertyAnnotation("tileMargin", TileSetModel.TAG_7));
        verify(this.view, times(2)).setTileMarginTags(anyListOf(Tag.class));
    }

    /**
     * Message 1.8 - Total tile height is greater than image height
     * @throws IOException
     */
    @Test
    public void testMessage18() throws IOException {
        loadEmptyFile();

        this.model.executeOperation(propertyModel.setPropertyValue("image", "test/mario_tileset.png"));

        assertTrue(!this.model.hasPropertyAnnotation("tileHeight", TileSetModel.TAG_8));
        assertTrue(!this.model.hasPropertyAnnotation("tileWidth", TileSetModel.TAG_8));

        String message = NLS.bind(TileSetModel.TAG_8.getMessage(), new Object[] {69, 67});
        this.model.executeOperation(propertyModel.setPropertyValue("tileHeight", 68));
        this.model.executeOperation(propertyModel.setPropertyValue("tileMargin", 1));
        assertTrue(this.model.hasPropertyAnnotation("image", TileSetModel.TAG_8));
        assertEquals(message, this.model.getPropertyTag("image", TileSetModel.TAG_8).getMessage());
        verify(this.view, times(4)).setImageTags(anyListOf(Tag.class));
        assertTrue(this.model.hasPropertyAnnotation("tileHeight", TileSetModel.TAG_8));
        assertEquals(message, this.model.getPropertyTag("tileHeight", TileSetModel.TAG_8).getMessage());
        verify(this.view, times(2)).setTileHeightTags(anyListOf(Tag.class));
        assertTrue(this.model.hasPropertyAnnotation("tileMargin", TileSetModel.TAG_8));
        assertEquals(message, this.model.getPropertyTag("tileMargin", TileSetModel.TAG_8).getMessage());
        verify(this.view, times(1)).setTileMarginTags(anyListOf(Tag.class));

        message = NLS.bind(TileSetModel.TAG_8.getMessage(), new Object[] {68, 67});
        this.model.executeOperation(propertyModel.setPropertyValue("tileMargin", 0));
        assertTrue(this.model.hasPropertyAnnotation("image", TileSetModel.TAG_8));
        assertEquals(message, this.model.getPropertyTag("image", TileSetModel.TAG_8).getMessage());
        verify(this.view, times(5)).setImageTags(anyListOf(Tag.class));
        assertTrue(this.model.hasPropertyAnnotation("tileHeight", TileSetModel.TAG_8));
        assertEquals(message, this.model.getPropertyTag("tileHeight", TileSetModel.TAG_8).getMessage());
        verify(this.view, times(3)).setTileHeightTags(anyListOf(Tag.class));
        assertTrue(!this.model.hasPropertyAnnotation("tileMargin", TileSetModel.TAG_8));
        verify(this.view, times(2)).setTileMarginTags(anyListOf(Tag.class));

        this.model.executeOperation(propertyModel.setPropertyValue("tileHeight", 16));
        assertTrue(!this.model.hasPropertyAnnotation("image", TileSetModel.TAG_8));
        verify(this.view, times(6)).setImageTags(anyListOf(Tag.class));
        assertTrue(!this.model.hasPropertyAnnotation("tileHeight", TileSetModel.TAG_8));
        verify(this.view, times(4)).setTileHeightTags(anyListOf(Tag.class));
        assertTrue(!this.model.hasPropertyAnnotation("tileMargin", TileSetModel.TAG_8));
        verify(this.view, times(2)).setTileMarginTags(anyListOf(Tag.class));
    }

    /**
     * Message 1.9 - Empty material tag
     * @throws IOException
     */
    @Test
    public void testMessage19() throws IOException {
        loadEmptyFile();

        assertTrue(!this.model.hasPropertyAnnotation("materialTag", TileSetModel.TAG_9));

        this.model.executeOperation(propertyModel.setPropertyValue("materialTag", ""));
        assertTrue(this.model.hasPropertyAnnotation("materialTag", TileSetModel.TAG_9));
        assertEquals(TileSetModel.TAG_9.getMessage(), this.model.getPropertyTag("materialTag", TileSetModel.TAG_9).getMessage());
        verify(this.view, times(1)).setMaterialTagTags(anyListOf(Tag.class));

        this.model.executeOperation(propertyModel.setPropertyValue("materialTag", "tile"));
        assertTrue(!this.model.hasPropertyAnnotation("materialTag", TileSetModel.TAG_9));
        verify(this.view, times(2)).setMaterialTagTags(anyListOf(Tag.class));
    }

    /**
     * Message 1.10 - Invalid tile margin
     * @throws IOException
     */
    @Test
    public void testMessage110() throws IOException {
        loadEmptyFile();

        assertTrue(!this.model.hasPropertyAnnotation("tileMargin", TileSetModel.TAG_10));

        this.model.executeOperation(propertyModel.setPropertyValue("tileMargin", -1));
        assertTrue(this.model.hasPropertyAnnotation("tileMargin", TileSetModel.TAG_10));
        assertEquals(TileSetModel.TAG_10.getMessage(), this.model.getPropertyTag("tileMargin", TileSetModel.TAG_10).getMessage());
        verify(this.view, times(1)).setTileMarginTags(anyListOf(Tag.class));

        this.model.executeOperation(propertyModel.setPropertyValue("tileMargin", 0));
        assertTrue(!this.model.hasPropertyAnnotation("tileMargin", TileSetModel.TAG_10));
        verify(this.view, times(2)).setTileMarginTags(anyListOf(Tag.class));
    }

    /**
     * Message 1.11 - Invalid tile spacing
     * @throws IOException
     */
    @Test
    public void testMessage111() throws IOException {
        loadEmptyFile();

        assertTrue(!this.model.hasPropertyAnnotation("tileSpacing", TileSetModel.TAG_11));

        this.model.executeOperation(propertyModel.setPropertyValue("tileSpacing", -1));
        assertTrue(this.model.hasPropertyAnnotation("tileSpacing", TileSetModel.TAG_11));
        assertEquals(TileSetModel.TAG_11.getMessage(), this.model.getPropertyTag("tileSpacing", TileSetModel.TAG_11).getMessage());
        verify(this.view, times(1)).setTileSpacingTags(anyListOf(Tag.class));

        this.model.executeOperation(propertyModel.setPropertyValue("tileSpacing", 0));
        assertTrue(!this.model.hasPropertyAnnotation("tileSpacing", TileSetModel.TAG_11));
        verify(this.view, times(2)).setTileSpacingTags(anyListOf(Tag.class));
    }

    /**
     * Refresh
     * @throws IOException
     */
    @Test
    public void testRefresh() throws Exception {
        // requirement
        testUseCase114();

        verify(this.view, times(4)).setImageProperty(any(String.class));
        verify(this.view, times(4)).setImageTags(anyListOf(Tag.class));
        verify(this.view, times(5)).setTileWidthProperty(anyInt());
        verify(this.view, never()).setTileWidthTags(anyListOf(Tag.class));
        verify(this.view, times(5)).setTileHeightProperty(anyInt());
        verify(this.view, never()).setTileHeightTags(anyListOf(Tag.class));
        verify(this.view, times(4)).setTileMarginProperty(anyInt());
        verify(this.view, never()).setTileMarginTags(anyListOf(Tag.class));
        verify(this.view, times(3)).setTileSpacingProperty(anyInt());
        verify(this.view, never()).setTileSpacingTags(anyListOf(Tag.class));
        verify(this.view, times(4)).setCollisionProperty(any(String.class));
        verify(this.view, never()).setCollisionTags(anyListOf(Tag.class));
        verify(this.view, times(5)).setMaterialTagProperty(any(String.class));
        verify(this.view, never()).setMaterialTagTags(anyListOf(Tag.class));
        verify(this.view, times(4)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(13)).setTiles(any(BufferedImage.class), any(float[].class), any(int[].class), any(int[].class), any(Color[].class), any(Vector3f.class));
        verify(this.view, never()).clearTiles();
        verify(this.view, times(6)).setTileHullColor(anyInt(), any(Color.class));

        this.presenter.refresh();

        verify(this.view, times(5)).setImageProperty(any(String.class));
        verify(this.view, times(5)).setImageTags(anyListOf(Tag.class));
        verify(this.view, times(6)).setTileWidthProperty(anyInt());
        verify(this.view, times(1)).setTileWidthTags(anyListOf(Tag.class));
        verify(this.view, times(6)).setTileHeightProperty(anyInt());
        verify(this.view, times(1)).setTileHeightTags(anyListOf(Tag.class));
        verify(this.view, times(5)).setTileMarginProperty(anyInt());
        verify(this.view, times(1)).setTileMarginTags(anyListOf(Tag.class));
        verify(this.view, times(4)).setTileSpacingProperty(anyInt());
        verify(this.view, times(1)).setTileSpacingTags(anyListOf(Tag.class));
        verify(this.view, times(5)).setCollisionProperty(any(String.class));
        verify(this.view, times(1)).setCollisionTags(anyListOf(Tag.class));
        verify(this.view, times(6)).setMaterialTagProperty(any(String.class));
        verify(this.view, times(1)).setMaterialTagTags(anyListOf(Tag.class));
        verify(this.view, times(5)).setCollisionGroups(anyListOf(String.class), anyListOf(Color.class), any(String[].class));
        verify(this.view, times(14)).setTiles(any(BufferedImage.class), any(float[].class), any(int[].class), any(int[].class), any(Color[].class), any(Vector3f.class));
        verify(this.view, never()).clearTiles();
        verify(this.view, times(6)).setTileHullColor(anyInt(), any(Color.class));
    }
}
