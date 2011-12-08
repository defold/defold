package com.dynamo.cr.tileeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.awt.Color;
import java.io.IOException;
import java.util.ArrayList;
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

import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.cr.tileeditor.Activator;
import com.dynamo.cr.tileeditor.operations.AddCollisionGroupNodeOperation;
import com.dynamo.cr.tileeditor.operations.RemoveCollisionGroupNodeOperation;
import com.dynamo.cr.tileeditor.operations.SetTileCollisionGroupsOperation;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.Messages;
import com.dynamo.cr.tileeditor.scene.TileSetLoader;
import com.dynamo.cr.tileeditor.scene.TileSetNode;
import com.dynamo.cr.tileeditor.scene.TileSetNodePresenter;
import com.dynamo.tile.ConvexHull;
import com.dynamo.tile.proto.Tile.TileSet;

public class TileSetNodeTest extends AbstractNodeTest {

    private TileSetNode node;
    private TileSetLoader loader;
    private TileSetNodePresenter presenter;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();

        this.loader = new TileSetLoader();
        this.presenter = new TileSetNodePresenter();

        this.node = registerAndLoadNodeType(TileSetNode.class, "tileset", this.loader);
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
        return this.node.getChildren().size();
    }

    private CollisionGroupNode collisionGroup(int i) {
        return (CollisionGroupNode)this.node.getChildren().get(i);
    }

    private int convexHullCount() {
        return this.node.getConvexHulls().size();
    }

    private String tileCollisionGroup(int i) {
        return this.node.getTileCollisionGroups().get(i);
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
        execute(new AddCollisionGroupNodeOperation(this.node, collisionGroup));
        when(getPresenterContext().getSelection()).thenReturn(new StructuredSelection(collisionGroup));
        verifySelection();
        return collisionGroup;
    }

    private void removeCollisionGroup(CollisionGroupNode collisionGroup, int updateCount) throws ExecutionException {
        RemoveCollisionGroupNodeOperation op = new RemoveCollisionGroupNodeOperation(collisionGroup);
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
        assertThat(collisionGroup(0).getName(), is("default"));
        assertThat(convexHullCount(), is(0));
    }

    private void loadWithTileCollisionGroup(String group) throws Exception {
        String path = "/collision_groups.tileset";
        String img = "/2x5_16_1.png";
        StringBuffer ddf = new StringBuffer();
        ddf.append("image: \"").append(img).append("\" tile_width: 16 tile_height: 16 tile_margin: 1 tile_spacing: 0 ")
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

        assertThat(collisionGroup(0).getName(), is("default"));
        assertThat(tileCollisionGroup(0), is("default"));
    }

    @Test
    public void testLoadMissingTileCollisionGroup() throws Exception {
        loadWithTileCollisionGroup("default1");

        assertThat(collisionGroup(0).getName(), is("default"));
        assertThat(tileCollisionGroup(0), is(""));
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

        // Simulate reload
        IFile tileSetFile = copyFile(tileSetHalfPath, tileSetPath);
        this.node.handleReload(tileSetFile);

        List<ConvexHull> postHulls = this.node.getConvexHulls();

        assertNotSame(preHulls.size(), postHulls.size());
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
        assertThat(collisionGroup(1).getName(), is("default1"));

        undo();
        assertThat(collisionGroupCount(), is(1));
        verifySelection();

        redo();
        assertThat(collisionGroupCount(), is(2));
        assertThat(collisionGroup(1).getName(), is("default1"));
        verifySelection();
    }

    /**
     * Part of Use Case 1.1.4 - Add a Collision Group
     *
     * @throws IOException
     */
    @Test
    public void testPaintingOperation() throws Exception {
        // Requirements
        testCreate();
        addCollisionGroup();

        // Preconditions
        assertThat(tileCollisionGroup(1), is(""));
        assertThat(tileCollisionGroup(2), is(""));

        // simulate painting
        List<String> oldGroups = this.node.getTileCollisionGroups();
        List<String> newGroups = new ArrayList<String>(oldGroups);
        newGroups.set(1, "default1");
        newGroups.set(2, "default1");
        SetTileCollisionGroupsOperation op = new SetTileCollisionGroupsOperation(this.node, oldGroups, newGroups, "default1");
        execute(op);
        assertThat(tileCollisionGroup(1), is("default1"));
        assertThat(tileCollisionGroup(2), is("default1"));

        undo();
        assertThat(tileCollisionGroup(1), is(""));
        assertThat(tileCollisionGroup(2), is(""));

        redo();
        assertThat(tileCollisionGroup(1), is("default1"));
        assertThat(tileCollisionGroup(2), is("default1"));
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

        // simulate painting
        this.presenter.onBeginPaintTile(getPresenterContext());
        this.presenter.onPaintTile(getPresenterContext(), 1);
        verify(getPresenterContext(), times(1)).refreshView();
        this.presenter.onPaintTile(getPresenterContext(), 1);
        this.presenter.onPaintTile(getPresenterContext(), 2);
        verify(getPresenterContext(), times(2)).refreshView();
        this.presenter.onPaintTile(getPresenterContext(), 2);
        this.presenter.onEndPaintTile(getPresenterContext());
        verifyExecution();
    }

    /**
     * Use Case 1.1.5 - Rename Collision Group
     *
     * @throws IOException
     */
    @Test
    public void testRenameCollisionGroup() throws Exception {

        // requirement
        testPaintingOperation();

        // preconditions
        assertThat(tileCollisionGroup(1), is("default1"));

        // test
        setNodeProperty(collisionGroup(1), "name", "default2");
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
    public void testRenameToExistingName() throws Exception {

        // requirement
        testPaintingOperation();

        // setup
        // simulate painting
        List<String> oldGroups = this.node.getTileCollisionGroups();
        List<String> newGroups = new ArrayList<String>(oldGroups);
        newGroups.set(1, "default");
        SetTileCollisionGroupsOperation op = new SetTileCollisionGroupsOperation(this.node, oldGroups, newGroups, "default1");
        execute(op);

        // preconditions
        assertThat(tileCollisionGroup(1), is("default"));
        assertThat(tileCollisionGroup(2), is("default1"));

        // test
        setNodeProperty(collisionGroup(1), "name", "default");
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
        testPaintingOperation();

        // preconditions
        assertThat(tileCollisionGroup(1), is("default1"));

        // test
        removeCollisionGroup(collisionGroup(1), 2);
        assertThat(collisionGroupCount(), is(1));
        assertThat(tileCollisionGroup(1), is(""));

        undo();
        assertThat(collisionGroupCount(), is(2));
        assertThat(tileCollisionGroup(1), is("default1"));

        redo();
        assertThat(collisionGroupCount(), is(1));
        assertThat(tileCollisionGroup(1), is(""));
    }

    @Test
    public void testRemoveDuplicateCollisionGroup() throws Exception {

        // requirements
        testPaintingOperation();

        // preconditions
        assertThat(tileCollisionGroup(1), is("default1"));

        // generate duplicate
        setNodeProperty(collisionGroup(0), "name", "default1");

        // test
        removeCollisionGroup(collisionGroup(1), 2);
        assertThat(collisionGroupCount(), is(1));
        assertThat(tileCollisionGroup(1), is("default1"));

        undo();
        assertThat(collisionGroupCount(), is(2));
        assertThat(tileCollisionGroup(1), is("default1"));

        redo();
        assertThat(collisionGroupCount(), is(1));
        assertThat(tileCollisionGroup(1), is("default1"));

        // Finally rename node again
        setNodeProperty(collisionGroup(0), "name", "default");
        assertThat(tileCollisionGroup(1), is("default"));
    }

    /**
     * Use Case 1.1.9 - Save
     *
     * @throws IOException
     */
    @Test
    public void testSave() throws Exception {

        // requires
        testPaintingOperation();

        TileSet ddf = (TileSet) this.loader.buildMessage(getLoaderContext(), this.node, null);

        assertThat(ddf.getImage(), is(this.node.getImage()));
        assertThat(ddf.getTileWidth(), is(this.node.getTileWidth()));
        assertThat(ddf.getTileHeight(), is(this.node.getTileHeight()));
        assertThat(ddf.getTileMargin(), is(this.node.getTileMargin()));
        assertThat(ddf.getTileSpacing(), is(this.node.getTileSpacing()));
        assertThat(ddf.getCollision(), is(this.node.getCollision()));
        assertThat(ddf.getMaterialTag(), is(this.node.getMaterialTag()));
        assertThat(ddf.getImage(), is(this.node.getImage()));

        assertThat(ddf.getCollisionGroups(0), is(collisionGroup(0).getName()));

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

        // Empty material
        setProperty("materialTag", "");
        assertPropertyStatus("materialTag", IStatus.ERROR, Messages.TileSetNode_materialTag_EMPTY);

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
        for (int i = 1; i <= n; ++i) {
            addCollisionGroup();
            assertNodePropertyStatus(collisionGroup(i), "name", IStatus.OK, null);
        }
        CollisionGroupNode newGroup = addCollisionGroup();
        assertNodePropertyStatus(newGroup, "name", IStatus.WARNING, NLS.bind(Messages.CollisionGroupNode_name_OVERFLOW, n));
        // Clear message
        removeCollisionGroup(collisionGroup(n-1), 1);
        assertNodePropertyStatus(newGroup, "name", IStatus.OK, null);
    }

    @Test
    public void testCollisionGroupColors() throws Exception {
        CollisionGroupNode collisionGroup = collisionGroup(0);

        // Preconditions
        Color color = CollisionGroupNode.getCollisionGroupColor("default");
        assertThat(color, notNullValue());
        assertThat(CollisionGroupNode.getCollisionGroupColor("default1"), nullValue());

        // Rename
        setNodeProperty(collisionGroup, "name", "default1");
        assertThat(CollisionGroupNode.getCollisionGroupColor("default1"), is(color));
        assertThat(CollisionGroupNode.getCollisionGroupColor("default"), nullValue());

        // Remove
        removeCollisionGroup(collisionGroup, 1);
        assertThat(CollisionGroupNode.getCollisionGroupColor("default1"), nullValue());

        // Add
        addCollisionGroup();
        collisionGroup = collisionGroup(0);
        assertThat(CollisionGroupNode.getCollisionGroupColor("default"), is(color));

        // Full
        int n = Activator.MAX_COLLISION_GROUP_COUNT;
        for (int i = 1; i <= n; ++i) {
            addCollisionGroup();
            String name = collisionGroup(i).getName();
            Color newColor = CollisionGroupNode.getCollisionGroupColor(name);
            assertThat(newColor, not(color));
            assertThat(newColor, notNullValue());
        }
        CollisionGroupNode newGroup = addCollisionGroup();
        String name = newGroup.getName();
        Color newColor = CollisionGroupNode.getCollisionGroupColor(name);
        assertThat(newColor, nullValue());
    }
}
