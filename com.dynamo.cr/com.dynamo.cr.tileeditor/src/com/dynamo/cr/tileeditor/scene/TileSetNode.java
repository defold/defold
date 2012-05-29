package com.dynamo.cr.tileeditor.scene;

import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
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
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.util.TextureHandle;
import com.dynamo.tile.ConvexHull;
import com.dynamo.tile.TileSetUtil;
import com.dynamo.tile.TileSetUtil.ConvexHulls;

@SuppressWarnings("serial")
public class TileSetNode extends Node {

    // TODO: Should be configurable
    private static final int PLANE_COUNT = 16;

    @Property(editorType=EditorType.RESOURCE, extensions={"jpg", "png"})
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

    @Property(editorType=EditorType.RESOURCE, extensions={"jpg", "png"})
    @Resource
    private String collision = "";

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    private String materialTag = "";

    @Property
    @Range(min=0)
    private float minEdgeLength;

    private List<CollisionGroupNode> tileCollisionGroups;
    // Used to simulate a shorter list of convex hulls.
    // convexHulls need to be retained in some cases to keep collision groups and keep undo functionality
    private int tileCollisionGroupCount;
    private List<ConvexHull> convexHulls;
    private float[] convexHullPoints = new float[0];

    // Graphics resources
    private BufferedImage loadedImage;
    private BufferedImage loadedCollision;
    private TextureHandle textureHandle;

    public TileSetNode() {
        this.tileCollisionGroups = new ArrayList<CollisionGroupNode>();
        this.tileCollisionGroupCount = 0;
        this.convexHulls = new ArrayList<ConvexHull>();

        this.textureHandle = new TextureHandle();
    }

