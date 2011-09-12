package com.dynamo.cr.tileeditor.core;

import java.awt.image.BufferedImage;
import java.beans.PropertyChangeEvent;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.imageio.ImageIO;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.tileeditor.Activator;
import com.dynamo.cr.tileeditor.pipeline.ConvexHull2D;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.TextFormat;

@Entity(commandFactory = UndoableCommandFactory.class, accessor = TileSetPropertyAccessor.class)
public class TileSetModel extends Model implements IPropertyObjectWorld, IAdaptable {

    // TODO: Should be configurable
    private static final int PLANE_COUNT = 16;

    public static final Tag TAG_1 = new Tag("1", Tag.TYPE_INFO, Messages.TileSetModel_TAG_1);
    public static final Tag TAG_2 = new Tag("2", Tag.TYPE_ERROR, Messages.TileSetModel_TAG_2);
    public static final Tag TAG_3 = new Tag("3", Tag.TYPE_ERROR, Messages.TileSetModel_TAG_3);
    public static final Tag TAG_4 = new Tag("4", Tag.TYPE_ERROR, Messages.TileSetModel_TAG_4);
    public static final Tag TAG_5 = new Tag("5", Tag.TYPE_ERROR, Messages.TileSetModel_TAG_5);
    public static final Tag TAG_6 = new Tag("6", Tag.TYPE_ERROR, Messages.TileSetModel_TAG_6);
    public static final Tag TAG_7 = new Tag("7", Tag.TYPE_ERROR, Messages.TileSetModel_TAG_7);
    public static final Tag TAG_8 = new Tag("8", Tag.TYPE_ERROR, Messages.TileSetModel_TAG_8);
    public static final Tag TAG_9 = new Tag("9", Tag.TYPE_ERROR, Messages.TileSetModel_TAG_9);
    public static final Tag TAG_10 = new Tag("10", Tag.TYPE_ERROR, Messages.TileSetModel_TAG_10);
    public static final Tag TAG_11 = new Tag("11", Tag.TYPE_ERROR, Messages.TileSetModel_TAG_11);

    @Property(isResource = true)
    String image;
    @Property
    int tileWidth;
    @Property
    int tileHeight;
    @Property
    int tileMargin;
    @Property
    int tileSpacing;
    @Property(isResource = true)
    String collision;
    @Property
    String materialTag;

    private static PropertyIntrospector<TileSetModel, TileSetModel> introspector = new PropertyIntrospector<TileSetModel, TileSetModel>(TileSetModel.class);

    private final IContainer contentRoot;
    List<ConvexHull> convexHulls;
    float[] convexHullPoints;
    List<String> collisionGroups;
    String[] selectedCollisionGroups;

    IOperationHistory undoHistory;
    IUndoContext undoContext;

    BufferedImage loadedImage;
    BufferedImage loadedCollision;

    public class ConvexHull {
        String collisionGroup = "";
        int index = 0;
        int count = 0;

        public String getCollisionGroup() {
            return this.collisionGroup;
        }

        public void setCollisionGroup(String collisionGroup) {
            if ((this.collisionGroup == null && collisionGroup != null) || !this.collisionGroup.equals(collisionGroup)) {
                String oldCollisionGroup = collisionGroup;
                this.collisionGroup = collisionGroup;
                firePropertyChangeEvent(new PropertyChangeEvent(this, "collisionGroup", oldCollisionGroup, collisionGroup));
            }
        }

        public int getIndex() {
            return this.index;
        }

        public void setIndex(int index) {
            if (this.index != index) {
                int oldIndex = this.index;
                this.index = index;
                firePropertyChangeEvent(new PropertyChangeEvent(this, "index", oldIndex, index));
            }
        }

        public int getCount() {
            return this.count;
        }

        public void setCount(int count) {
            if (this.count != count) {
                int oldCount = this.count;
                this.count = count;
                firePropertyChangeEvent(new PropertyChangeEvent(this, "count", oldCount, count));
            }
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

    public TileSetModel(IContainer contentRoot, IOperationHistory undoHistory, IUndoContext undoContext) {
        this.contentRoot = contentRoot;
        this.convexHulls = new ArrayList<ConvexHull>();
        this.collisionGroups = new ArrayList<String>();
        this.selectedCollisionGroups = new String[0];
        this.undoHistory = undoHistory;
        this.undoContext = undoContext;
        this.undoHistory.setLimit(this.undoContext, 100);
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<TileSetModel, TileSetModel>(this, this, introspector);
        }
        return null;
    }

