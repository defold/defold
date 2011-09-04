package com.dynamo.cr.tileeditor.core;

import java.awt.image.BufferedImage;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import javax.imageio.ImageIO;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.tileeditor.operations.SetPropertiesOperation;
import com.dynamo.cr.tileeditor.pipeline.ConvexHull2D;
import com.dynamo.tile.proto.Tile.TileSet;

public class TileSetModel implements IPropertyObjectWorld, IOperationHistoryListener {

    // TODO: Should be configurable
    private static final int PLANE_COUNT = 16;

    @Property(commandFactory = UndoableCommandFactory.class, isResource = true)
    String image;
    @Property(commandFactory = UndoableCommandFactory.class)
    int tileWidth;
    @Property(commandFactory = UndoableCommandFactory.class)
    int tileHeight;
    @Property(commandFactory = UndoableCommandFactory.class)
    int tileMargin;
    @Property(commandFactory = UndoableCommandFactory.class)
    int tileSpacing;
    @Property(commandFactory = UndoableCommandFactory.class, isResource = true)
    String collision;
    @Property(commandFactory = UndoableCommandFactory.class)
    String materialTag;

    List<Tile> tiles;
    float[] convexHulls;

    IOperationHistory undoHistory;
    IUndoContext undoContext;

    BufferedImage loadedImage;
    BufferedImage loadedCollision;

