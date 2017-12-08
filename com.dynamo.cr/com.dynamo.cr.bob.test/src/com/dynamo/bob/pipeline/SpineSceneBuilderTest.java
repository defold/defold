package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.atlas.proto.AtlasProto.Atlas;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.spine.proto.Spine;
import com.dynamo.rig.proto.Rig;
import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.AnimationTrack;
import com.dynamo.rig.proto.Rig.Bone;
import com.dynamo.rig.proto.Rig.EventTrack;
import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.rig.proto.Rig.MeshEntry;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Skeleton;
import com.dynamo.rig.proto.Rig.RigAnimation;
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

    private void assertAnim(RigAnimation anim, boolean pos, boolean rot, boolean scale) {
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

    private static RigAnimation getAnim(Map<Long, RigAnimation> animations, String id) {
        return animations.get(MurmurHash.hash64(id));
    }

    private void assertAnimSet(AnimationSet animSet) {
        assertEquals(8, animSet.getAnimationsCount());
        Map<Long, RigAnimation> anims = new HashMap<Long, RigAnimation>();
        for (RigAnimation anim : animSet.getAnimationsList()) {
            anims.put(anim.getId(), anim);
        }
        assertAnim(getAnim(anims, "anim_curve"), true, false, false);
        assertAnim(getAnim(anims, "anim_multi"), true, true, true);
        assertAnim(getAnim(anims, "anim_pos"), true, false, false);
        assertAnim(getAnim(anims, "anim_rot"), false, true, false);
        assertAnim(getAnim(anims, "anim_scale"), false, false, true);
        assertAnim(getAnim(anims, "anim_stepped"), true, false, false);

        RigAnimation animTrackOrder = getAnim(anims, "anim_track_order");
        assertEquals(0, animTrackOrder.getTracks(0).getBoneIndex());
        assertEquals(1, animTrackOrder.getTracks(1).getBoneIndex());

        RigAnimation animEvent = getAnim(anims, "anim_event");
        assertEquals(1, animEvent.getEventTracksCount());
        EventTrack eventTrack = animEvent.getEventTracks(0);
        assertEquals(MurmurHash.hash64("test_event"), eventTrack.getEventId());
        assertEquals(1, eventTrack.getKeys(0).getInteger());
        assertEquals(0.5f, eventTrack.getKeys(1).getFloat(), 0.00000f);
        assertEquals(MurmurHash.hash64("test_string"), eventTrack.getKeys(2).getString());

        RigAnimation animStepped = getAnim(anims, "anim_stepped");
        AnimationTrack trackStepped = animStepped.getTracks(0);
        int keyCount = trackStepped.getPositionsCount() / 3;
        assertEquals(0.0, trackStepped.getPositions(0 * 3), EPSILON);
        assertEquals(0.0, trackStepped.getPositions(1 * 3), EPSILON);
        assertEquals(100.0, trackStepped.getPositions((keyCount - 1) * 3), EPSILON);
    }

    private void assertIK(Skeleton skeleton) {
        assertEquals(1, skeleton.getIksCount());
        Rig.IK ik = skeleton.getIks(0);
        assertEquals(1, ik.getParent());
        assertEquals(2, ik.getChild());
        assertEquals(3, ik.getTarget());
        assertEquals(false, ik.getPositive());
        assertEquals(0.5, ik.getMix(), EPSILON);
    }

    @Test
    public void testSpineScene() throws Exception {
        addImage("/test.png", 16, 16);
        StringBuilder src = new StringBuilder();
        src.append("images { image:  \"/test.png\" }");
        build("/skeleton_atlas.atlas", src.toString());

        src = new StringBuilder();
        src.append("spine_json: \"/skeleton.json\"");
        src.append("atlas: \"/skeleton_atlas.atlas\"");
        List<Message> outputs = build("/test.spinescene", src.toString());
        Rig.RigScene scene = (Rig.RigScene)outputs.get(0);
        Rig.Skeleton skeleton = (Rig.Skeleton)outputs.get(1);
        Rig.MeshSet meshset = (Rig.MeshSet)outputs.get(2);
        Rig.AnimationSet animationset = (Rig.AnimationSet)outputs.get(3);

        assertSkeleton(skeleton);
        assertMeshSet(meshset);
        assertAnimSet(animationset);
        assertIK(skeleton);
    }

    private void assertLT(float expected, float actual)
    {
        if (expected <= actual) {
            fail(actual + " is not less than " + expected);
        }
    }

    private void assertGT(float expected, float actual)
    {
        if (expected >= actual) {
            fail(actual + " is not greater than " + expected);
        }
    }

    @Test
    public void testBezierCurve() throws Exception {
        addImage("/test.png", 16, 16);
        StringBuilder src = new StringBuilder();
        src.append("images { image:  \"/test.png\" }");
        build("/skeleton_atlas.atlas", src.toString());

        src = new StringBuilder();
        src.append("spine_json: \"/curve_skeleton.json\"");
        src.append("atlas: \"/skeleton_atlas.atlas\"");
        List<Message> outputs = build("/test.spinescene", src.toString());
        Rig.AnimationSet animationset = (Rig.AnimationSet)outputs.get(3);

        RigAnimation animation = animationset.getAnimations(0);
        AnimationTrack track = animation.getTracks(0);

        // Verify the keys behave like a bezier curve and does not follow a linear curve
        int positionsCount = track.getPositionsCount();
        float t = 0.0f;
        final float endPosition = 100.0f;
        for (int i = 1; i < positionsCount / 3; i++) {
            t = (float)i / (positionsCount / 3);
            float x = track.getPositions(i*3);
            float xLinear = t * endPosition;

            if (t < 0.3333f) {
                assertLT(xLinear, x);
            } else if (t > 0.6666f) {
                assertGT(xLinear, x);
            }
        }
    }
}
