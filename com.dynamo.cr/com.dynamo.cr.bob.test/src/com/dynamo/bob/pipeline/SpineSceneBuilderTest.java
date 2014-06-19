package com.dynamo.bob.pipeline;

import static org.junit.Assert.*;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.spine.proto.Spine;
import com.dynamo.spine.proto.Spine.AnimationSet;
import com.dynamo.spine.proto.Spine.Animation;
import com.dynamo.spine.proto.Spine.AnimationTrack;
import com.dynamo.spine.proto.Spine.Bone;
import com.dynamo.spine.proto.Spine.MeshSet;
import com.dynamo.spine.proto.Spine.Mesh;
import com.dynamo.spine.proto.Spine.Skeleton;
import com.google.protobuf.Message;

public class SpineSceneBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    private void assertSkeleton(Skeleton skeleton) {
        assertEquals(5, skeleton.getBonesCount());
        Map<String, Bone> bones = new HashMap<String, Bone>();
        for (Bone bone : skeleton.getBonesList()) {
            bones.put(bone.getId(), bone);
        }
        assertTrue(bones.containsKey("root"));
        assertTrue(bones.containsKey("bone_animated"));
        assertTrue(bones.containsKey("bone_noscale"));
        assertTrue(bones.containsKey("bone_rotated"));
        assertTrue(bones.containsKey("bone_scale"));
    }

    private void assertMeshSet(MeshSet meshSet) {
        assertEquals(3, meshSet.getMeshesCount());
        Map<String, Mesh> meshes = new HashMap<String, Mesh>();
        for (Mesh mesh : meshSet.getMeshesList()) {
            meshes.put(mesh.getName(), mesh);
        }
        assertTrue(meshes.containsKey(""));
        assertTrue(meshes.containsKey("test_skin"));
        assertTrue(meshes.containsKey("test_skin2"));
    }

    private void assertAnim(Animation anim, boolean pos, boolean rot, boolean scale) {
        assertTrue(anim != null);
        assertEquals(1, anim.getTracksCount());
        assertEquals(1.0f, anim.getDuration(), 0.0f);
        assertEquals(30.0f, anim.getSampleRate(), 0.0f);
        AnimationTrack track = anim.getTracks(0);
        int sampleCount = (int)Math.floor(anim.getDuration() * anim.getSampleRate()) + 1;
        int posCount = 3 * sampleCount;
        if (!pos) {
            posCount = 0;
        }
        int rotCount = 4 * sampleCount;
        if (!rot) {
            rotCount = 0;
        }
        int scaleCount = 3 * sampleCount;
        if (!scale) {
            scaleCount = 0;
        }
        assertEquals(posCount, track.getPositionsCount());
        assertEquals(rotCount, track.getRotationsCount());
        assertEquals(scaleCount, track.getScaleCount());
    }

    private void assertAnimSet(AnimationSet animSet) {
        assertEquals(5, animSet.getAnimationsCount());
        Map<String, Animation> anims = new HashMap<String, Animation>();
        for (Animation anim : animSet.getAnimationsList()) {
            anims.put(anim.getName(), anim);
        }
        assertAnim(anims.get("anim_curve"), true, false, false);
        assertAnim(anims.get("anim_multi"), true, true, true);
        assertAnim(anims.get("anim_pos"), true, false, false);
        assertAnim(anims.get("anim_rot"), false, true, false);
        assertAnim(anims.get("anim_scale"), false, false, true);
    }

    @Test
    public void testSpineScene() throws Exception {
        addImage("/test.png", 16, 16);
        StringBuilder src = new StringBuilder();
        src.append("images { image:  \"/test.png\" }");
        addFile("/skeleton_atlas.atlas", src.toString());
        src = new StringBuilder();
        src.append("spine_json: \"/skeleton.json\"");
        src.append(" atlas: \"/skeleton_atlas.atlas\"");
        List<Message> outputs = build("/test.spinescene", src.toString());
        Spine.SpineScene scene = (Spine.SpineScene)outputs.get(0);

        assertSkeleton(scene.getSkeleton());
        assertMeshSet(scene.getMeshSet());
        assertAnimSet(scene.getAnimationSet());
    }
}
