package com.dynamo.cr.tileeditor.core;

import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.beans.PropertyChangeEvent;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.Arrays;
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
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.tile.ConvexHull;
import com.dynamo.bob.tile.TileSetUtil;
import com.dynamo.bob.tile.TileSetUtil.ConvexHulls;
import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.properties.Resource;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.TextFormat;

@Entity(commandFactory = TileSetUndoableCommandFactory.class)
public class TileSetModel extends Model implements ITileWorld, IAdaptable {

    // TODO: Should be configurable
    private static final int PLANE_COUNT = 16;

    private static Logger logger = LoggerFactory.getLogger(TileSetModel.class);

    @Property(editorType=EditorType.RESOURCE, extensions={"jpg", "png"})
    @Resource
    @NotEmpty
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
    @Property(editorType=EditorType.RESOURCE, extensions={"jpg", "png"})
    @Resource
    @NotEmpty
    String collision;
    @Property
    @NotEmpty(severity = IStatus.ERROR)
    String materialTag;

    private static PropertyIntrospector<TileSetModel, TileSetModel> introspector = new PropertyIntrospector<TileSetModel, TileSetModel>(TileSetModel.class);

    private final IContainer contentRoot;
    List<ConvexHull> convexHulls;
    float[] convexHullPoints = new float[0];
    List<String> collisionGroups;
    String[] selectedCollisionGroups;

    IOperationHistory undoHistory;
    IUndoContext undoContext;

    BufferedImage loadedImage;
    BufferedImage loadedCollision;

