package com.dynamo.cr.tileeditor.core;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;

import com.dynamo.tile.proto.Tile.TileSet;


public class TileSetModel extends Model {

    public static final String KEY_IMAGE = "image";
    public static final String KEY_TILE_WIDTH = "tile width";
    public static final String KEY_TILE_HEIGHT = "tile height";
    public static final String KEY_TILE_MARGIN = "tile margin";
    public static final String KEY_TILE_SPACING = "tile spacing";
    public static final String KEY_COLLISION = "collision";
    public static final String KEY_MATERIAL_TAG = "material tag";

    List<Tile> tiles;
    float[] convexHulls;

    IOperationHistory undoHistory;
    IUndoContext undoContext;

    public class Tile {
        String collisionGroup = null;
        int convexHullIndex = 0;
        int convexHullCount = 0;

        public String getCollisionGroup() {
            return this.collisionGroup;
        }

        public void setCollisionGroup(String collisionGroup) {
            this.collisionGroup = collisionGroup;
        }

        public int getConvexHullIndex() {
            return this.convexHullIndex;
        }

        public void setConvexHullIndex(int convexHullIndex) {
            this.convexHullIndex = convexHullIndex;
        }

        public int getConvexHullCount() {
            return this.convexHullCount;
        }

        public void setConvexHullCount(int convexHullCount) {
            this.convexHullCount = convexHullCount;
        }
    }

    public TileSetModel(IOperationHistory undoHistory, IUndoContext undoContext) {
        this.tiles = new ArrayList<Tile>();
        this.undoHistory = undoHistory;
        this.undoContext = undoContext;
    }

    public String getImage() {
        return (String)getProperty(this, KEY_IMAGE);
    }

    public void setImage(String image) {
        modifyModel(KEY_IMAGE, getImage(), image);
    }

    public int getTileWidth() {
        return ((Integer)getProperty(this, KEY_TILE_WIDTH)).intValue();
    }

    public void setTileWidth(int tileWidth) {
        modifyModel(KEY_TILE_WIDTH, new Integer(getTileWidth()), new Integer(tileWidth));
    }

    public int getTileHeight() {
        return ((Integer)getProperty(this, KEY_TILE_HEIGHT)).intValue();
    }

    public void setTileHeight(int tileHeight) {
        modifyModel(KEY_TILE_HEIGHT, new Integer(getTileHeight()), new Integer(tileHeight));
    }

    public int getTileMargin() {
        return ((Integer)getProperty(this, KEY_TILE_MARGIN)).intValue();
    }

    public void setTileMargin(int tileMargin) {
        modifyModel(KEY_TILE_MARGIN, new Integer(getTileMargin()), new Integer(tileMargin));
    }

    public int getTileSpacing() {
        return ((Integer)getProperty(this, KEY_TILE_SPACING)).intValue();
    }

    public void setTileSpacing(int tileSpacing) {
        modifyModel(KEY_TILE_SPACING, new Integer(getTileSpacing()), new Integer(tileSpacing));
    }

    public String getCollision() {
        return (String)getProperty(this, KEY_COLLISION);
    }

    public void setCollision(String collision) {
        modifyModel(KEY_COLLISION, getCollision(), collision);
    }

    public String getMaterialTag() {
        return (String)getProperty(this, KEY_MATERIAL_TAG);
    }

    public void setMaterialTag(String materialTag) {
        modifyModel(KEY_MATERIAL_TAG, getMaterialTag(), materialTag);
    }

    public List<Tile> getTiles() {
        return this.tiles;
    }

    public void setTiles(List<Tile> tiles) {
        this.tiles = tiles;
    }

    public float[] getConvexHulls() {
        return this.convexHulls;
    }

    public void setConvexHulls(float[] convexHulls) {
        this.convexHulls = convexHulls;
    }

    public void load(TileSet tileSet) {
        setProperty(this, KEY_IMAGE, tileSet.getImage());
        setProperty(this, KEY_TILE_WIDTH, new Integer(tileSet.getTileWidth()));
        setProperty(this, KEY_TILE_HEIGHT, new Integer(tileSet.getTileHeight()));
        setProperty(this, KEY_TILE_MARGIN, new Integer(tileSet.getTileMargin()));
        setProperty(this, KEY_TILE_SPACING, new Integer(tileSet.getTileSpacing()));
        setProperty(this, KEY_COLLISION, tileSet.getCollision());
        setProperty(this, KEY_MATERIAL_TAG, tileSet.getMaterialTag());
    }

    private void modifyModel(String key, Object previousValue, Object value) {
        if (this.undoHistory != null) {
            IUndoableOperation operation = new ModifyModelOperation(this, this, key, previousValue, value);
            operation.addContext(this.undoContext);
            this.undoHistory.add(operation);
        }
        setProperty(this, key, value);
    }

}
