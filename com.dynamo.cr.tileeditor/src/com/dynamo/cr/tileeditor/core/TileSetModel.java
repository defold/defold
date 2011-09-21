package com.dynamo.cr.tileeditor.core;

import java.awt.image.BufferedImage;
import java.beans.PropertyChangeEvent;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import javax.imageio.ImageIO;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.tileeditor.Activator;
import com.dynamo.cr.tileeditor.pipeline.ConvexHull2D;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.TextFormat;

@Entity(commandFactory = TileSetUndoableCommandFactory.class)
public class TileSetModel extends Model implements ITileWorld, IAdaptable {

    // TODO: Should be configurable
    private static final int PLANE_COUNT = 16;

    @Property(isResource = true)
    @Resource
    String image;
    @Property
    @Range(min=1)
    int tileWidth;
    @Property
    @Range(min=1)
    int tileHeight;
    @Property
    @Range(min=0)
    int tileMargin;
    @Property
    @Range(min=0)
    int tileSpacing;
    @Property(isResource = true)
    @Resource
    String collision;
    @Property
    @NotEmpty
    String materialTag;

    private static PropertyIntrospector<TileSetModel, TileSetModel> introspector = new PropertyIntrospector<TileSetModel, TileSetModel>(TileSetModel.class, Messages.class);

    private final IContainer contentRoot;
    List<ConvexHull> convexHulls;
    float[] convexHullPoints;
    List<String> collisionGroups;
    String[] selectedCollisionGroups;

    IOperationHistory undoHistory;
    IUndoContext undoContext;

    BufferedImage loadedImage;
    BufferedImage loadedCollision;

    public static class ConvexHull {
        String collisionGroup = "";
        int index = 0;
        int count = 0;

        public ConvexHull(ConvexHull convexHull) {
            this.collisionGroup = convexHull.collisionGroup;
            this.index = convexHull.index;
            this.count = convexHull.count;
        }

        public ConvexHull(String collisionGroup) {
            this.collisionGroup = collisionGroup;
        }

        public ConvexHull(String collisionGroup, int index, int count) {
            this.collisionGroup = collisionGroup;
            this.index = index;
            this.count = count;
        }

        public String getCollisionGroup() {
            return this.collisionGroup;
        }

        public void setCollisionGroup(String collisionGroup) {
            this.collisionGroup = collisionGroup;
        }

        public int getIndex() {
            return this.index;
        }

        public int getCount() {
            return this.count;
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
        if (undoHistory != null)
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

    private void loadImage() {
        try {
            this.loadedImage = loadImageFile(image);
        } catch (Exception e) {
            this.loadedImage = null;
        }
    }

    public void setImage(String image) {
        if ((this.image == null && image != null) || !this.image.equals(image)) {
            String oldImage = this.image;
            this.image = image;
            this.loadedImage = null;
            if (this.image != null && !this.image.equals("")) {
                loadImage();
                if (this.image != null) {
                    updateConvexHulls();
                }
            }
            firePropertyChangeEvent(new PropertyChangeEvent(this, "image", oldImage, image));
        }
    }

    public int getTileWidth() {
        return this.tileWidth;
    }

    protected IStatus validateTileWidth() {
        return verifyTileDimensions();
    }

    public void setTileWidth(int tileWidth) {
        if (this.tileWidth != tileWidth) {
            int oldTileWidth = tileWidth;
            this.tileWidth = tileWidth;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "tileWidth", new Integer(oldTileWidth), new Integer(tileWidth)));
            updateConvexHulls();
        }
    }

    public int getTileHeight() {
        return this.tileHeight;
    }

    protected IStatus validateTileHeight() {
        return verifyTileDimensions();
    }