    List<IModelChangedListener> listeners;

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
        this.undoHistory.addOperationHistoryListener(this);
        this.undoHistory.setLimit(this.undoContext, 100);
        this.listeners = new ArrayList<IModelChangedListener>();
    }

    public String getImage() {
        return image;
    }

    public void setImage(String image) {
        this.image = image;
    }

    public int getTileWidth() {
        return this.tileWidth;
    }

    public void setTileWidth(int tileWidth) {
        this.tileWidth = tileWidth;
    }

    public int getTileHeight() {
        return this.tileHeight;
    }

    public void setTileHeight(int tileHeight) {
        this.tileHeight = tileHeight;
    }

    public int getTileMargin() {
        return this.tileMargin;
    }

    public void setTileMargin(int tileMargin) {
        this.tileMargin = tileMargin;
    }

    public int getTileSpacing() {
        return this.tileSpacing;
    }

    public void setTileSpacing(int tileSpacing) {
        this.tileSpacing = tileSpacing;
    }

    public String getCollision() {
        return this.collision;
    }

    public void setCollision(String collision) {
        this.collision = collision;
    }

    public String getMaterialTag() {
        return this.materialTag;
    }

    public void setMaterialTag(String materialTag) {
        this.materialTag = materialTag;
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

    public void addModelChangedListener(IModelChangedListener listener) {
        listeners.add(listener);
    }

    public void removeModelChangedListener(IModelChangedListener listener) {
        listeners.remove(listener);
    }

    public void load(TileSet tileSet) {
        this.image = tileSet.getImage();
        this.tileWidth = tileSet.getTileWidth();
        this.tileHeight = tileSet.getTileHeight();
        this.tileMargin = tileSet.getTileMargin();
        this.tileSpacing = tileSet.getTileSpacing();
        this.collision = tileSet.getCollision();
        this.materialTag = tileSet.getMaterialTag();
        try {
            if (this.image != null && !this.image.equals("")) {
                this.loadedImage = ImageIO.read(new FileInputStream(image));
            }
            if (this.collision != null && !this.collision.equals("")) {
                this.loadedCollision = ImageIO.read(new FileInputStream(collision));
            }
            if (verifyImageDimensions()) {
                updateTiles();
                updateConvexHulls();
            }
        } catch (IOException e) {
            // TODO: Fix logging
            e.printStackTrace();
        }
        fireModelChangedEvent(new ModelChangedEvent(ModelChangedEvent.CHANGE_FLAG_PROPERTIES));
    }

    public <T> void executeOperation(SetPropertiesOperation<T> operation) {
        operation.addContext(this.undoContext);
        IStatus status = null;
        try {
            status = this.undoHistory.execute(operation, null, null);

        } catch (final ExecutionException e) {
            // TODO: Logging
            e.printStackTrace();
        }

        if (status != Status.OK_STATUS) {
            // TODO: Logging
        }
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        if (event.getEventType() != OperationHistoryEvent.DONE
                && event.getEventType() != OperationHistoryEvent.UNDONE
                && event.getEventType() != OperationHistoryEvent.REDONE) {
            return;
        }
        IUndoableOperation operation = event.getOperation();
        if (operation instanceof SetPropertiesOperation<?>) {
            SetPropertiesOperation<?> setOp = (SetPropertiesOperation<?>)operation;
            if (setOp.getProperty().equals("image")) {
                if (this.image != null && !this.image.equals("")) {
                    try {
                        this.loadedImage = ImageIO.read(new FileInputStream(this.image));
                        if (!verifyImageDimensions()) {
                            this.loadedImage = null;
                        }
                        updateTiles();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            } else if (setOp.getProperty().equals("collision")) {
                if (this.collision != null && !this.collision.equals("")) {
                    try {
                        this.loadedCollision = ImageIO.read(new FileInputStream(this.collision));
                        if (!verifyImageDimensions()) {
                            this.loadedCollision = null;
                        }
                        updateConvexHulls();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }
            fireModelChangedEvent(new ModelChangedEvent(ModelChangedEvent.CHANGE_FLAG_PROPERTIES));
        }
    }

    private void fireModelChangedEvent(ModelChangedEvent e) {
        for (IModelChangedListener listener : listeners) {
            listener.onModelChanged(e);
        }
    }

    private int calcTileCount(int tileSize, int imageSize) {
        int actualTileSize = (2 * this.tileMargin + this.tileSpacing + tileSize);
        if (actualTileSize != 0) {
            return (imageSize + this.tileSpacing)/actualTileSize;
        } else {
            return 0;
        }
    }

    private void updateTiles() {
        if (this.loadedImage == null && this.loadedCollision == null) {
            return;
        }
        int imageWidth = 0;
        int imageHeight = 0;
        if (this.image != null) {
            imageWidth = this.loadedImage.getWidth();
            imageHeight = this.loadedImage.getHeight();
        } else {
            imageWidth = this.loadedCollision.getWidth();
            imageHeight = this.loadedCollision.getHeight();
        }
        int tilesPerRow = calcTileCount(this.tileWidth, imageWidth);
        int tilesPerColumn = calcTileCount(this.tileHeight, imageHeight);
        int tileCount = tilesPerRow * tilesPerColumn;
        int prevTileCount = this.tiles.size();
        if (tileCount != prevTileCount) {
            List<TileSetModel.Tile> newTiles = new ArrayList<TileSetModel.Tile>(tileCount);
            int i;
            for (i = 0; i < tileCount && i < prevTileCount; ++i) {
                newTiles.add(this.tiles.get(i));
            }
            for (; i < tileCount; ++i) {
                TileSetModel.Tile tile = new Tile();
                tile.setCollisionGroup("tile");
                newTiles.add(tile);
            }
            this.tiles = newTiles;
        }
    }

    private boolean verifyImageDimensions() {
        if (this.loadedImage != null && this.loadedCollision != null) {
            if (this.loadedImage.getWidth() != this.loadedCollision.getWidth() || this.loadedImage.getHeight() != this.loadedCollision.getHeight()) {
                // TODO: Report error
                return false;
            }
        }
        return true;
    }

    private void updateConvexHulls() {
        if (this.loadedCollision == null) {
            return;
        }

        int width = this.loadedCollision.getWidth();
        int height = this.loadedCollision.getHeight();

        int tilesPerRow = calcTileCount(this.tileWidth, width);
        int tilesPerColumn = calcTileCount(this.tileHeight, height);
        ConvexHull2D.Point[][] points = new ConvexHull2D.Point[tilesPerRow * tilesPerColumn][];
        int pointCount = 0;
        int[] mask = new int[this.tileWidth * this.tileHeight];
        for (int row = 0; row < tilesPerColumn; ++row) {
            for (int col = 0; col < tilesPerRow; ++col) {
                int x = this.tileMargin + col * (2 * this.tileMargin + this.tileSpacing + this.tileWidth);
                int y = this.tileMargin + row * (2 * this.tileMargin + this.tileSpacing + this.tileHeight);
                mask = loadedCollision.getAlphaRaster().getPixels(x, y, this.tileWidth, this.tileHeight, mask);
                int index = col + row * tilesPerRow;
                points[index] = ConvexHull2D.imageConvexHull(mask, this.tileWidth, this.tileHeight, PLANE_COUNT);
                TileSetModel.Tile tile = this.tiles.get(index);
                tile.setConvexHullIndex(pointCount);
                tile.setConvexHullCount(points[index].length);
                pointCount += points[index].length;
            }
        }
        this.convexHulls = new float[pointCount * 2];
        for (int row = 0; row < tilesPerRow; ++row) {
            for (int col = 0; col < tilesPerColumn; ++col) {
                int index = col + row * tilesPerRow;
                for (int i = 0; i < points.length; ++i) {
                    this.convexHulls[i*2 + 0] = points[index][i].getX();
                    this.convexHulls[i*2 + 1] = points[index][i].getY();
                }
            }
        }
    }

}