    public String getImage() {
        return image;
    }

    public void setImage(String image) {
        if ((this.image == null && image != null) || !this.image.equals(image)) {
            String oldImage = this.image;
            this.image = image;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "image", oldImage, image));
            if (this.image != null && !this.image.equals("")) {
                try {
                    this.loadedImage = loadImage(image);
                    updateConvexHulls();
                    clearPropertyTag("image", TAG_2);
                } catch (Exception e) {
                    this.loadedImage = null;
                    setPropertyTag("image", Tag.bind(TAG_2, image));
                }
                clearPropertyTag("image", TAG_1);
            } else {
                setPropertyTag("image", TAG_1);
            }
        }
    }

    public int getTileWidth() {
        return this.tileWidth;
    }

    public void setTileWidth(int tileWidth) {
        if (this.tileWidth != tileWidth) {
            int oldTileWidth = tileWidth;
            this.tileWidth = tileWidth;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "tileWidth", new Integer(oldTileWidth), new Integer(tileWidth)));
            if (tileWidth > 0) {
                clearPropertyTag("tileWidth", TAG_5);
            } else {
                setPropertyTag("tileWidth", TAG_5);
            }
            updateConvexHulls();
        }
    }

    public int getTileHeight() {
        return this.tileHeight;
    }

    public void setTileHeight(int tileHeight) {
        if (this.tileHeight != tileHeight) {
            int oldTileHeight = this.tileHeight;
            this.tileHeight = tileHeight;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "tileHeight", new Integer(oldTileHeight), new Integer(tileHeight)));
            if (tileHeight > 0) {
                clearPropertyTag("tileHeight", TAG_6);
            } else {
                setPropertyTag("tileHeight", TAG_6);
            }
            updateConvexHulls();
        }
    }

    public int getTileMargin() {
        return this.tileMargin;
    }

    public void setTileMargin(int tileMargin) {
        if (this.tileMargin != tileMargin) {
            int oldTileMargin = this.tileMargin;
            this.tileMargin = tileMargin;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "tileMargin", new Integer(oldTileMargin), new Integer(tileMargin)));
            if (tileMargin >= 0) {
                clearPropertyTag("tileMargin", TAG_10);
            } else {
                setPropertyTag("tileMargin", TAG_10);
            }
            updateConvexHulls();
        }
    }

    public int getTileSpacing() {
        return this.tileSpacing;
    }

    public void setTileSpacing(int tileSpacing) {
        if (this.tileSpacing != tileSpacing) {
            int oldTileSpacing = this.tileSpacing;
            this.tileSpacing = tileSpacing;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "tileSpacing", new Integer(oldTileSpacing), new Integer(tileSpacing)));
            if (tileSpacing >= 0) {
                clearPropertyTag("tileSpacing", TAG_11);
            } else {
                setPropertyTag("tileSpacing", TAG_11);
            }
            updateConvexHulls();
        }
    }

    public String getCollision() {
        return this.collision;
    }

    public void setCollision(String collision) {
        if ((this.collision == null && collision != null) || !this.collision.equals(collision)) {
            String oldCollision = this.collision;
            this.collision = collision;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "collision", oldCollision, collision));
            if (this.collision != null && !this.collision.equals("")) {
                try {
                    this.loadedCollision = loadImage(collision);
                    updateConvexHulls();
                    clearPropertyTag("collision", TAG_3);
                } catch (Exception e) {
                    this.loadedCollision = null;
                    setPropertyTag("collision", Tag.bind(TAG_3, collision));
                }
            }
        }
    }

    public String getMaterialTag() {
        return this.materialTag;
    }

    public void setMaterialTag(String materialTag) {
        if ((this.materialTag == null && materialTag != null) || !this.materialTag.equals(materialTag)) {
            String oldMaterialTag = this.materialTag;
            this.materialTag = materialTag;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "materialTag", oldMaterialTag, materialTag));
            if (materialTag == null || materialTag.equals("")) {
                setPropertyTag("materialTag", TAG_9);
            } else {
                clearPropertyTag("materialTag", TAG_9);
            }
        }
    }

    public List<ConvexHull> getConvexHulls() {
        return this.convexHulls;
    }

    public void setConvexHulls(List<ConvexHull> convexHulls) {
        if (this.convexHulls != convexHulls) {
            boolean equal = true;
            if (this.convexHulls.size() == convexHulls.size()) {
                int n = this.convexHulls.size();
                for (int i = 0; i < n; ++i) {
                    if (!this.convexHulls.get(i).equals(convexHulls.get(i))) {
                        equal = false;
                        break;
                    }
                }
            } else {
                equal = false;
            }
            if (!equal) {
                List<ConvexHull> oldConvexHulls = this.convexHulls;
                this.convexHulls = convexHulls;
                firePropertyChangeEvent(new PropertyChangeEvent(this, "convexHulls", oldConvexHulls, convexHulls));
            }
        }
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

    public void setCollisionGroups(List<String> collisionGroups) {
        if (this.collisionGroups != collisionGroups) {
            Collections.sort(collisionGroups);
            boolean equal = true;
            if (this.collisionGroups.size() == collisionGroups.size()) {
                int n = this.collisionGroups.size();
                for (int i = 0; i < n; ++i) {
                    if (!this.collisionGroups.get(i).equals(collisionGroups.get(i))) {
                        equal = false;
                        break;
                    }
                }
            } else {
                equal = false;
            }
            if (!equal) {
                List<String> oldCollisionGroups = this.collisionGroups;
                this.collisionGroups = collisionGroups;
                firePropertyChangeEvent(new PropertyChangeEvent(this, "collisionGroups", oldCollisionGroups, collisionGroups));
            }
        }
    }

    public String[] getSelectedCollisionGroups() {
        return this.selectedCollisionGroups;
    }

    public void setSelectedCollisionGroup(String selectedCollisionGroup) {
        List<String> selectedCollisionGroups = new ArrayList<String>(1);
        selectedCollisionGroups.add(selectedCollisionGroup);
        setSelectedCollisionGroups(selectedCollisionGroups.toArray(new String[selectedCollisionGroups.size()]));
    }

    public void setSelectedCollisionGroups(String[] selectedCollisionGroups) {
        this.selectedCollisionGroups = selectedCollisionGroups;
    }

    public void addCollisionGroup(String collisionGroup) {
        addCollisionGroups(new String[] {collisionGroup});
    }

    public void addCollisionGroups(String[] collisionGroups) {
        boolean added = false;
        List<String> oldCollisionGroups = new ArrayList<String>(this.collisionGroups);
        for (String collisionGroup : collisionGroups) {
            if (this.collisionGroups.contains(collisionGroup)) {
                Activator.logException(new IllegalArgumentException(collisionGroup));
            } else {
                added = true;
                this.collisionGroups.add(collisionGroup);
            }
        }
        if (added) {
            Collections.sort(this.collisionGroups);
            firePropertyChangeEvent(new PropertyChangeEvent(this, "collisionGroups", oldCollisionGroups, this.collisionGroups));
        }
    }

    public void removeCollisionGroup(String collisionGroup) {
        removeCollisionGroups(new String[] {collisionGroup});
    }

    public void removeCollisionGroups(String[] collisionGroups) {
        boolean removed = false;
        List<String> oldCollisionGroups = new ArrayList<String>(this.collisionGroups);
        for (String collisionGroup : collisionGroups) {
            if (!this.collisionGroups.contains(collisionGroup)) {
                Activator.logException(new IllegalArgumentException(collisionGroup));
            } else {
                removed = true;
                this.collisionGroups.remove(collisionGroup);
            }
        }
        if (removed) {
            firePropertyChangeEvent(new PropertyChangeEvent(this, "collisionGroups", oldCollisionGroups, this.collisionGroups));
        }
    }

    public void renameCollisionGroup(String oldCollisionGroup, String newCollisionGroup) {
        renameCollisionGroups(new String[] {oldCollisionGroup}, new String[] {newCollisionGroup});
    }

    public void renameCollisionGroups(String[] oldCollisionGroups, String[] newCollisionGroups) {
        boolean renamed = false;
        List<String> tmpCollisionGroups = new ArrayList<String>(this.collisionGroups);
        int n = oldCollisionGroups.length;
        for (int i = 0; i < n; ++i) {
            if (!tmpCollisionGroups.contains(oldCollisionGroups[i])) {
                Activator.logException(new IllegalArgumentException(oldCollisionGroups[i]));
            } else {
                renamed = true;
                this.collisionGroups.remove(oldCollisionGroups[i]);
                if (!this.collisionGroups.contains(newCollisionGroups[i])) {
                    this.collisionGroups.add(newCollisionGroups[i]);
                }
            }
        }
        if (renamed) {
            Collections.sort(this.collisionGroups);
            firePropertyChangeEvent(new PropertyChangeEvent(this, "collisionGroups", tmpCollisionGroups, this.collisionGroups));
        }
    }

    public IOperationHistory getUndoHistory() {
        return undoHistory;
    }

    public BufferedImage getLoadedImage() {
        return this.loadedImage;
    }

    public BufferedImage getLoadedCollision() {
        return this.loadedCollision;
    }

    private BufferedImage loadImage(String fileName) throws Exception {
        try {
            IFile file = this.contentRoot.getFile(new Path(fileName));
            InputStream is = file.getContents();
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
        // Set properties
        setImage(tileSet.getImage());
        setTileWidth(tileSet.getTileWidth());
        setTileHeight(tileSet.getTileHeight());
        setTileMargin(tileSet.getTileMargin());
        setTileSpacing(tileSet.getTileSpacing());
        setCollision(tileSet.getCollision());
        setMaterialTag(tileSet.getMaterialTag());
        // Set groups
        setCollisionGroups(new ArrayList<String>(tileSet.getCollisionGroupsList()));
        // Set convex hulls
        List<ConvexHull> convexHulls = new ArrayList<ConvexHull>(tileSet.getConvexHullsCount());
        for (Tile.ConvexHull convexHullDDF : tileSet.getConvexHullsList()) {
            ConvexHull convexHull = new ConvexHull();
            convexHull.setIndex(convexHullDDF.getIndex());
            convexHull.setCount(convexHullDDF.getCount());
            convexHull.setCollisionGroup(convexHullDDF.getCollisionGroup());
            convexHulls.add(convexHull);
        }
        setConvexHulls(convexHulls);
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

    public void executeOperation(IUndoableOperation operation) {
        operation.addContext(this.undoContext);
        IStatus status = null;
        try {
            status = this.undoHistory.execute(operation, null, null);
        } catch (final ExecutionException e) {
            Activator.logException(e);
        }

        if (status != Status.OK_STATUS) {
            // TODO: Logging or dialog?
        }
    }

    private void updateConvexHulls() {
        if (verifyImageDimensions() && verifyTileDimensions()) {
            updateConvexHullsList();
            updateConvexHullsData();
        }
    }

    private boolean verifyImageDimensions() {
        if (this.loadedImage != null && this.loadedCollision != null) {
            if (this.loadedImage.getWidth() != this.loadedCollision.getWidth() || this.loadedImage.getHeight() != this.loadedCollision.getHeight()) {
                Tag tag = Tag.bind(TAG_4, new Object[] {this.loadedImage.getWidth(), this.loadedImage.getHeight(), this.loadedCollision.getWidth(), this.loadedCollision.getHeight()});
                setPropertyTag("image", tag);
                setPropertyTag("collision", tag);
                return false;
            }
        }
        clearPropertyTag("image", TAG_4);
        clearPropertyTag("collision", TAG_4);
        return true;
    }

    private boolean verifyTileDimensions() {
        boolean result = true;
        if (this.loadedImage != null || this.loadedCollision != null) {
            int imageWidth = 0;
            int imageHeight = 0;
            if (this.loadedImage != null) {
                imageWidth = this.loadedImage.getWidth();
                imageHeight = this.loadedImage.getHeight();
            } else {
                imageWidth = this.loadedCollision.getWidth();
                imageHeight = this.loadedCollision.getHeight();
            }
            int totalTileWidth = this.tileWidth + this.tileMargin;
            if (totalTileWidth > imageWidth) {
                Tag tag = Tag.bind(TAG_7, new Object[] {totalTileWidth, imageWidth});
                if (this.loadedImage != null) {
                    setPropertyTag("image", tag);
                } else {
                    setPropertyTag("collision", tag);
                }
                setPropertyTag("tileWidth", tag);
                if (this.tileMargin > 0) {
                    setPropertyTag("tileMargin", tag);
                } else {
                    clearPropertyTag("tileMargin", tag);
                }
                result = false;
            } else {
                clearPropertyTag("image", TAG_7);
                clearPropertyTag("collision", TAG_7);
                clearPropertyTag("tileWidth", TAG_7);
                clearPropertyTag("tileMargin", TAG_7);
            }
            int totalTileHeight = this.tileHeight + this.tileMargin;
            if (totalTileHeight > imageHeight) {
                Tag tag = Tag.bind(TAG_8, new Object[] {totalTileHeight, imageHeight});
                if (this.loadedImage != null) {
                    setPropertyTag("image", tag);
                } else {
                    setPropertyTag("collision", tag);
                }
                setPropertyTag("tileHeight", tag);
                if (this.tileMargin > 0) {
                    setPropertyTag("tileMargin", tag);
                } else {
                    clearPropertyTag("tileMargin", tag);
                }
                result = false;
            } else {
                clearPropertyTag("image", TAG_8);
                clearPropertyTag("collision", TAG_8);
                clearPropertyTag("tileHeight", TAG_8);
                clearPropertyTag("tileMargin", TAG_8);
            }
        }
        return result;
    }

    private void updateConvexHullsList() {
        if (this.loadedImage == null && this.loadedCollision == null) {
            return;
        }
        int imageWidth = 0;
        int imageHeight = 0;
        if (this.loadedImage != null) {
            imageWidth = this.loadedImage.getWidth();
            imageHeight = this.loadedImage.getHeight();
        } else {
            imageWidth = this.loadedCollision.getWidth();
            imageHeight = this.loadedCollision.getHeight();
        }
        int tilesPerRow = TileSetUtil.calculateTileCount(this.tileWidth, imageWidth, this.tileMargin, this.tileSpacing);
        int tilesPerColumn = TileSetUtil.calculateTileCount(this.tileHeight, imageHeight, this.tileMargin, this.tileSpacing);
        if (tilesPerRow <= 0 || tilesPerColumn <= 0) {
            // TODO: Report error
            return;
        }
        int tileCount = tilesPerRow * tilesPerColumn;
        int prevTileCount = this.convexHulls.size();
        if (tileCount != prevTileCount) {
            List<TileSetModel.ConvexHull> newTiles = new ArrayList<TileSetModel.ConvexHull>(tileCount);
            int i;
            for (i = 0; i < tileCount && i < prevTileCount; ++i) {
                newTiles.add(this.convexHulls.get(i));
            }
            for (; i < tileCount; ++i) {
                newTiles.add(new ConvexHull());
            }
            setConvexHulls(newTiles);
        }
    }

    private void updateConvexHullsData() {
        if (this.loadedCollision == null) {
            return;
        }

        int width = this.loadedCollision.getWidth();
        int height = this.loadedCollision.getHeight();

        int tilesPerRow = TileSetUtil.calculateTileCount(this.tileWidth, width, this.tileMargin, this.tileSpacing);
        int tilesPerColumn = TileSetUtil.calculateTileCount(this.tileHeight, height, this.tileMargin, this.tileSpacing);
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
        int totalIndex = 0;
        for (int row = 0; row < tilesPerColumn; ++row) {
            for (int col = 0; col < tilesPerRow; ++col) {
                int index = col + row * tilesPerRow;
                for (int i = 0; i < points[index].length; ++i) {
                    this.convexHullPoints[totalIndex++] = points[index][i].getX();
                    this.convexHullPoints[totalIndex++] = points[index][i].getY();
                }
            }
        }
    }

    Map<String, List<Tag>> propertyTags = new HashMap<String, List<Tag>>();

    public boolean hasPropertyAnnotation(String property, Tag tag) {
        List<Tag> tags = propertyTags.get(property);
        if (tags != null && !tags.isEmpty()) {
            return tags.contains(tag);
        }
        return false;
    }

    public List<Tag> getPropertyTags(String property) {
        return propertyTags.get(property);
    }

    public Tag getPropertyTag(String property, Tag tag) {
        List<Tag> tags = propertyTags.get(property);
        if (tags != null && !tags.isEmpty()) {
            int index = tags.indexOf(tag);
            return tags.get(index);
        }
        return null;
    }

    private void setPropertyTag(String property, Tag tag) {
        List<Tag> tags = propertyTags.get(property);
        if (tags == null) {
            tags = new ArrayList<Tag>();
            propertyTags.put(property, tags);
        }
        if (tags.contains(tag)) {
            tags.remove(tag);
        }
        tags.add(tag);
        firePropertyTagEvent(new PropertyTagEvent(this, property, tags));
    }

    private void clearPropertyTag(String property, Tag tag) {
        List<Tag> tags = propertyTags.get(property);
        if (tags != null && tags.contains(tag)) {
            tags.remove(tag);
            firePropertyTagEvent(new PropertyTagEvent(this, property, tags));
        }
    }

}
