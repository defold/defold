package com.dynamo.cr.tileeditor.scene;

import java.awt.image.BufferedImage;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

import javax.imageio.ImageIO;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.editor.ui.Activator;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.properties.ValidatorUtil;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.tile.ConvexHull;
import com.dynamo.tile.TileSetUtil;
import com.dynamo.tile.TileSetUtil.ConvexHulls;

public class TileSetNode extends Node {

    // TODO: Should be configurable
    private static final int PLANE_COUNT = 16;

    @Property(isResource = true)
    @Resource
    private String image = "";

    @Property
    @Range(min=1)
    private int tileWidth;

    @Property
    @Range(min=1)
    private int tileHeight;

    @Property
    @Range(min=0)
    private int tileMargin;

    @Property
    @Range(min=0)
    private int tileSpacing;

    @Property(isResource = true)
    @Resource
    private String collision = "";

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    private String materialTag = "";

    private List<CollisionGroupNode> tileCollisionGroups;
    // Used to simulate a shorter list of convex hulls.
    // convexHulls need to be retained in some cases to keep collision groups and keep undo functionality
    private int tileCollisionGroupCount;
    private List<ConvexHull> convexHulls;
    private float[] convexHullPoints = new float[0];

    private BufferedImage loadedImage;
    private BufferedImage loadedCollision;

    public TileSetNode() {
        this.tileCollisionGroups = new ArrayList<CollisionGroupNode>();
        this.tileCollisionGroupCount = 0;
        this.convexHulls = new ArrayList<ConvexHull>();
    }

    public String getImage() {
        return image;
    }

    public IStatus validateImage() {
        if (this.image.isEmpty() && this.collision.isEmpty()) {
            return ValidatorUtil.createStatus(this, "image", IStatus.INFO, "EMPTY", null);
        }
        IStatus status = verifyImageDimensions();
        if (!status.isOK())
            return status;
        else
            return verifyTileDimensions(true, true);
    }

    public void setImage(String image) {
        if (!this.image.equals(image)) {
            this.image = image;
            this.loadedImage = null;
            if (!this.image.isEmpty()) {
                updateImage();
            }
            updateConvexHulls();
            notifyChange();
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
            this.tileWidth = tileWidth;
            updateConvexHulls();
            notifyChange();
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
            this.tileHeight = tileHeight;
            updateConvexHulls();
            notifyChange();
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
            this.tileMargin = tileMargin;
            updateConvexHulls();
            notifyChange();
        }
    }

    public int getTileSpacing() {
        return this.tileSpacing;
    }

    public void setTileSpacing(int tileSpacing) {
        if (this.tileSpacing != tileSpacing) {
            this.tileSpacing = tileSpacing;
            updateConvexHulls();
            notifyChange();
        }
    }

    public String getCollision() {
        return this.collision;
    }

    public IStatus validateCollision() {
        if (this.image.isEmpty() && this.collision.isEmpty()) {
            return ValidatorUtil.createStatus(this, "collision", IStatus.INFO, "EMPTY", null);
        }
        IStatus status = verifyImageDimensions();
        if (!status.isOK())
            return status;
        else
            return verifyTileDimensions(true, true);
    }

    public void setCollision(String collision) {
        if (!this.collision.equals(collision)) {
            this.collision = collision;
            this.loadedCollision = null;
            if (this.collision != null && !this.collision.equals("")) {
                updateCollision();
            }
            updateConvexHulls();
            notifyChange();
        }
    }

    public String getMaterialTag() {
        return this.materialTag;
    }

    public void setMaterialTag(String materialTag) {
        if (!this.materialTag.equals(materialTag)) {
            this.materialTag = materialTag;
            notifyChange();
        }
    }

    public void addCollisionGroup(CollisionGroupNode groupNode) {
        addChild(groupNode);
        sortCollisionGroups();
    }

    public void removeCollisionGroup(CollisionGroupNode groupNode) {
        removeChild(groupNode);
    }

    private void sortCollisionGroups() {
        sortChildren(new Comparator<Node>() {
            @Override
            public int compare(Node o1, Node o2) {
                String id1 = ((CollisionGroupNode)o1).getName();
                String id2 = ((CollisionGroupNode)o2).getName();
                return id1.compareTo(id2);
            }
        });
    }

    public List<ConvexHull> getConvexHulls() {
        return Collections.unmodifiableList(this.convexHulls);
    }

    public List<String> getTileCollisionGroups() {
        List<String> tileCollisionGroups = new ArrayList<String>(this.tileCollisionGroupCount);
        for (CollisionGroupNode collisionGroup : this.tileCollisionGroups) {
            if (collisionGroup != null) {
                tileCollisionGroups.add(collisionGroup.getName());
            } else {
                tileCollisionGroups.add("");
            }
        }
        return tileCollisionGroups;
    }