    public void setTileHeight(int tileHeight) {
        if (this.tileHeight != tileHeight) {
            int oldTileHeight = this.tileHeight;
            this.tileHeight = tileHeight;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "tileHeight", new Integer(oldTileHeight), new Integer(tileHeight)));
            updateConvexHulls();
        }
    }

    public int getTileMargin() {
        return this.tileMargin;
    }

    protected IStatus validateTileMargin() {
        if (this.tileMargin > 0)
            return verifyTileDimensions();
        else
            return Status.OK_STATUS;
    }

    public void setTileMargin(int tileMargin) {
        if (this.tileMargin != tileMargin) {
            int oldTileMargin = this.tileMargin;
            this.tileMargin = tileMargin;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "tileMargin", new Integer(oldTileMargin), new Integer(tileMargin)));
            updateConvexHulls();
        }
    }

    public int getTileSpacing() {
        return this.tileSpacing;
    }

    protected IStatus validateTileSpacing() {
        return verifyTileDimensions();
    }

    public void setTileSpacing(int tileSpacing) {
        if (this.tileSpacing != tileSpacing) {
            int oldTileSpacing = this.tileSpacing;
            this.tileSpacing = tileSpacing;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "tileSpacing", new Integer(oldTileSpacing), new Integer(tileSpacing)));
            updateConvexHulls();
        }
    }

    public String getCollision() {
        return this.collision;
    }

    private void loadCollision() {
        try {
            this.loadedCollision = loadImageFile(collision);
        } catch (Exception e) {
            this.loadedCollision = null;
        }
    }

    public void setCollision(String collision) {
        if ((this.collision == null && collision != null) || !this.collision.equals(collision)) {
            String oldCollision = this.collision;
            this.collision = collision;
            this.loadedCollision = null;
            if (this.collision != null && !this.collision.equals("")) {
                loadCollision();
            }
            updateConvexHulls();
            firePropertyChangeEvent(new PropertyChangeEvent(this, "collision", oldCollision, collision));
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
        }
    }

    public List<ConvexHull> getConvexHulls() {
        List<ConvexHull> copy = new ArrayList<ConvexHull>(this.convexHulls.size());
        for (ConvexHull convexHull : this.convexHulls) {
            copy.add(new ConvexHull(convexHull));
        }
        return copy;
    }

    public void setConvexHulls(List<ConvexHull> convexHulls) {
        if (!this.convexHulls.equals(convexHulls)) {
            List<ConvexHull> oldConvexHulls = this.convexHulls;
            this.convexHulls = convexHulls;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "convexHulls", oldConvexHulls, convexHulls));
        }
    }

    public void setConvexHull(int index, ConvexHull convexHull) {
        ConvexHull oldConvexHull = this.convexHulls.get(index);
        if (!oldConvexHull.equals(convexHull)) {
            List<ConvexHull> oldConvexHulls = new ArrayList<ConvexHull>(this.convexHulls);
            this.convexHulls.set(index, convexHull);
            firePropertyChangeEvent(new PropertyChangeEvent(this, "convexHulls", oldConvexHulls, this.convexHulls));
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

    private BufferedImage loadImageFile(String fileName) throws Exception {
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

    public void load(InputStream is) throws IOException {
        // Build message
        TileSet.Builder tileSetBuilder = TileSet.newBuilder();
        TextFormat.merge(new InputStreamReader(is), tileSetBuilder);
        TileSet tileSet = tileSetBuilder.build();
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
            ConvexHull convexHull = new ConvexHull(convexHullDDF.getCollisionGroup(), convexHullDDF.getIndex(), convexHullDDF.getCount());
            convexHulls.add(convexHull);
        }
        setConvexHulls(convexHulls);
        updateConvexHulls();
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
            Activator.logException(status.getException());
        }
    }

    private void updateConvexHulls() {
        if (isValid()) {
            updateConvexHullsList();
        }
    }

    public boolean isValid() {
        PropertyIntrospectorModel<TileSetModel, TileSetModel> propertyModel = new PropertyIntrospectorModel<TileSetModel, TileSetModel>(this, this, introspector);
        return propertyModel.isValid();
    }

    protected IStatus validateImage() {
        IStatus status = verifyImageDimensions();
        if (!status.isOK())
            return status;
        else
            return verifyTileDimensions();
    }

    protected IStatus validateCollision() {
        IStatus status = verifyImageDimensions();
        if (!status.isOK())
            return status;
        else
            return verifyTileDimensions();
    }

    private IStatus verifyImageDimensions() {
        if (this.loadedImage != null && this.loadedCollision != null) {
            if (this.loadedImage.getWidth() != this.loadedCollision.getWidth() || this.loadedImage.getHeight() != this.loadedCollision.getHeight()) {
                return createStatus(Messages.TS_DIFF_IMG_DIMS, new Object[] {this.loadedImage.getWidth(), this.loadedImage.getHeight(), this.loadedCollision.getWidth(), this.loadedCollision.getHeight()});
            }
        }
        return Status.OK_STATUS;
    }

    private IStatus verifyTileDimensions() {
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
                Status status = createStatus(Messages.TS_TILE_WIDTH_GT_IMG, new Object[] {totalTileWidth, imageWidth});
                return status;
            }
            int totalTileHeight = this.tileHeight + this.tileMargin;
            if (totalTileHeight > imageHeight) {
                Status status = createStatus(Messages.TS_TILE_HEIGHT_GT_IMG, new Object[] {totalTileHeight, imageHeight});
                return status;
            }
        }
        return Status.OK_STATUS;
    }

    private void updateConvexHullsList() {
        List<ConvexHull> convexHulls = null;
        if (this.loadedCollision != null) {
            int imageWidth = this.loadedCollision.getWidth();
            int imageHeight = this.loadedCollision.getHeight();
            int tilesPerRow = TileSetUtil.calculateTileCount(this.tileWidth, imageWidth, this.tileMargin, this.tileSpacing);
            int tilesPerColumn = TileSetUtil.calculateTileCount(this.tileHeight, imageHeight, this.tileMargin, this.tileSpacing);
            if (tilesPerRow <= 0 || tilesPerColumn <= 0) {
                return;
            }
            int tileCount = tilesPerRow * tilesPerColumn;
            convexHulls = new ArrayList<TileSetModel.ConvexHull>(tileCount);
            int i;
            int prevTileCount = this.convexHulls.size();
            for (i = 0; i < tileCount && i < prevTileCount; ++i) {
                ConvexHull convexHull = new ConvexHull(this.convexHulls.get(i).getCollisionGroup());
                convexHulls.add(convexHull);
            }
            for (; i < tileCount; ++i) {
                convexHulls.add(new ConvexHull(""));
            }
            updateConvexHullsData(convexHulls);
        } else {
            convexHulls = new ArrayList<ConvexHull>(this.convexHulls.size());
            for (ConvexHull convexHull : this.convexHulls) {
                ConvexHull newHull = new ConvexHull(convexHull.getCollisionGroup());
                convexHulls.add(newHull);
            }
        }
        setConvexHulls(convexHulls);
    }

    private void updateConvexHullsData(List<ConvexHull> convexHulls) {
        if (convexHulls.size() == 0 || this.loadedCollision == null) {
            this.convexHullPoints = null;
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
                ConvexHull convexHull = new ConvexHull(convexHulls.get(index).getCollisionGroup(), pointCount, points[index].length);
                convexHulls.set(index, convexHull);
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

    public boolean handleResourceChanged(IResourceChangeEvent event) {

        final IFile files[] = new IFile[2];
        final boolean[] reload = new boolean[] { false };

        if (image != null && image.length() > 0) {
            files[0] = this.contentRoot.getFile(new Path(image));
        }
        if (collision != null && collision.length() > 0) {
            files[1] = this.contentRoot.getFile(new Path(collision));
        }

        try {
            event.getDelta().accept(new IResourceDeltaVisitor() {

                @Override
                public boolean visit(IResourceDelta delta) throws CoreException {
                    IResource resource = delta.getResource();

                    boolean found = false;
                    if (files[0] != null && files[0].equals(resource)) {
                        loadImage();
                        found = true;
                    }
                    // NOTE: not else here
                    if (files[1] != null && files[1].equals(resource)) {
                        loadCollision();
                        found = true;
                    }
                    if (found) {
                        updateConvexHulls();
                        reload[0] = true;
                        return false;
                    }
                    return true;
                }
            });
        } catch (CoreException e) {
            Activator.logException(e);
        }

        return reload[0];
    }

    @Override
    public IContainer getContentRoot() {
        return this.contentRoot;
    }

}
