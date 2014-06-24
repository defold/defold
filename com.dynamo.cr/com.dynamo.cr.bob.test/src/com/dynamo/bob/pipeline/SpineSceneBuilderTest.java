package com.dynamo.bob.pipeline;

import static org.junit.Assert.*;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.spine.proto.Spine;
import com.dynamo.spine.proto.Spine.AnimationSet;
import com.dynamo.spine.proto.Spine.SpineAnimation;
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

    private static boolean hasBone(Map<Long, Bone> bones, String id) {
        return bones.containsKey(MurmurHash.hash64(id));
    }

    private void assertSkeleton(Skeleton skeleton) {
        assertEquals(5, skeleton.getBonesCount());
        Map<Long, Bone> bones = new HashMap<Long, Bone>();
        for (Bone bone : skeleton.getBonesList()) {
            bones.put(bone.getId(), bone);
        }
        assertTrue(hasBone(bones, "root"));
        assertTrue(hasBone(bones, "bone_animated"));
        assertTrue(hasBone(bones, "bone_noscale"));
        assertTrue(hasBone(bones, "bone_rotated"));
        assertTrue(hasBone(bones, "bone_scale"));
    }

    private static boolean hasMesh(Map<Long, Mesh> meshes, String id) {
        return meshes.containsKey(MurmurHash.hash64(id));
    }

    private void assertMeshSet(MeshSet meshSet) {
        assertEquals(3, meshSet.getMeshesCount());
        Map<Long, Mesh> meshes = new HashMap<Long, Mesh>();
        for (Mesh mesh : meshSet.getMeshesList()) {
            meshes.put(mesh.getId(), mesh);
        }
        assertTrue(hasMesh(meshes, ""));
        assertTrue(hasMesh(meshes, "test_skin"));
        assertTrue(hasMesh(meshes, "test_skin2"));
    }

    private void assertAnim(SpineAnimation anim, boolean pos, boolean rot, boolean scale) {
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

    private static SpineAnimation getAnim(Map<Long, SpineAnimation> animations, String id) {
        return animations.get(MurmurHash.hash64(id));
    }

    private void assertAnimSet(AnimationSet animSet) {
        assertEquals(5, animSet.getAnimationsCount());
        Map<Long, SpineAnimation> anims = new HashMap<Long, SpineAnimation>();
        for (SpineAnimation anim : animSet.getAnimationsList()) {
            anims.put(anim.getId(), anim);
        }
        assertAnim(getAnim(anims, "anim_curve"), true, false, false);
        assertAnim(getAnim(anims, "anim_multi"), true, true, true);
        assertAnim(getAnim(anims, "anim_pos"), true, false, false);
        assertAnim(getAnim(anims, "anim_rot"), false, true, false);
        assertAnim(getAnim(anims, "anim_scale"), false, false, true);
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
