package com.dynamo.cr.spine.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doAnswer;

import java.awt.image.BufferedImage;
import java.io.IOException;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.osgi.util.NLS;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneUtil;
import com.dynamo.cr.sceneed.core.test.AbstractNodeTest;
import com.dynamo.cr.spine.scene.Messages;
import com.dynamo.cr.spine.scene.SpineBoneNode;
import com.dynamo.cr.spine.scene.SpineModelLoader;
import com.dynamo.cr.spine.scene.SpineModelNode;
import com.dynamo.cr.tileeditor.scene.AtlasLoader;
import com.dynamo.cr.tileeditor.scene.AtlasNode;
import com.dynamo.spine.proto.Spine.SpineModelDesc;

public class SpineModelTest extends AbstractNodeTest {

    private SpineModelLoader loader;
    private SpineModelNode spineModelNode;

    @Override
    @Before
    public void setup() throws CoreException, IOException {
        super.setup();

        this.loader = new SpineModelLoader();

        String[] paths = new String[] {"/empty.atlas", "/default.atlas", "/invalid.atlas"};
        String[] contents = new String[] {
                "",
                "animations { id: \"test_sprite\" images { image: \"/16x16_1.png\" } } animations { id: \"test_sprite2\" images { image: \"/16x16_2.png\" } }",
                "animations { id: \"test_sprite\" images { image: \"/non_existant.png\" } } animations { id: \"test_sprite2\" images { image: \"/16x16_2.png\" } }",
        };
        for (int i = 0; i < paths.length; ++i) {
            String path = paths[i];
            registerFile(path, contents[i]);
            AtlasNode atlas = new AtlasLoader().load(getLoaderContext(), getFile(path).getContents());
            atlas.setModel(getModel());
            registerLoadedNode(path, atlas);
        }

        this.spineModelNode = registerAndLoadRoot(SpineModelNode.class, "spinemodel", this.loader);
    }

    // Helpers

    private void setProperty(String id, Object value) throws ExecutionException {
        setNodeProperty(this.spineModelNode, id, value);
    }

    private void assertPropertyStatus(String id, int severity, String message) {
        assertNodePropertyStatus(this.spineModelNode, id, severity, message);
    }

    // Tests

    @Test
    public void testLoad() throws Exception {
        assertThat(this.spineModelNode.getSpineScene(), is(""));
        assertThat(this.spineModelNode.getAtlas(), is(""));
        assertThat(this.spineModelNode.getDefaultAnimation(), is(""));
        assertThat(this.spineModelNode.getSkin(), is(""));
    }

    private void create() throws Exception {
        setProperty("spineScene", "/skeleton.json");
        setProperty("atlas", "/default.atlas");
        setProperty("defaultAnimation", "anim_pos");
    }

    @Test
    public void testBuildMessage() throws Exception {

        create();

        SpineModelDesc ddf = (SpineModelDesc)this.loader.buildMessage(getLoaderContext(), this.spineModelNode, null);

        assertThat(ddf.getAtlas(), is(this.spineModelNode.getAtlas()));
        assertThat(ddf.getDefaultAnimation(), is(this.spineModelNode.getDefaultAnimation()));
    }

    private static SpineBoneNode getBone(Node parent, int index) {
        return (SpineBoneNode)parent.getChildren().get(index);
    }

    private static void assertBone(Node parent, String id, int index) {
        SpineBoneNode node = getBone(parent, index);
        assertEquals(id, node.getId());
    }

    @Test
    public void testBoneHierarchy() throws Exception {
        create();
        assertEquals(1, this.spineModelNode.getChildren().size());
        SpineBoneNode root = getBone(this.spineModelNode, 0);
        assertEquals("root", root.getId());
        assertEquals(4, root.getChildren().size());
        assertBone(root, "bone_animated", 0);
        assertBone(root, "bone_noscale", 1);
        assertBone(root, "bone_rotated", 2);
        assertBone(root, "bone_scale", 3);
    }

    @Test
    public void testReloadSpineScene() throws Exception {
        create();

        assertTrue(this.spineModelNode.handleReload(getFile("/skeleton.json"), false));
    }

    @Test
    public void testReloadAtlas() throws Exception {
        create();

        assertTrue(this.spineModelNode.handleReload(getFile("/default.atlas"), false));
    }

    @Test
    public void testReloadAtlasImage() throws Exception {
        create();

        assertTrue(this.spineModelNode.handleReload(getFile("/16x16_1.png"), false));
    }

    @Test
    public void testMessages() throws Exception {
        assertPropertyStatus("atlas", IStatus.INFO, null); // default message

        setProperty("atlas", "/non_existant");
        assertPropertyStatus("atlas", IStatus.ERROR, null); // default message

        setProperty("atlas", "/invalid.atlas");
        assertPropertyStatus("atlas", IStatus.ERROR, Messages.SpineModelNode_atlas_INVALID_REFERENCE);

        registerFile("/test.test", "");
        setProperty("atlas", "/test.test");
        assertPropertyStatus("atlas", IStatus.ERROR, Messages.SpineModelNode_atlas_CONTENT_ERROR);

        setProperty("spineScene", "/skeleton.json");
        setProperty("atlas", "/empty.atlas");
        assertPropertyStatus("atlas", IStatus.ERROR, NLS.bind(Messages.SpineModelNode_atlas_MISSING_ANIMS, "test_sprite"));

        setProperty("spineScene", "/empty.json");
        setProperty("defaultAnimation", "test");
        assertPropertyStatus("defaultAnimation", IStatus.ERROR, NLS.bind(Messages.SpineModelNode_defaultAnimation_INVALID, "test"));

        setProperty("skin", "test");
        assertPropertyStatus("skin", IStatus.ERROR, NLS.bind(Messages.SpineModelNode_skin_INVALID, "test"));
    }

}
