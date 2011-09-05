package com.dynamo.cr.tileeditor.core;

import java.awt.image.BufferedImage;
import java.io.BufferedInputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.List;

import javax.imageio.ImageIO;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.ui.views.properties.IPropertySource;

import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospectorSource;
import com.dynamo.cr.tileeditor.operations.SetPropertiesOperation;
import com.dynamo.cr.tileeditor.pipeline.ConvexHull2D;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.TextFormat;

public class TileSetModel implements IPropertyObjectWorld, IOperationHistoryListener, IAdaptable {

    // TODO: Should be configurable
    private static final int PLANE_COUNT = 16;

    public static final int CHANGE_FLAG_PROPERTIES = (1 << 0);
    public static final int CHANGE_FLAG_COLLISION_GROUPS = (1 << 1);
    public static final int CHANGE_FLAG_TILES = (1 << 2);

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

    List<ConvexHull> convexHulls;
    float[] convexHullPoints;
    List<String> collisionGroups;

    IOperationHistory undoHistory;
    IUndoContext undoContext;

    BufferedImage loadedImage;
    BufferedImage loadedCollision;

    List<IModelChangedListener> listeners;

    private IPropertySource propertySource;

    public class ConvexHull {
        String collisionGroup = null;
        int index = 0;
        int count = 0;

        public String getCollisionGroup() {
            return this.collisionGroup;
        }

        public void setCollisionGroup(String collisionGroup) {
            this.collisionGroup = collisionGroup;
        }

        public int getIndex() {
            return this.index;
        }

        public void setIndex(int index) {
            this.index = index;
        }

        public int getCount() {
            return this.count;
        }

        public void setCount(int count) {
            this.count = count;
        }

        @Override
        public boolean equals(Object o) {
            if (o instanceof ConvexHull) {
                ConvexHull tile = (ConvexHull)o;
                return ((this.collisionGroup == tile.collisionGroup)
                        || (this.collisionGroup != null && this.collisionGroup.equals(tile.collisionGroup)))
                        && this.index == tile.index
                        && this.count == tile.count;
            } else {
                return super.equals(o);
            }
        }
    }

    public TileSetModel(IOperationHistory undoHistory, IUndoContext undoContext) {
        this.convexHulls = new ArrayList<ConvexHull>();
        this.collisionGroups = new ArrayList<String>();
        this.undoHistory = undoHistory;
        this.undoContext = undoContext;
        this.undoHistory.addOperationHistoryListener(this);
        this.undoHistory.setLimit(this.undoContext, 100);
        this.listeners = new ArrayList<IModelChangedListener>();
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertySource.class) {
            if (this.propertySource == null) {
                this.propertySource = new PropertyIntrospectorSource<TileSetModel, TileSetModel>(this, this, null);
            }
            return this.propertySource;
        }
        return null;
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

    public List<ConvexHull> getConvexHulls() {
        return this.convexHulls;
    }

    public void setConvexHulls(List<ConvexHull> convexHulls) {
        this.convexHulls = convexHulls;
    }

    public float[] getConvexHullPoints() {
        return this.convexHullPoints;
    }

    public void setConvexHullPoints(float[] convexHullPoints) {
        this.convexHullPoints = convexHullPoints;
    }

    public List<String> getCollisionGroups() {
        return this.collisionGroups;
    }

    public void addCollisionGroup(String collisionGroup) {
        this.collisionGroups.add(collisionGroup);
        fireModelChangedEvent(new ModelChangedEvent(CHANGE_FLAG_COLLISION_GROUPS));
    }

    public void renameCollisionGroup(String collisionGroup, String newCollisionGroup) {
        this.collisionGroups.set(this.collisionGroups.indexOf(collisionGroup), newCollisionGroup);
        boolean changedTiles = false;
        for (ConvexHull convexHull : this.convexHulls) {
            if (convexHull.getCollisionGroup().equals(collisionGroup)) {
                changedTiles = true;
                convexHull.setCollisionGroup(newCollisionGroup);
            }
        }
        int changeFlags = CHANGE_FLAG_COLLISION_GROUPS;
        if (changedTiles) {
            changeFlags |= CHANGE_FLAG_TILES;
        }
        fireModelChangedEvent(new ModelChangedEvent(changeFlags));
    }

    public void setTileCollisionGroup(int index, String collisionGroup) {
        this.convexHulls.get(index).setCollisionGroup(collisionGroup);
        fireModelChangedEvent(new ModelChangedEvent(CHANGE_FLAG_TILES));
    }

    public void addModelChangedListener(IModelChangedListener listener) {
        listeners.add(listener);
    }

    public void removeModelChangedListener(IModelChangedListener listener) {
        listeners.remove(listener);
    }

