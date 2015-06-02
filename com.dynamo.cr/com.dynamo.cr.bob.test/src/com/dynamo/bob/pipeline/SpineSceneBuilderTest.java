package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.atlas.proto.AtlasProto.Atlas;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.spine.proto.Spine;
import com.dynamo.spine.proto.Spine.AnimationSet;
import com.dynamo.spine.proto.Spine.AnimationTrack;
import com.dynamo.spine.proto.Spine.Bone;
import com.dynamo.spine.proto.Spine.EventTrack;
import com.dynamo.spine.proto.Spine.Mesh;
import com.dynamo.spine.proto.Spine.MeshEntry;
import com.dynamo.spine.proto.Spine.MeshSet;
import com.dynamo.spine.proto.Spine.Skeleton;
import com.dynamo.spine.proto.Spine.SpineAnimation;
import com.google.protobuf.Message;

public class SpineSceneBuilderTest extends AbstractProtoBuilderTest {

    private static final double EPSILON = 0.000001f;

    @Before
    public void setup() {
        addTestFiles();
    }

    private static void assertBone(Skeleton skeleton, int index, String id, boolean inheritScale) {
        Bone bone = skeleton.getBones(index);
        assertEquals(bone.getId(), MurmurHash.hash64(id));
        assertEquals(bone.getInheritScale(), inheritScale);
    }

    private static void assertBone(Skeleton skeleton, int index, String id) {
        assertBone(skeleton, index, id, true);
    }

    private void assertSkeleton(Skeleton skeleton) {
        assertEquals(9, skeleton.getBonesCount());
        // Verify depth-first order
        assertBone(skeleton, 0, "root");
        assertBone(skeleton, 1, "bone_animated");
        assertBone(skeleton, 2, "bone_animated_child");
        assertBone(skeleton, 3, "bone_animated_child_2");
        assertBone(skeleton, 4, "bone_noscale", false);
        assertBone(skeleton, 5, "bone_noscale_child");
        assertBone(skeleton, 6, "bone_noscale_child_2");
        assertBone(skeleton, 7, "bone_rotated");
        assertBone(skeleton, 8, "bone_scale", true);
    }

    private static boolean hasMeshEntry(Map<Long, MeshEntry> meshes, String id) {
        return meshes.containsKey(MurmurHash.hash64(id));
    }

    private void assertMeshSet(MeshSet meshSet) {
        assertEquals(3, meshSet.getMeshEntriesCount());
        Map<Long, MeshEntry> meshes = new HashMap<Long, MeshEntry>();
        for (MeshEntry meshEntry : meshSet.getMeshEntriesList()) {
            meshes.put(meshEntry.getId(), meshEntry);
        }
        assertTrue(hasMeshEntry(meshes, ""));
        assertTrue(hasMeshEntry(meshes, "test_skin"));
        assertTrue(hasMeshEntry(meshes, "test_skin2"));
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
        assertEquals(8, animSet.getAnimationsCount());
        Map<Long, SpineAnimation> anims = new HashMap<Long, SpineAnimation>();
        for (SpineAnimation anim : animSet.getAnimationsList()) {
            anims.put(anim.getId(), anim);
        }
        assertAnim(getAnim(anims, "anim_curve"), true, false, false);
        assertAnim(getAnim(anims, "anim_multi"), true, true, true);
        assertAnim(getAnim(anims, "anim_pos"), true, false, false);
        assertAnim(getAnim(anims, "anim_rot"), false, true, false);
        assertAnim(getAnim(anims, "anim_scale"), false, false, true);
        assertAnim(getAnim(anims, "anim_stepped"), true, false, false);

        SpineAnimation animTrackOrder = getAnim(anims, "anim_track_order");
        assertEquals(0, animTrackOrder.getTracks(0).getBoneIndex());
        assertEquals(1, animTrackOrder.getTracks(1).getBoneIndex());

        SpineAnimation animEvent = getAnim(anims, "anim_event");
        assertEquals(1, animEvent.getEventTracksCount());
        EventTrack eventTrack = animEvent.getEventTracks(0);
        assertEquals(MurmurHash.hash64("test_event"), eventTrack.getEventId());
        assertEquals(1, eventTrack.getKeys(0).getInteger());
        assertEquals(0.5f, eventTrack.getKeys(1).getFloat(), 0.00000f);
        assertEquals(MurmurHash.hash64("test_string"), eventTrack.getKeys(2).getString());

        SpineAnimation animStepped = getAnim(anims, "anim_stepped");
        AnimationTrack trackStepped = animStepped.getTracks(0);
        int keyCount = trackStepped.getPositionsCount() / 3;
        assertEquals(0.0, trackStepped.getPositions(0 * 3), EPSILON);
        assertEquals(0.0, trackStepped.getPositions(1 * 3), EPSILON);
        assertEquals(100.0, trackStepped.getPositions((keyCount - 1) * 3), EPSILON);
    }

    @Test
    public void testSpineScene() throws Exception {
        addImage("/test.png", 16, 16);
        StringBuilder src = new StringBuilder();
        src.append("images { image:  \"/test.png\" }");
        build("/skeleton_atlas.atlas", src.toString());
        
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
