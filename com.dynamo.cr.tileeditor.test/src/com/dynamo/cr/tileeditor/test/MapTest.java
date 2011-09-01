package com.dynamo.cr.tileeditor.test;

import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.tileeditor.core.IMapView;
import com.dynamo.cr.tileeditor.core.MapModel;
import com.dynamo.cr.tileeditor.core.MapPresenter;
import com.dynamo.tile.proto.Tile.TileMap;

public class MapTest {
    IMapView view;
    MapModel model;
    MapPresenter presenter;

    @Before
    public void setup() {
        this.view = mock(IMapView.class);
        this.model = new MapModel();
        this.presenter = new MapPresenter(this.model, this.view);
    }

    /**
     * Use Case 1.1
     */
    @Test
    public void testSuperMarioMap() {
        TileMap tileMap = TileMap.newBuilder()
                .setImage("")
                .setTileWidth(0)
                .setTileHeight(0)
                .setTileCount(0)
                .setTileSpacing(0)
                .setTileMargin(0)
                .setCollision("")
                .setMaterialTag("tile")
                .build();
        this.presenter.init(tileMap);

        verify(this.view).setImage(eq(""));
        verify(this.view).setTileWidth(eq(0));
        verify(this.view).setTileHeight(eq(0));
        verify(this.view).setTileCount(eq(0));
        verify(this.view).setTileMargin(eq(0));
        verify(this.view).setTileSpacing(eq(0));
        verify(this.view).setCollision(eq(""));
        verify(this.view).setMaterialTag(eq("tile"));
    }
}