    public TileSetModel(IContainer contentRoot, IOperationHistory undoHistory, IUndoContext undoContext) {
        this.contentRoot = contentRoot;
        this.undoHistory = undoHistory;
        this.undoContext = undoContext;

        this.convexHulls = new ArrayList<ConvexHull>();
        this.collisionGroups = new ArrayList<String>();
        this.selectedCollisionGroups = new String[0];
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
        if ((this.image == null && image != null) || (this.image != null && !this.image.equals(image))) {
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
        return verifyTileDimensions(true, false);
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
        return verifyTileDimensions(false, true);
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
            return verifyTileDimensions(true, true);
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
        if ((this.collision == null && collision != null) || (this.collision != null && !this.collision.equals(collision))) {
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
        if ((this.materialTag == null && materialTag != null) || (this.materialTag != null && !this.materialTag.equals(materialTag))) {
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

    public void setConvexHulls(List<ConvexHull> convexHulls, float[] convexHullPoints) {
        if (!this.convexHulls.equals(convexHulls) || !Arrays.equals(this.convexHullPoints, convexHullPoints)) {
            List<ConvexHull> oldConvexHulls = this.convexHulls;
            this.convexHulls = convexHulls;
            this.convexHullPoints = convexHullPoints;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "convexHulls", oldConvexHulls, convexHulls));
        }
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
                this.logger.error("Invalid argument {}", collisionGroup);
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
                this.logger.error("Invalid argument {}", collisionGroup);
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
                this.logger.error("Invalid argument {}", oldCollisionGroups[i]);
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

    public IUndoContext getUndoContext() {
        return undoContext;
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
                BufferedImage origImage = ImageIO.read(is);
                BufferedImage image = new BufferedImage(origImage.getWidth(), origImage.getHeight(), BufferedImage.TYPE_4BYTE_ABGR);
                Graphics2D g2d= image.createGraphics();
                g2d.drawImage(origImage, 0, 0, null);
                g2d.dispose();
                return image;
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
        setConvexHulls(convexHulls, new float[0]);
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
            logger.error("Error occurred while executing operation", e);
        }

        if (status != Status.OK_STATUS) {
            logger.error("Error occurred while executing operation", status.getException());
        }
    }

    public IStatus validate() {
        PropertyIntrospectorModel<TileSetModel, TileSetModel> propertyModel = new PropertyIntrospectorModel<TileSetModel, TileSetModel>(this, this, introspector);
        return propertyModel.getStatus();
    }

    protected IStatus validateImage() {
        IStatus status = verifyImageDimensions();
        if (!status.isOK())
            return status;
        else
            return verifyTileDimensions(true, true);
    }

    protected IStatus validateCollision() {
        IStatus status = verifyImageDimensions();
        if (!status.isOK())
            return status;
        else
            return verifyTileDimensions(true, true);
    }

    private IStatus verifyImageDimensions() {
        if (this.loadedImage != null && this.loadedCollision != null) {
            if (this.loadedImage.getWidth() != this.loadedCollision.getWidth() || this.loadedImage.getHeight() != this.loadedCollision.getHeight()) {
                return createStatus(Messages.TS_DIFF_IMG_DIMS, new Object[] {this.loadedImage.getWidth(), this.loadedImage.getHeight(), this.loadedCollision.getWidth(), this.loadedCollision.getHeight()});
            }
        }
        return Status.OK_STATUS;
    }

    private IStatus verifyTileDimensions(boolean verifyWidth, boolean verifyHeight) {
        if ((verifyWidth || verifyHeight) && (this.loadedImage != null || this.loadedCollision != null)) {
            int imageWidth = 0;
            int imageHeight = 0;
            if (this.loadedImage != null) {
                imageWidth = this.loadedImage.getWidth();
                imageHeight = this.loadedImage.getHeight();
            } else {
                imageWidth = this.loadedCollision.getWidth();
                imageHeight = this.loadedCollision.getHeight();
            }
            if (verifyWidth) {
                int totalTileWidth = this.tileWidth + this.tileMargin;
                if (totalTileWidth > imageWidth) {
                    Status status = createStatus(Messages.TS_TILE_WIDTH_GT_IMG, new Object[] {totalTileWidth, imageWidth});
                    return status;
                }
            }
            if (verifyHeight) {
                int totalTileHeight = this.tileHeight + this.tileMargin;
                if (totalTileHeight > imageHeight) {
                    Status status = createStatus(Messages.TS_TILE_HEIGHT_GT_IMG, new Object[] {totalTileHeight, imageHeight});
                    return status;
                }
            }
        }
        return Status.OK_STATUS;
    }

    private void updateConvexHulls() {
        IStatus status = validate();
        if (status.getSeverity() > IStatus.INFO || this.loadedCollision == null) {
            setConvexHulls(new ArrayList<ConvexHull>(), new float[0]);
            return;
        }

        ConvexHulls result = TileSetUtil.calculateConvexHulls(loadedCollision.getAlphaRaster(), PLANE_COUNT,
                loadedCollision.getWidth(), loadedCollision.getHeight(),
                tileWidth, tileHeight, tileMargin, tileSpacing);

        int tileCount = result.hulls.length;
        int prevTileCount = this.convexHulls.size();
        int i = 0;
        for (i = 0; i < tileCount && i < prevTileCount; ++i) {
            result.hulls[i].setCollisionGroup(this.convexHulls.get(i).getCollisionGroup());
        }
        for (; i < tileCount; ++i) {
            result.hulls[i].setCollisionGroup("");
        }

        setConvexHulls(Arrays.asList(result.hulls), result.points);
    }

    public boolean handleReload(IFile file) {
        boolean result = false;
        if (image != null && image.length() > 0) {
            IFile imgFile = this.contentRoot.getFile(new Path(image));
            if (file.equals(imgFile)) {
                loadImage();
                result = true;
            }
        }
        if (collision != null && collision.length() > 0) {
            IFile collisionFile = this.contentRoot.getFile(new Path(collision));
            if (file.equals(collisionFile)) {
                loadCollision();
                result = true;
            }
        }
        if (result) {
            updateConvexHulls();
        }
        return result;
    }

    public boolean handleResourceChanged(IResourceChangeEvent event) {

        final boolean[] reload = new boolean[] { false };

        try {
            event.getDelta().accept(new IResourceDeltaVisitor() {

                @Override
                public boolean visit(IResourceDelta delta) throws CoreException {
                    IResource resource = delta.getResource();
                    if (resource.getType() == IResource.FILE) {
                        if (handleReload((IFile)resource)) {
                            reload[0] = true;
                            return false;
                        }
                    }
                    return true;
                }
            });
        } catch (CoreException e) {
            logger.error("Error occurred while reloading", e);
        }

        return reload[0];
    }

    @Override
    public IContainer getContentRoot() {
        return this.contentRoot;
    }

}