    public void setTileCollisionGroups(List<String> tileCollisionGroups) {
        int oldSize = this.tileCollisionGroups.size();
        int newSize = tileCollisionGroups.size();
        List<CollisionGroupNode> tileGroupNodes = new ArrayList<CollisionGroupNode>(newSize);
        List<CollisionGroupNode> sortedCollisionGroups = getCollisionGroups();
        Collections.sort(sortedCollisionGroups);
        for (int i = 0; i < newSize; ++i) {
            int index = Collections.binarySearch(sortedCollisionGroups, new CollisionGroupNode(tileCollisionGroups.get(i)));
            if (index >= 0) {
                tileGroupNodes.add(sortedCollisionGroups.get(index));
            } else {
                tileGroupNodes.add(null);
            }
        }
        if (oldSize != newSize || !this.tileCollisionGroups.equals(tileGroupNodes)) {
            this.tileCollisionGroups = tileGroupNodes;
            notifyChange();
        }
    }

    public float[] getConvexHullPoints() {
        return this.convexHullPoints;
    }

    public BufferedImage getLoadedImage() {
        return this.loadedImage;
    }

    public BufferedImage getLoadedCollision() {
        return this.loadedCollision;
    }

    private List<CollisionGroupNode> getCollisionGroups() {
        List<Node> children = getChildren();
        int count = children.size();
        List<CollisionGroupNode> collisionGroups = new ArrayList<CollisionGroupNode>(count);
        for (int i = 0; i < count; ++i) {
            collisionGroups.add((CollisionGroupNode) children.get(i));
        }
        return collisionGroups;
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null) {
            updateImage();
            updateCollision();
            updateConvexHulls();
            notifyChange();
        }
    }

    private void updateImage() {
        try {
            this.loadedImage = loadImageFile(this.image);
        } catch (Exception e) {
            this.loadedImage = null;
        }
    }

    private void updateCollision() {
        try {
            this.loadedCollision = loadImageFile(this.collision);
        } catch (Exception e) {
            this.loadedCollision = null;
        }
    }

    private BufferedImage loadImageFile(String fileName) throws Exception {
        if (getModel() == null) {
            return null;
        }
        try {
            IFile file = getModel().getFile(fileName);
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

    private IStatus verifyImageDimensions() {
        if (this.loadedImage != null && this.loadedCollision != null) {
            if (this.loadedImage.getWidth() != this.loadedCollision.getWidth() || this.loadedImage.getHeight() != this.loadedCollision.getHeight()) {
                return createErrorStatus(Messages.TileSetNode_DIFF_IMG_DIMS, new Object[] {
                        this.loadedImage.getWidth(),
                        this.loadedImage.getHeight(),
                        this.loadedCollision.getWidth(),
                        this.loadedCollision.getHeight() });
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
                    return createErrorStatus(Messages.TileSetNode_TILE_WIDTH_GT_IMG, new Object[] {
                            totalTileWidth, imageWidth });
                }
            }
            if (verifyHeight) {
                int totalTileHeight = this.tileHeight + this.tileMargin;
                if (totalTileHeight > imageHeight) {
                    return createErrorStatus(Messages.TileSetNode_TILE_HEIGHT_GT_IMG, new Object[] {
                            totalTileHeight, imageHeight });
                }
            }
        }
        return Status.OK_STATUS;
    }

    private IStatus createErrorStatus(String message, Object[] binding) {
        return new Status(IStatus.ERROR, Activator.PLUGIN_ID, NLS.bind(message, binding));
    }

    private void updateConvexHulls() {
        if (getModel() != null) {
            IStatus status = validate();
            if (status.getSeverity() <= IStatus.INFO && this.loadedCollision != null) {
                ConvexHulls result = TileSetUtil.calculateConvexHulls(loadedCollision.getAlphaRaster(), PLANE_COUNT,
                        loadedCollision.getWidth(), loadedCollision.getHeight(),
                        tileWidth, tileHeight, tileMargin, tileSpacing);

                int tileCount = result.hulls.length;
                this.convexHulls = Arrays.asList(result.hulls);
                // Add additional slots for tile collision groups (never shrink
                // the tile collision group list since we might lose edited
                // groups and break undo)
                for (int i = this.tileCollisionGroups.size(); i < tileCount; ++i) {
                    this.tileCollisionGroups.add(null);
                }
                // Simulate a shorter list (see convexHullCount and getConvexHulls)
                this.tileCollisionGroupCount = tileCount;
                this.convexHullPoints = result.points;
            }
        }
    }

    @Override
    public void handleReload(IFile file) {
        boolean result = false;
        if (image != null && image.length() > 0) {
            IFile imgFile = getModel().getFile(image);
            if (file.equals(imgFile)) {
                updateImage();
                result = true;
            }
        }
        if (collision != null && collision.length() > 0) {
            IFile collisionFile = getModel().getFile(collision);
            if (file.equals(collisionFile)) {
                updateCollision();
                result = true;
            }
        }
        if (result) {
            updateConvexHulls();
        }
    }

}
