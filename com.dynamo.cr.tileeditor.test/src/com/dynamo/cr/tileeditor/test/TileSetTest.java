package com.dynamo.cr.tileeditor.test;

import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.tileeditor.core.ITileSetView;
import com.dynamo.cr.tileeditor.core.TileSetModel;
import com.dynamo.cr.tileeditor.core.TileSetPresenter;
import com.dynamo.tile.proto.Tile.TileSet;

public class TileSetTest {
    ITileSetView view;
    TileSetModel model;
    TileSetPresenter presenter;

    @Before
    public void setup() {
        this.view = mock(ITileSetView.class);
        this.model = new TileSetModel();
        this.presenter = new TileSetPresenter(this.model, this.view);
    }

    /**
     * Use Case 1.1
     */
    @Test
    public void testSuperMarioTileSet() {
        // new file
        TileSet tileSet = TileSet.newBuilder()
                .setImage("")
                .setTileWidth(0)
                .setTileHeight(0)
                .setTileSpacing(0)
                .setTileMargin(0)
                .setCollision("")
                .setMaterialTag("tile")
                .build();
        this.presenter.load(tileSet);

        verify(this.view).setImage(eq(""));
        assertEquals("", this.model.getImage());
        verify(this.view).setTileWidth(eq(0));
        assertEquals(0, this.model.getTileWidth());
        verify(this.view).setTileHeight(eq(0));
        assertEquals(0, this.model.getTileHeight());
        verify(this.view).setTileMargin(eq(0));
        assertEquals(0, this.model.getTileMargin());
        verify(this.view).setTileSpacing(eq(0));
        assertEquals(0, this.model.getTileSpacing());
        verify(this.view).setCollision(eq(""));
        assertEquals("", this.model.getCollision());
        verify(this.view).setMaterialTag(eq("tile"));
        assertEquals("tile", this.model.getMaterialTag());

        // set properties
        this.presenter.setImage("test/mario_tileset.png");
        assertEquals("test/mario_tileset.png", this.model.getImage());
        this.presenter.setTileWidth(16);
        assertEquals(16, this.model.getTileWidth());
        this.presenter.setTileHeight(16);
        assertEquals(16, this.model.getTileHeight());
        this.presenter.setTileMargin(0);
        assertEquals(0, this.model.getTileMargin());
        this.presenter.setTileSpacing(1);
        assertEquals(1, this.model.getTileSpacing());

        assertEquals(20, this.model.getTiles().size());

        for (TileSetModel.Tile tile : this.model.getTiles()) {
            assertEquals(0, tile.getConvexHullCount());
        }
        this.presenter.setCollision("test/mario_tileset.png");
        assertEquals("test/mario_tileset.png", this.model.getCollision());
        for (int i = 0; i < 10; ++i) {
            assertEquals(8, this.model.getTiles().get(i).getConvexHullCount());
        }
        for (int i = 10; i < 14; ++i) {
            assertEquals(4, this.model.getTiles().get(i).getConvexHullCount());
        }
        for (int i = 14; i < 20; ++i) {
            assertEquals(8, this.model.getTiles().get(i).getConvexHullCount());
        }

        this.presenter.setTileCollisionGroup(1, "obstruction");
        assertEquals("obstruction", this.model.getTiles().get(1).getCollisionGroup());

        this.presenter.setTileCollisionGroup(2, "hazard");
        assertEquals("hazard", this.model.getTiles().get(2).getCollisionGroup());

    }
}