    @Override
    public void dispose() {
        super.dispose();
        this.textureHandle.clear();
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
        }
    }

    public int getTileSpacing() {
        return this.tileSpacing;
    }

    public void setTileSpacing(int tileSpacing) {
        if (this.tileSpacing != tileSpacing) {
            this.tileSpacing = tileSpacing;
            updateConvexHulls();
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
        }
    }

    public String getMaterialTag() {
        return this.materialTag;
    }

    public void setMaterialTag(String materialTag) {
        this.materialTag = materialTag;
    }

    public float getMinEdgeLength() {
        return this.minEdgeLength;
    }

    public void setMinEdgeLength(float minEdgeLength) {
        if (this.minEdgeLength != minEdgeLength) {
            this.minEdgeLength = minEdgeLength;
            updateConvexHulls();
        }
    }

    @Override
    protected void childAdded(Node child) {
        sortChildren();
    }

    public List<CollisionGroupNode> getCollisionGroups() {
        List<CollisionGroupNode> groups = new ArrayList<CollisionGroupNode>();
        for (Node child : getChildren()) {
            if (child instanceof CollisionGroupNode) {
                groups.add((CollisionGroupNode)child);
            }
        }
        return groups;
    }

    public List<AnimationNode> getAnimations() {
        List<AnimationNode> animations = new ArrayList<AnimationNode>();
        for (Node child : getChildren()) {
            if (child instanceof AnimationNode) {
                animations.add((AnimationNode)child);
            }
        }
        return animations;
    }

    public List<ConvexHull> getConvexHulls() {
        return Collections.unmodifiableList(this.convexHulls);
    }

    public List<CollisionGroupNode> getTileCollisionGroups() {
        int n = this.tileCollisionGroupCount;
        List<CollisionGroupNode> result = new ArrayList<CollisionGroupNode>(n);
        for (int i = 0; i < n; ++i) {
            CollisionGroupNode group = this.tileCollisionGroups.get(i);
            if (group != null && group.getParent() != null) {
                result.add(group);
            } else {
                result.add(null);
            }
        }
        return result;
    }

    public void setTileCollisionGroups(List<CollisionGroupNode> tileCollisionGroups) {
        this.tileCollisionGroups = new ArrayList<CollisionGroupNode>(tileCollisionGroups);
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

    public TextureHandle getTextureHandle() {
        return this.textureHandle;
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null) {
            updateImage();
            updateCollision();
            updateConvexHulls();
        }
    }

    private void updateImage() {
        try {
            this.loadedImage = loadImageFile(this.image);
            this.textureHandle.setImage(this.loadedImage);
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

    public void updateConvexHulls() {
        if (getModel() != null) {
            ConvexHulls result = null;
            if (this.loadedCollision != null && this.tileWidth > 0 && this.tileHeight > 0 && this.tileMargin >= 0 && this.tileSpacing >= 0) {
                result = TileSetUtil.calculateConvexHulls(loadedCollision.getAlphaRaster(), PLANE_COUNT,
                    loadedCollision.getWidth(), loadedCollision.getHeight(),
                    tileWidth, tileHeight, tileMargin, tileSpacing, minEdgeLength);
            }
            if (result != null) {
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
                return;
            }
        }
        this.convexHullPoints = new float[0];
        if (!this.convexHulls.isEmpty()) {
            this.convexHulls = new ArrayList<ConvexHull>();
        }
        this.tileCollisionGroupCount = 0;
    }

    @Override
    public boolean handleReload(IFile file) {
        boolean reloaded = false;
        if (image != null && image.length() > 0) {
            IFile imgFile = getModel().getFile(image);
            if (file.equals(imgFile)) {
                updateImage();
                reloaded = true;
            }
        }
        if (collision != null && collision.length() > 0) {
            IFile collisionFile = getModel().getFile(collision);
            if (file.equals(collisionFile)) {
                updateCollision();
                reloaded = true;
            }
        }
        if (reloaded) {
            updateConvexHulls();
        }
        return reloaded;
    }

    public int calculateTileCount() {
        int imageWidth = 0;
        int imageHeight = 0;
        if (this.loadedImage != null) {
            imageWidth = this.loadedImage.getWidth();
            imageHeight = this.loadedImage.getHeight();
        } else if (this.loadedCollision != null) {
            imageWidth = this.loadedCollision.getWidth();
            imageHeight = this.loadedCollision.getHeight();
        }
        if (imageWidth > 0 && imageHeight > 0) {
            int tilesPerRow = TileSetUtil.calculateTileCount(this.tileWidth, imageWidth, this.tileMargin, this.tileSpacing);
            int tilesPerColumn = TileSetUtil.calculateTileCount(this.tileHeight, imageHeight, this.tileMargin, this.tileSpacing);
            return tilesPerRow * tilesPerColumn;
        }
        return 0;
    }

    public String getChildId(Node node) {
        if (node instanceof AnimationNode) {
            return ((AnimationNode)node).getId();
        } else if (node instanceof CollisionGroupNode) {
            return ((CollisionGroupNode)node).getId();
        } else {
            return null;
        }
    }

    public void sortChildren() {
        sortChildren(new Comparator<Node>() {
            @Override
            public int compare(Node o1, Node o2) {
                if ((o1 instanceof AnimationNode && o2 instanceof CollisionGroupNode)
                        || (o1 instanceof CollisionGroupNode && o2 instanceof AnimationNode)) {
                    if (o1 instanceof CollisionGroupNode) {
                        return 1;
                    } else {
                        return -1;
                    }
                } else {
                    String id1 = getChildId(o1);
                    String id2 = getChildId(o2);
                    return id1.compareTo(id2);
                }

            }
        });
    }

    private void writeObject(ObjectOutputStream out) throws IOException {
        out.writeObject(this.image);
        out.writeInt(this.tileWidth);
        out.writeInt(this.tileHeight);
        out.writeInt(this.tileMargin);
        out.writeInt(this.tileSpacing);
        out.writeObject(this.collision);
        out.writeObject(this.materialTag);
        out.writeObject(this.tileCollisionGroups);
    }

    @SuppressWarnings({"unchecked"})
    private void readObject(ObjectInputStream in) throws IOException, ClassNotFoundException {
        this.image = (String)in.readObject();
        this.tileWidth = in.readInt();
        this.tileHeight = in.readInt();
        this.tileMargin = in.readInt();
        this.tileSpacing = in.readInt();
        this.collision = (String)in.readObject();
        this.materialTag = (String)in.readObject();
        this.tileCollisionGroups = (List<CollisionGroupNode>)in.readObject();
        this.textureHandle = new TextureHandle();
        this.convexHulls = new ArrayList<ConvexHull>();
    }
}
