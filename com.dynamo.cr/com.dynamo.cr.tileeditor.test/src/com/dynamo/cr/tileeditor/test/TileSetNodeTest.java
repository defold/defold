package com.dynamo.cr.tileeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.osgi.util.NLS;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.tile.ConvexHull;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.RemoveChildrenOperation;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.cr.tileeditor.Activator;
import com.dynamo.cr.tileeditor.operations.AddAnimationNodeOperation;
import com.dynamo.cr.tileeditor.operations.AddCollisionGroupNodeOperation;
import com.dynamo.cr.tileeditor.operations.RemoveTileSetChildrenOperation;
import com.dynamo.cr.tileeditor.operations.SetTileCollisionGroupsOperation;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.Messages;
import com.dynamo.cr.tileeditor.scene.TileSetLoader;
import com.dynamo.cr.tileeditor.scene.TileSetNode;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.TileSet;

public class TileSetNodeTest extends AbstractNodeTest {

    private TileSetNode node;
    private TileSetLoader loader;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();

        this.loader = new TileSetLoader();

        this.node = registerAndLoadRoot(TileSetNode.class, "tileset", this.loader);
    }

    @After
    public void teardown() {
        // Handle static singleton
        CollisionGroupNode.clearCollisionGroups();
    }

    // Helpers

    private String image() {
        return this.node.getImage();
    }

    private int tileWidth() {
        return this.node.getTileWidth();
    }

    private int tileHeight() {
        return this.node.getTileHeight();
    }

    private int tileMargin() {
        return this.node.getTileMargin();
    }

    private int tileSpacing() {
        return this.node.getTileSpacing();
    }

    private String collision() {
        return this.node.getCollision();
    }

    private String materialTag() {
        return this.node.getMaterialTag();
    }

    private int collisionGroupCount() {
        return this.node.getCollisionGroups().size();
    }

    private CollisionGroupNode collisionGroup(int i) {
        return this.node.getCollisionGroups().get(i);
    }

    private int convexHullCount() {
        return this.node.getConvexHulls().size();
    }

    private String tileCollisionGroup(int i) {
        CollisionGroupNode group = this.node.getTileCollisionGroups().get(i);
        if (group != null) {
            return group.getId();
        } else {
            return null;
        }
    }

    private int animationCount() {
        return this.node.getAnimations().size();
    }

    private AnimationNode animation(int i) {
        return this.node.getAnimations().get(i);
    }

    private void setProperty(String id, Object value) throws Exception {
        setNodeProperty(this.node, id, value);
    }

    private void setProperties(String[] ids, Object[] values) throws Exception {
        for (int i = 0; i < ids.length; ++i) {
            setProperty(ids[i], values[i]);
        }
    }

    private void assertPropertyStatus(String id, int severity, String message) {
        assertNodePropertyStatus(this.node, id, severity, message);
    }

    private void assertPropertiesStatus(String[] ids, int severity, String message) {
        for (String id : ids) {
            assertPropertyStatus(id, severity, message);
        }
    }

    private CollisionGroupNode addCollisionGroup() throws ExecutionException {
        CollisionGroupNode collisionGroup = new CollisionGroupNode();
        execute(new AddCollisionGroupNodeOperation(this.node, collisionGroup, getPresenterContext()));
        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(collisionGroup));
        verifySelection();
        return collisionGroup;
    }

    private void removeCollisionGroup(CollisionGroupNode collisionGroup) throws ExecutionException {
        RemoveTileSetChildrenOperation op = new RemoveTileSetChildrenOperation(Collections.singletonList((Node)collisionGroup), getPresenterContext());
        execute(op);
        verifySelection();
    }

    private AnimationNode addAnimation() throws ExecutionException {
        AnimationNode animation = new AnimationNode();
        execute(new AddAnimationNodeOperation(this.node, animation, getPresenterContext()));
        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(animation));
        verifySelection();
        return animation;
    }

    private void removeAnimation(AnimationNode animation) throws ExecutionException {
        RemoveChildrenOperation op = new RemoveChildrenOperation(Collections.singletonList((Node)animation), getPresenterContext());
        execute(op);
        verifySelection();
    }

    // Tests

    /**
     * Test load
     * @throws IOException
     */
    @Test
    public void testLoad() throws IOException {
        assertThat(image(), is(""));
        assertThat(tileWidth(), is(16));
        assertThat(tileHeight(), is(16));
        assertThat(tileMargin(), is(0));
        assertThat(tileSpacing(), is(0));
        assertThat(collision(), is(""));
        assertThat(materialTag(), is("tile"));
        assertThat(collisionGroupCount(), is(1));
        assertThat(collisionGroup(0).getId(), is("default"));
        assertThat(convexHullCount(), is(0));
    }

    private void loadWithTileCollisionGroup(String group) throws Exception {
        String path = "/collision_groups.tileset";
        String img = "/2x5_16_1.png";
        StringBuffer ddf = new StringBuffer();
        ddf.append("image: \"").append(img).append("\" tile_width: 16 tile_height: 16 tile_margin: 0 tile_spacing: 1 ")
            .append("collision: \"").append(img).append("\" material_tag: \"tile\" collision_groups: \"default\" ");
        for (int i = 0; i < 10; ++i) {
            ddf.append("convex_hulls: {index: 0 count: 0 collision_group: \"").append(group).append("\"} ");
        }

        registerFile(path, ddf.toString());

        this.node = this.loader.load(getLoaderContext(), getFile(path).getContents());
        this.node.setModel(getModel());
    }

    @Test
    public void testLoadTileCollisionGroup() throws Exception {
        loadWithTileCollisionGroup("default");

        assertThat(collisionGroup(0).getId(), is("default"));
        assertThat(tileCollisionGroup(0), is("default"));
    }

    @Test
    public void testLoadMissingTileCollisionGroup() throws Exception {
        loadWithTileCollisionGroup("default1");

        assertThat(collisionGroup(0).getId(), is("default"));
        assertThat(tileCollisionGroup(0), nullValue());
    }

    private void loadWithAnimation(String path) throws Exception {
        String img = "/2x5_16_1.png";
        StringBuffer ddf = new StringBuffer();
        ddf.append("image: \"").append(img).append("\" tile_width: 16 tile_height: 16 tile_margin: 0 tile_spacing: 1 ")
            .append("collision: \"").append(img).append("\" material_tag: \"tile\" collision_groups: \"default\" ")
            .append("animations: {id: \"anim\" start_tile: 1 end_tile: 4 playback: PLAYBACK_ONCE_FORWARD ")
                .append("fps: 30 flip_horizontal: 1 flip_vertical: 1} extrude_borders: 0 inner_padding: 0");

        registerFile(path, ddf.toString());

        this.node = this.loader.load(getLoaderContext(), getFile(path).getContents());
        this.node.setModel(getModel());
    }

    @Test
    public void testLoadAnimation() throws Exception {
        String path = "/animations.tileset";
        loadWithAnimation(path);

        assertThat(animationCount(), is(1));
        AnimationNode animation = animation(0);
        assertThat(animation.getId(), is("anim"));
        assertThat(animation.getStartTile(), is(1));
        assertThat(animation.getEndTile(), is(4));
        assertThat(animation.getPlayback(), is(Tile.Playback.PLAYBACK_ONCE_FORWARD));
        assertThat(animation.getFps(), is(30));
        assertTrue(animation.isFlipHorizontally());
        assertTrue(animation.isFlipVertically());

        saveLoadCompare(this.loader, TileSet.newBuilder(), path);
    }

    @Test
    public void testLoadDuplicateCollisionGroupIds() throws Exception {
        String img = "/2x5_16_1.png";
        StringBuffer ddf = new StringBuffer();
        ddf.append("image: \"").append(img).append("\" tile_width: 16 tile_height: 16 tile_margin: 0 tile_spacing: 1 ")
            .append("collision: \"").append(img).append("\" material_tag: \"tile\" collision_groups: \"default\" collision_groups: \"default\"")
            .append("animations: {id: \"anim\" start_tile: 1 end_tile: 4 playback: PLAYBACK_ONCE_FORWARD ")
            .append("fps: 30 flip_horizontal: 0 flip_vertical: 0}")
            .append("convex_hulls: {index: 0 count: 3 collision_group: \"default\"} ");
        float[] points = new float[] {
                0.0f, 0.0f, 0.0f,
                1.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f
        };
        for (float f : points) {
            ddf.append("convex_hull_points: " + f + " ");
        }

        String path = "/faulty.tileset";
        registerFile(path, ddf.toString());

        this.node = this.loader.load(getLoaderContext(), getFile(path).getContents());
        this.node.setModel(getModel());

        assertThat(convexHullCount(), is(10));
        assertThat(tileCollisionGroup(0), is("default"));
    }

    /**
     * Use Case 1.1.1 - Create the Tile Set
     *
     * @throws IOException
     */
    @Test
    public void testCreate() throws Exception {

        String img = "/4x5_16_1.png";

        // Test all properties
        String[] ids = new String[] {"image", "tileWidth", "tileHeight", "tileMargin", "tileSpacing", "collision", "materialTag"};
        Object[] values = new Object[] {img, 17, 17, 1, 1, img, "tile2"};
        setProperties(ids, values);

        // Set properties for hull verification
        ids = new String[] {"tileWidth", "tileHeight", "tileMargin"};
        values = new Object[] {16, 16, 0};
        setProperties(ids, values);

        assertThat(convexHullCount(), is(20));
        for (int i = 0; i < 10; ++i) {
            assertThat(this.node.getConvexHulls().get(i).getCount(), is(8));
        }
        for (int i = 10; i < 14; ++i) {
            assertThat(this.node.getConvexHulls().get(i).getCount(), is(4));
        }
        for (int i = 14; i < 20; ++i) {
            assertThat(this.node.getConvexHulls().get(i).getCount(), is(8));
        }
    }

    /**
     * Use Case 1.1.3 - Edit collisions
     * @throws Exception
     */
    @Test
    public void testReloadCollision() throws Exception {
        String tileSetPath = "/4x5_16_1.png";
        String tileSetHalfPath = "/2x5_16_1.png";

        // Setup
        setProperty("image", tileSetPath);
        setProperty("collision", tileSetPath);
        List<ConvexHull> preHulls = this.node.getConvexHulls();
        BufferedImage preImage = this.node.getLoadedImage();

        // Simulate reload
        IFile tileSetFile = copyFile(tileSetHalfPath, tileSetPath);
        boolean result = this.node.handleReload(tileSetFile, false);
        assertTrue(result);

        List<ConvexHull> postHulls = this.node.getConvexHulls();
        BufferedImage postImage = this.node.getLoadedImage();

        assertNotSame(preHulls.size(), postHulls.size());
        assertNotSame(preImage.getHeight(), postImage.getHeight());
    }

    /**
     * Part of Use Case 1.1.4 - Add a Collision Group
     *
     * @throws IOException
     */
    @Test
    public void testAddCollisionGroup() throws Exception {

        // requirement
        testCreate();

        addCollisionGroup();
        assertThat(collisionGroupCount(), is(2));
        assertThat(collisionGroup(1).getId(), is("default1"));

        undo();
        assertThat(collisionGroupCount(), is(1));
        verifySelection();

        redo();
        assertThat(collisionGroupCount(), is(2));
        assertThat(collisionGroup(1).getId(), is("default1"));
        verifySelection();
    }

    /**
     * Part of Use Case 1.1.4 - Add a Collision Group
     *
     * @throws IOException
     */
    @Test
    public void testPainting() throws Exception {
        // Requirements
        testCreate();
        addCollisionGroup();
        CollisionGroupNode group = collisionGroup(1);

        // Preconditions
        assertThat(tileCollisionGroup(1), nullValue());
        assertThat(tileCollisionGroup(2), nullValue());

        // simulate painting
        List<CollisionGroupNode> oldGroups = this.node.getTileCollisionGroups();
        List<CollisionGroupNode> newGroups = new ArrayList<CollisionGroupNode>(oldGroups);
        newGroups.set(1, group);
        newGroups.set(2, group);
        SetTileCollisionGroupsOperation op = new SetTileCollisionGroupsOperation(this.node, oldGroups, newGroups, group);
        execute(op);
        assertThat(tileCollisionGroup(1), is("default1"));
        assertThat(tileCollisionGroup(2), is("default1"));

        undo();
        assertThat(tileCollisionGroup(1), nullValue());
        assertThat(tileCollisionGroup(2), nullValue());

        redo();
        assertThat(tileCollisionGroup(1), is("default1"));
        assertThat(tileCollisionGroup(2), is("default1"));
    }

    /**
     * Use Case 1.1.5 - Rename Collision Group
     *
     * @throws IOException
     */
    @Test
    public void testRenameCollisionGroup() throws Exception {

        // requirement
        testPainting();

        // preconditions
        assertThat(tileCollisionGroup(1), is("default1"));

        // test
        setNodeProperty(collisionGroup(1), "id", "default2");
        assertThat(tileCollisionGroup(1), is("default2"));

        undo();
        assertThat(tileCollisionGroup(1), is("default1"));

        redo();
        assertThat(tileCollisionGroup(1), is("default2"));
    }

    /**
     * Use Case 1.1.6 - Rename Collision Group to Existing Group
     *
     * @throws IOException
     */
    @Test
    public void testRenameCollisionGroupToExistingName() throws Exception {

        // requirement
        testPainting();

        // setup
        // simulate painting
        List<CollisionGroupNode> oldGroups = this.node.getTileCollisionGroups();
        List<CollisionGroupNode> newGroups = new ArrayList<CollisionGroupNode>(oldGroups);
        newGroups.set(1, collisionGroup(0));
        SetTileCollisionGroupsOperation op = new SetTileCollisionGroupsOperation(this.node, oldGroups, newGroups, collisionGroup(1));
        execute(op);

        // preconditions
        assertThat(tileCollisionGroup(1), is("default"));
        assertThat(tileCollisionGroup(2), is("default1"));

        // test
        setNodeProperty(collisionGroup(1), "id", "default");
        assertThat(tileCollisionGroup(1), is("default"));
        assertThat(tileCollisionGroup(2), is("default"));

        // undo
        undo();
        assertThat(tileCollisionGroup(1), is("default"));
        assertThat(tileCollisionGroup(2), is("default1"));

        // redo
        redo();
        assertThat(tileCollisionGroup(1), is("default"));
        assertThat(tileCollisionGroup(2), is("default"));
    }

    /**
     * Use Case 1.1.8 - Remove Collision Group
     *
     * @throws IOException
     */
    @Test
    public void testRemoveCollisionGroup() throws Exception {

        // requirements
        testPainting();

        // preconditions
        assertThat(tileCollisionGroup(1), is("default1"));

        // test
        removeCollisionGroup(collisionGroup(1));
        assertThat(collisionGroupCount(), is(1));
        assertThat(tileCollisionGroup(1), nullValue());

        undo();
        assertThat(collisionGroupCount(), is(2));
        assertThat(tileCollisionGroup(1), is("default1"));
        verifySelection();

        redo();
        assertThat(collisionGroupCount(), is(1));
        assertThat(tileCollisionGroup(1), nullValue());
        verifySelection();
    }

    @Test
    public void testRemoveDuplicateCollisionGroup() throws Exception {

        // requirements
        testPainting();

        // preconditions
        assertThat(tileCollisionGroup(1), is("default1"));

        // generate duplicate
        setNodeProperty(collisionGroup(0), "id", "default1");

        // test
        removeCollisionGroup(collisionGroup(1));
        assertThat(collisionGroupCount(), is(1));
        assertThat(tileCollisionGroup(1), is("default1"));

        undo();
        assertThat(collisionGroupCount(), is(2));
        assertThat(tileCollisionGroup(1), is("default1"));

        redo();
        assertThat(collisionGroupCount(), is(1));
        assertThat(tileCollisionGroup(1), is("default1"));

        // Finally rename node again
        setNodeProperty(collisionGroup(0), "id", "default");
        assertThat(tileCollisionGroup(1), is("default"));
    }

    @Test
    public void testAddAnimation() throws Exception {
        testCreate();

        assertThat(animationCount(), is(1));

        addAnimation();
        assertThat(animationCount(), is(2));
        assertThat(animation(1).getId(), is("anim1"));

        undo();
        assertThat(animationCount(), is(1));
        verifySelection();

        redo();
        assertThat(animationCount(), is(2));
        assertThat(animation(1).getId(), is("anim1"));
        verifySelection();
    }

    @Test
    public void testRemoveAnimation() throws Exception {
        testCreate();

        assertThat(animationCount(), is(1));

        removeAnimation(animation(0));
        assertThat(animationCount(), is(0));

        undo();
        assertThat(animationCount(), is(1));
        verifySelection();

        redo();
        assertThat(animationCount(), is(0));
        verifySelection();
    }

    /**
     * Use Case 1.1.9 - Save
     *
     * @throws IOException
     */
    @Test
    public void testSave() throws Exception {

        // requires
        testPainting();

        TileSet ddf = (TileSet) this.loader.buildMessage(getLoaderContext(), this.node, null);

        assertThat(ddf.getImage(), is(this.node.getImage()));
        assertThat(ddf.getTileWidth(), is(this.node.getTileWidth()));
        assertThat(ddf.getTileHeight(), is(this.node.getTileHeight()));
        assertThat(ddf.getTileMargin(), is(this.node.getTileMargin()));
        assertThat(ddf.getTileSpacing(), is(this.node.getTileSpacing()));
        assertThat(ddf.getCollision(), is(this.node.getCollision()));
        assertThat(ddf.getMaterialTag(), is(this.node.getMaterialTag()));
        assertThat(ddf.getImage(), is(this.node.getImage()));

        assertThat(ddf.getCollisionGroups(0), is(collisionGroup(0).getId()));

        assertThat(ddf.getConvexHulls(1).getCollisionGroup(), is(tileCollisionGroup(1)));
    }

    @Test
    public void testMessages() throws Exception {
        assertPropertyStatus("image", IStatus.INFO, Messages.TileSetNode_image_EMPTY);
        assertPropertyStatus("collision", IStatus.INFO, Messages.TileSetNode_collision_EMPTY);

        String missingPath = "/missing.png";
        // Image not found
        setProperty("image", missingPath);
        assertPropertyStatus("image", IStatus.ERROR, NLS.bind(Messages.TileSetNode_image_NOT_FOUND, missingPath));
        assertPropertyStatus("collision", IStatus.OK, null);

        // Collision not found
        setProperty("collision", "/missing.png");
        assertPropertyStatus("collision", IStatus.ERROR, NLS.bind(Messages.TileSetNode_collision_NOT_FOUND, missingPath));

        // Different img dimensions
        setProperty("image", "/4x5_16_1.png");
        setProperty("collision", "/2x5_16_1.png");
        String message = NLS.bind(Messages.TileSetNode_DIFF_IMG_DIMS, new Object[] {
                84, 67, 84, 33 });
        assertPropertyStatus("image", IStatus.ERROR, message);
        assertPropertyStatus("collision", IStatus.ERROR, message);

        // Invalid width/height
        setProperty("tileWidth", 0);
        assertPropertyStatus("tileWidth", IStatus.ERROR, null); // Messages.SceneModel_RangeValidator_tileWidth);
        setProperty("tileHeight", 0);
        assertPropertyStatus("tileHeight", IStatus.ERROR, null); // Messages.SceneModel_RangeValidator_tileHeight);

        // Total tile width greater than image width
        setProperties(new String[] {"collision", "tileWidth", "tileHeight", "tileMargin"},
                new Object[] {"/4x5_16_1.png", 85, 16, 1});
        message = NLS.bind(Messages.TileSetNode_TILE_WIDTH_GT_IMG, new Object[] {
                86, 84 });
        assertPropertiesStatus(new String[] { "image", "tileWidth",
                "tileMargin", "collision" }, IStatus.ERROR, message);
        assertPropertiesStatus(new String[] { "tileHeight", "tileSpacing" }, IStatus.OK, null);
        setProperty("tileMargin", 0);
        message = NLS.bind(Messages.TileSetNode_TILE_WIDTH_GT_IMG, new Object[] {
                85, 84 });
        assertPropertiesStatus(new String[] { "image", "tileWidth", "collision" }, IStatus.ERROR, message);
        assertPropertiesStatus(new String[] { "tileHeight", "tileMargin",
        "tileSpacing" }, IStatus.OK, null);

        // Total tile height greater than image width
        setProperties(new String[] { "tileWidth", "tileHeight", "tileMargin" }, new Object[] {
                16, 68, 1 });
        message = NLS.bind(Messages.TileSetNode_TILE_HEIGHT_GT_IMG, new Object[] {
                69, 67 });
        assertPropertiesStatus(new String[] { "image", "tileHeight",
                "tileMargin", "collision" }, IStatus.ERROR, message);
        assertPropertiesStatus(new String[] { "tileWidth", "tileSpacing" }, IStatus.OK, null);
        setProperty("tileMargin", 0);
        message = NLS.bind(Messages.TileSetNode_TILE_HEIGHT_GT_IMG, new Object[] {
                68, 67 });
        assertPropertiesStatus(new String[] { "image", "tileHeight",
        "collision" }, IStatus.ERROR, message);
        assertPropertiesStatus(new String[] { "tileWidth", "tileMargin",
        "tileSpacing" }, IStatus.OK, null);

        // Invalid tile margin
        setProperty("tileMargin", -1);
        assertPropertyStatus("tileMargin", IStatus.ERROR, Messages.TileSetNode_tileMargin_OUTSIDE_RANGE);

        // Invalid tile spacing
        setProperty("tileSpacing", -1);
        assertPropertyStatus("tileSpacing", IStatus.ERROR, Messages.TileSetNode_tileSpacing_OUTSIDE_RANGE);

    }

    @Test
    public void testImages() throws Exception {
        assertThat(this.node.getLoadedImage(), nullValue());
        assertThat(this.node.getLoadedCollision(), nullValue());

        testCreate();
        assertThat(this.node.getLoadedImage(), notNullValue());
        assertThat(this.node.getLoadedCollision(), notNullValue());

        setProperty("image", "");
        setProperty("collision", "");
        assertThat(this.node.getLoadedImage(), nullValue());
        assertThat(this.node.getLoadedCollision(), nullValue());
    }

    @Test
    public void testCollisionGroupGeneration() throws Exception {
        assertThat(this.node.getConvexHullPoints().length, is(0));
        assertThat(this.node.getConvexHulls().size(), is(0));
        assertThat(this.node.getTileCollisionGroups().size(), is(0));

        testCreate();
        assertThat(this.node.getConvexHullPoints().length, not(0));
        assertThat(this.node.getConvexHulls().size(), not(0));
        assertThat(this.node.getTileCollisionGroups().size(), not(0));

        setProperty("collision", "");
        assertThat(this.node.getConvexHullPoints().length, is(0));
        assertThat(this.node.getConvexHulls().size(), is(0));
        assertThat(this.node.getTileCollisionGroups().size(), is(0));
    }

    @Test
    public void testCollisionGroupMessages() throws Exception {
        // Too many collision groups
        int n = Activator.MAX_COLLISION_GROUP_COUNT;
        for (int i = 1; i < n; ++i) {
            CollisionGroupNode collisionGroup = addCollisionGroup();
            assertNodePropertyStatus(collisionGroup, "id", IStatus.OK, null);
        }
        CollisionGroupNode newGroup = addCollisionGroup();
        assertNodePropertyStatus(newGroup, "id", IStatus.WARNING, NLS.bind(Messages.CollisionGroupNode_id_OVERFLOW, n));
        // Clear message
        removeCollisionGroup(collisionGroup(n-1));
        assertNodePropertyStatus(newGroup, "id", IStatus.OK, null);
    }

    @Test
    public void testCollisionGroupColors() throws Exception {
        CollisionGroupNode collisionGroup = collisionGroup(0);

        // Preconditions
        Color color = CollisionGroupNode.getCollisionGroupColor("default");
        assertThat(color, notNullValue());
        assertThat(CollisionGroupNode.getCollisionGroupColor("default1"), nullValue());

        // Rename
        setNodeProperty(collisionGroup, "id", "default1");
        assertThat(CollisionGroupNode.getCollisionGroupColor("default1"), is(color));
        assertThat(CollisionGroupNode.getCollisionGroupColor("default"), nullValue());

        // Remove
        removeCollisionGroup(collisionGroup);
        assertThat(CollisionGroupNode.getCollisionGroupColor("default1"), nullValue());

        // Add
        addCollisionGroup();
        collisionGroup = collisionGroup(0);
        assertThat(CollisionGroupNode.getCollisionGroupColor("default"), is(color));

        // Full
        int n = Activator.MAX_COLLISION_GROUP_COUNT;
        for (int i = 1; i < n; ++i) {
            addCollisionGroup();
            String id = collisionGroup(i).getId();
            Color newColor = CollisionGroupNode.getCollisionGroupColor(id);
            assertThat(newColor, not(color));
            assertThat(newColor, notNullValue());
        }
        CollisionGroupNode newGroup = addCollisionGroup();
        String id = newGroup.getId();
        Color newColor = CollisionGroupNode.getCollisionGroupColor(id);
        assertThat(newColor, nullValue());
    }

    @Test
    public void testAnimationMessages() throws Exception {
        loadWithAnimation("/animation.tileset");
        AnimationNode animation = animation(0);

        assertNodePropertyStatus(animation, "startTile", IStatus.OK, null);
        animation.setStartTile(11);
        assertNodePropertyStatus(animation, "startTile", IStatus.ERROR, NLS.bind(Messages.AnimationNode_startTile_INVALID, 10));

        assertNodePropertyStatus(animation, "endTile", IStatus.OK, null);
        animation.setEndTile(11);
        assertNodePropertyStatus(animation, "endTile", IStatus.ERROR, NLS.bind(Messages.AnimationNode_endTile_INVALID, 10));
    }

}