    private BufferedImage loadImage(String fileName) throws IOException {
        try {
            InputStream is = new BufferedInputStream(new FileInputStream(fileName));
            try {
                return ImageIO.read(is);
            } finally {
                is.close();
            }
        } catch (IOException e) {
            throw e;
        }
    }

    public void load(TileSet tileSet) throws IOException {
        this.image = tileSet.getImage();
        this.tileWidth = tileSet.getTileWidth();
        this.tileHeight = tileSet.getTileHeight();
        this.tileMargin = tileSet.getTileMargin();
        this.tileSpacing = tileSet.getTileSpacing();
        this.collision = tileSet.getCollision();
        this.materialTag = tileSet.getMaterialTag();
        this.collisionGroups = new ArrayList<String>(tileSet.getCollisionGroupsCount());
        for (String collisionGroup : tileSet.getCollisionGroupsList()) {
            this.collisionGroups.add(collisionGroup);
        }
        this.convexHulls = new ArrayList<ConvexHull>(tileSet.getConvexHullsCount());
        for (Tile.ConvexHull convexHullDDF : tileSet.getConvexHullsList()) {
            ConvexHull convexHull = new ConvexHull();
            convexHull.setIndex(convexHullDDF.getIndex());
            convexHull.setCount(convexHullDDF.getCount());
            convexHull.setCollisionGroup(convexHullDDF.getCollisionGroup());
            this.convexHulls.add(convexHull);
        }
        if (this.image != null && !this.image.equals("")) {
            this.loadedImage = loadImage(image);
        }
        if (this.collision != null && !this.collision.equals("")) {
            this.loadedCollision = loadImage(collision);
        }
        if (verifyImageDimensions()) {
            updateTiles();
            updateConvexHulls();
        }
        fireModelChangedEvent(new ModelChangedEvent(CHANGE_FLAG_PROPERTIES | CHANGE_FLAG_COLLISION_GROUPS | CHANGE_FLAG_TILES));
    }

    public void save(OutputStream outputStream, IProgressMonitor monitor) throws IOException {
        TileSet.Builder tileSetBuilder = TileSet.newBuilder()
                .setImage(this.image)
                .setTileWidth(this.tileWidth)
                .setTileHeight(this.tileHeight)
                .setTileMargin(this.tileMargin)
                .setTileSpacing(this.tileSpacing)
                .setCollision(this.collision)
                .setMaterialTag(this.materialTag);
        for (String collisionGroup : this.collisionGroups) {
            tileSetBuilder.addCollisionGroups(collisionGroup);
        }
        for (ConvexHull convexHull : this.convexHulls) {
            Tile.ConvexHull.Builder convexHullBuilder = Tile.ConvexHull.newBuilder();
            convexHullBuilder.setIndex(convexHull.getIndex());
            convexHullBuilder.setCount(convexHull.getCount());
            convexHullBuilder.setCollisionGroup(convexHull.getCollisionGroup());
            tileSetBuilder.addConvexHulls(convexHullBuilder);
        }
        TileSet tileSet = tileSetBuilder.build();
        try {
            OutputStreamWriter writer = new OutputStreamWriter(outputStream);
            try {
                TextFormat.print(tileSet, writer);
                writer.flush();
            } finally {
                writer.close();
            }
        } catch (IOException e) {
            throw e;
        }
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
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }
            updateTiles();
            updateConvexHulls();
            fireModelChangedEvent(new ModelChangedEvent(CHANGE_FLAG_PROPERTIES));
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
        int prevTileCount = this.convexHulls.size();
        if (tileCount != prevTileCount) {
            List<TileSetModel.ConvexHull> newTiles = new ArrayList<TileSetModel.ConvexHull>(tileCount);
            int i;
            for (i = 0; i < tileCount && i < prevTileCount; ++i) {
                newTiles.add(this.convexHulls.get(i));
            }
            for (; i < tileCount; ++i) {
                TileSetModel.ConvexHull tile = new ConvexHull();
                tile.setCollisionGroup("tile");
                newTiles.add(tile);
            }
            this.convexHulls = newTiles;
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
                TileSetModel.ConvexHull tile = this.convexHulls.get(index);
                tile.setIndex(pointCount);
                tile.setCount(points[index].length);
                pointCount += points[index].length;
            }
        }
        this.convexHullPoints = new float[pointCount * 2];
        for (int row = 0; row < tilesPerColumn; ++row) {
            for (int col = 0; col < tilesPerRow; ++col) {
                int index = col + row * tilesPerRow;
                for (int i = 0; i < points[index].length; ++i) {
                    this.convexHullPoints[i*2 + 0] = points[index][i].getX();
                    this.convexHullPoints[i*2 + 1] = points[index][i].getY();
                }
            }
        }
    }

}
