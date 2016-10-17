package com.dynamo.bob.pipeline;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.fail;

import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;
import javax.vecmath.Tuple4d;
import javax.vecmath.Vector3d;

import org.eclipse.swt.widgets.Display;
import org.jagatoo.loaders.models.collada.datastructs.animation.Bone;
import org.junit.Test;

import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.MurmurHash;

import com.dynamo.rig.proto.Rig;

public class ColladaUtilTest {

    static double EPSILON = 0.0001;

    public ColladaUtilTest(){
        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }
    }

    private List<Float> bake(List<Integer> indices, List<Float> values, int elemCount) {
        List<Float> result = new ArrayList<Float>(indices.size() * elemCount);
        for (Integer idx : indices) {
            for (int offset = 0; offset < elemCount; ++offset) {
                result.add(values.get(idx * elemCount + offset));
            }
        }
        return result;
    }

    private void assertVtx(List<Float> pos, int i, double xe, double ye, double ze) {
        float x = pos.get(i * 3 + 0);
        float y = pos.get(i * 3 + 1);
        float z = pos.get(i * 3 + 2);

        assertEquals(xe, x, EPSILON);
        assertEquals(ye, y, EPSILON);
        assertEquals(ze, z, EPSILON);
    }

    private void assertNrm(List<Float> nrm, int i, double xe, double ye, float ze) {
        float x = nrm.get(i * 3 + 0);
        float y = nrm.get(i * 3 + 1);
        float z = nrm.get(i * 3 + 2);

        assertEquals(xe, x, EPSILON);
        assertEquals(ye, y, EPSILON);
        assertEquals(ze, z, EPSILON);
    }

    private void assertUV(List<Float> nrm, int i, double ue, double ve) {
        float u = nrm.get(i * 2 + 0);
        float v = nrm.get(i * 2 + 1);

        assertEquals(ue, u, EPSILON);
        assertEquals(ve, v, EPSILON);
    }

    private void assertV(Tuple3d expected, Tuple3d actual) {
        assertEquals(expected.x, actual.x, EPSILON);
        assertEquals(expected.y, actual.y, EPSILON);
        assertEquals(expected.z, actual.z, EPSILON);
    }

    private void assertV(Tuple4d expected, Tuple4d actual) {
        assertEquals(expected.x, actual.x, EPSILON);
        assertEquals(expected.y, actual.y, EPSILON);
        assertEquals(expected.z, actual.z, EPSILON);
        assertEquals(expected.w, actual.w, EPSILON);
    }

    private InputStream load(String path) {
        return getClass().getResourceAsStream(path);
    }

    /*
     * Helper to test that a bone has the expected position and rotation.
     */
    private void assertBone(Rig.Bone bone, Vector3d expectedPosition, Quat4d expectedRotation) {
        assertV(expectedPosition, MathUtil.ddfToVecmath(bone.getPosition()));
        assertV(expectedRotation, MathUtil.ddfToVecmath(bone.getRotation()));
    }

    /*
     * Helper to test that a track has a certain rotation at a specific keyframe.
     */
    private void assertAnimationRotation(Rig.AnimationTrack track, int keyframe, Quat4d expectedRotation) {
        int i = keyframe * 4;
        Quat4d actualRotation = new Quat4d(track.getRotations(i), track.getRotations(i+1), track.getRotations(i+2), track.getRotations(i+3));
        assertV(expectedRotation, actualRotation);
    }

    /*
     * Helper to test that no animation is performed on either position or scale of a track.
     */
    private void assertAnimationNoPosScale(Rig.AnimationTrack track) {
        if (track.getPositionsCount() > 0) {
            int posCount = track.getPositionsCount();
            for (int i = 0; i < posCount; i++) {
                assertEquals(0.0, track.getPositions(i), EPSILON);
            }
        } else if (track.getScaleCount() > 0) {
            int scaleCount = track.getScaleCount();
            for (int i = 0; i < scaleCount; i++) {
                assertEquals(1.0, track.getScale(i), EPSILON);
            }
        }
    }

    /***
     * Helper to test if a rotation animation increases or decreases on a certain euler angle.
     * @param changesOnX Set to negative if the rotation is expected to decrease around X, or positive if expected to increase. 0.0 if no change is expected.
     * @param changesOnY Set to negative if the rotation is expected to decrease around Y, or positive if expected to increase. 0.0 if no change is expected.
     * @param changesOnZ Set to negative if the rotation is expected to decrease around Z, or positive if expected to increase. 0.0 if no change is expected.
     */
    private void assertAnimationRotationChanges(Rig.AnimationTrack track, float changesOnX, float changesOnY, float changesOnZ) {
        // 4 floats per keyframe due to quaternions, last keyframe is a duplicate so skip it.
        int keyframeCount = track.getRotationsCount() / 4 - 1;

        String[] axisLabel = {"X", "Y", "Z"};
        double[] changes = {changesOnX, changesOnY, changesOnZ};
        for (int axis = 0; axis < 3; ++axis) {
            double lastVal = track.getRotations(axis);
            for (int i = 1; i < keyframeCount; i++) {
                double val = track.getRotations(i*4+axis);
                double diff = val - lastVal;
                double diffNrm = diff;
                if ((Math.abs(diff) - EPSILON) > 0.0) {
                    diffNrm /= Math.abs(diff);
                }
                assertEquals(String.format("Rotation diff is incorrect, value %f around %s at key %d.", diff, axisLabel[axis], i), changes[axis], diffNrm, EPSILON);
                lastVal = val;
            }
        }
    }

    @Test
    public void testMayaQuad() throws Exception {
        Rig.Mesh.Builder mesh = Rig.Mesh.newBuilder();
        ColladaUtil.loadMesh(load("maya_quad.dae"), mesh, new HashMap<Integer, Integer>());
        List<Float> pos = bake(mesh.getIndicesList(), mesh.getPositionsList(), 3);
        List<Float> nrm = bake(mesh.getNormalsIndicesList(), mesh.getNormalsList(), 3);
        List<Float> uvs = bake(mesh.getTexcoord0IndicesList(), mesh.getTexcoord0List(), 2);
        assertThat(2 * 3 * 3, is(pos.size()));
        assertThat(2 * 3 * 3, is(nrm.size()));

        assertVtx(pos, 0, -0.005, -0.005, 0);
        assertVtx(pos, 1,  0.005, -0.005, 0);
        assertVtx(pos, 2, -0.005,  0.005, 0);
        assertVtx(pos, 3, -0.005,  0.005, 0);
        assertVtx(pos, 4,  0.005, -0.005, 0);
        assertVtx(pos, 5,  0.005,  0.005, 0);

        assertNrm(nrm, 0, 0, 0, 1);
        assertNrm(nrm, 1, 0, 0, 1);
        assertNrm(nrm, 2, 0, 0, 1);
        assertNrm(nrm, 3, 0, 0, 1);
        assertNrm(nrm, 4, 0, 0, 1);
        assertNrm(nrm, 5, 0, 0, 1);

        assertUV(uvs, 0, 0, 1);
        assertUV(uvs, 1, 1, 1);
        assertUV(uvs, 2, 0, 0);
        assertUV(uvs, 3, 0, 0);
        assertUV(uvs, 4, 1, 1);
        assertUV(uvs, 5, 1, 0);
    }

    @Test
    public void testBlenderPolylistQuad() throws Exception {
        Rig.Mesh.Builder mesh = Rig.Mesh.newBuilder();
        ColladaUtil.loadMesh(load("blender_polylist_quad.dae"), mesh, new HashMap<Integer, Integer>());

        List<Float> pos = bake(mesh.getIndicesList(), mesh.getPositionsList(), 3);
        List<Float> nrm = bake(mesh.getNormalsIndicesList(), mesh.getNormalsList(), 3);
        List<Float> uvs = bake(mesh.getTexcoord0IndicesList(), mesh.getTexcoord0List(), 2);

        assertThat(2 * 3 * 3, is(pos.size()));
        assertThat(2 * 3 * 3, is(nrm.size()));
        assertThat(2 * 3 * 2, is(uvs.size()));

        assertVtx(pos, 0, -1, 0,  1);
        assertVtx(pos, 1,  1, 0,  1);
        assertVtx(pos, 2,  1, 0, -1);
        assertVtx(pos, 3, -1, 0,  1);
        assertVtx(pos, 4,  1, 0, -1);
        assertVtx(pos, 5, -1, 0, -1);

        assertNrm(nrm, 0, 0, 0, 1);
        assertNrm(nrm, 1, 0, 0, 1);
        assertNrm(nrm, 2, 0, 0, 1);
        assertNrm(nrm, 3, 0, 0, 1);
        assertNrm(nrm, 4, 0, 0, 1);
        assertNrm(nrm, 5, 0, 0, 1);

        assertUV(uvs, 0, 0, 0);
        assertUV(uvs, 1, 0, 0);
        assertUV(uvs, 2, 0, 0);
        assertUV(uvs, 3, 0, 0);
        assertUV(uvs, 4, 0, 0);
        assertUV(uvs, 5, 0, 0);
    }

    @Test
    public void testBlenderAnimations() throws Exception {
        Rig.Skeleton.Builder skeleton = Rig.Skeleton.newBuilder();
        ArrayList<Bone> bones = ColladaUtil.loadSkeleton(load("blender_animated_cube.dae"), skeleton, new ArrayList<String>(), new HashMap<Integer, Integer>());
        Rig.AnimationSet.Builder animation = Rig.AnimationSet.newBuilder();
        ColladaUtil.loadAnimations(load("blender_animated_cube.dae"), animation, bones, 16.0f, new ArrayList<String>());
        assertEquals(1, animation.getAnimationsCount());
    }

    @Test
    public void testSkeleton() throws Exception {

        String[] boneIds   = {"root", "l_hip", "l_knee", "l_ankle", "l_null_toe", "pelvis", "spine",  "l_humerus", "l_ulna",    "l_wrist", "r_humerus", "r_ulna",    "r_wrist", "neck",  "null_head", "r_hip", "r_knee", "r_ankle", "r_null_toe"};
        String[] parentIds = {null,   "root",  "l_hip",  "l_knee",  "l_ankle",    "root",   "pelvis", "spine",     "l_humerus", "l_ulna",  "spine",     "r_humerus", "r_ulna",  "spine", "neck",      "root",  "r_hip",  "r_knee",  "r_ankle"};

        Rig.Skeleton.Builder skeleton = Rig.Skeleton.newBuilder();
        ColladaUtil.loadSkeleton(load("simple_anim.dae"), skeleton, new ArrayList<String>(), new HashMap<Integer, Integer>());
        List<Rig.Bone> bones = skeleton.getBonesList();
        assertEquals(boneIds.length, bones.size());
        HashMap<String,Integer> idToIndex = new HashMap<String,Integer>();
        for (int index = 0; index < boneIds.length; ++index) {
            idToIndex.put(boneIds[index], index);
        }
        for (int index = 0; index < boneIds.length; ++index) {
            Rig.Bone bone = bones.get(index);
            long id = MurmurHash.hash64(boneIds[index]);
            assertEquals(id, bone.getId());
            assertEquals(idToIndex.getOrDefault(parentIds[index], 65535).intValue(), bone.getParent());
        }
    }

    @Test
    public void testBoneNoAnimation() throws Exception {
        Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("one_vertice_bone_noanim.dae"), meshBuilder, animSetBuilder, skeletonBuilder);

        assertEquals(0, animSetBuilder.getAnimationsCount());

        // The animation is exported from Blender and has Z as up-axis.
        // The bone is originally in blender located in origo, rotated -90 degrees around it's local Z-axis.
        // After the up-axis has been taken into account the final rotation of the bone is;
        // x: -90, y: 90, z: 0
        assertEquals(1, skeletonBuilder.getBonesCount());
        assertBone(skeletonBuilder.getBones(0), new Vector3d(0.0, 0.0, 0.0), new Quat4d(-0.5, -0.5, -0.5, 0.5));
    }

    /*
     *  Tests a collada with only one bone with animation (matrix input track).
     */
    @Test
    public void testOneBoneAnimation() throws Exception {
        Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("one_vertice_bone.dae"), meshBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        // Same bone setup as testBoneNoAnimation().
        assertEquals(1, skeletonBuilder.getBonesCount());
        assertBone(skeletonBuilder.getBones(0), new Vector3d(0.0, 0.0, 0.0), new Quat4d(-0.5, -0.5, -0.5, 0.5));

        /*
         *  The bone is animated with a matrix track in the DAE,
         *  which expands into 3 separate tracks in our format;
         *  position, rotation and scale.
         */
        assertEquals(3, animSetBuilder.getAnimations(0).getTracksCount());

        /*
         *  We go through all tracks and verify they behave as expected.
         *  - Positions and scale don't change
         *  - Rotation is only increased on X-axis
         */
        int trackCount = animSetBuilder.getAnimations(0).getTracksCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(trackIndex);
            assertEquals(0, track.getBoneIndex());

            /*
             *  The collada file does not animate either position or scale for the bones,
             *  but since the input animations are matrices we don't "know" that. But we
             *  will verify that they do not change.
             */
            assertAnimationNoPosScale(track);

            if (track.getRotationsCount() > 0) {
                // Assert that the rotation keyframes keeps increasing rotation around X
                assertAnimationRotationChanges(track, 1.0f, 0.0f, 0.0f);
            }
        }
    }

    /*
     *  Tests a collada with two connected bones, each with their own animation track.
     */
    @Test
    public void testTwoBoneAnimation() throws Exception {
        Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("two_bone.dae"), meshBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        /*
         *  The file includes two bones with a matrix animation on each.
         *  Each matrix animation expands into 3 different component animations; 2*3 = 6 animation tracks.
         */
        assertEquals(2, skeletonBuilder.getBonesCount());
        assertEquals(6, animSetBuilder.getAnimations(0).getTracksCount());

        // Bone 0 is located at origo an has no rotation (after up-axis has been applied).
        assertBone(skeletonBuilder.getBones(0), new Vector3d(0.0, 0.0, 0.0), new Quat4d(0.0, 0.0, 0.0, 1.0));

        // Bone 1 is located at (0, 2, 0), without any rotation.
        assertBone(skeletonBuilder.getBones(1), new Vector3d(0.0, 2.0, 0.0), new Quat4d(0.0, 0.0, 0.0, 1.0));

        /*
         * Test bone animations animations. The file has the following animations:
         *   - No position or scale animations.
         *   - First half of the animation consist of bone 1 animating rotation to 90 on Z-axis.
         *   - Second half of the animation consist of bone 1 animating back rotation to 0 on Z-axis,
         *     and bone 0 animating rotation to 90 on Z-axis.
         */
        Quat4d rotIdentity = new Quat4d(0.0, 0.0, 0.0, 1.0);
        Quat4d rot90ZAxis = new Quat4d(0.0, 0.0, 0.707, 0.707);

        int trackCount = animSetBuilder.getAnimations(0).getTracksCount();
        float duration = animSetBuilder.getAnimations(0).getDuration();
        float sampleRate = animSetBuilder.getAnimations(0).getSampleRate();
        int keyframeCount = (int)Math.ceil(duration*sampleRate);

        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(trackIndex);
            int boneIndex = track.getBoneIndex();

            // There should be no position or scale animation.
            assertAnimationNoPosScale(track);

            if (track.getRotationsCount() > 0) {
                if (boneIndex == 0) {
                    // Verify animations on root bone
                    assertAnimationRotation(track, 0, rotIdentity);
                    assertAnimationRotation(track, keyframeCount/2, rotIdentity);
                    assertAnimationRotation(track, keyframeCount, rot90ZAxis);

                } else if (boneIndex == 1) {
                    // Verify animation on secondary bone
                    assertAnimationRotation(track, 0, rotIdentity);
                    assertAnimationRotation(track, keyframeCount/2, rot90ZAxis);
                    assertAnimationRotation(track, keyframeCount, rotIdentity);

                } else {
                    fail("Animations on invalid bone index: " + boneIndex);
                }
            }
        }
    }

    /*
     *  Tests a collada with only one bone with animation that doesn't have a keyframe at t=0.
     */
    @Test
    public void testDefaultKeyframes() throws Exception {
        Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("one_bone_no_initial_keyframe.dae"), meshBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        // Same bone setup as testBoneNoAnimation().
        assertEquals(1, skeletonBuilder.getBonesCount());
        assertBone(skeletonBuilder.getBones(0), new Vector3d(0.0, 0.0, 0.0), new Quat4d(-0.5, -0.5, -0.5, 0.5));

        /*
         *  The bone is animated with a matrix track in the DAE,
         *  which expands into 3 separate tracks in our format;
         *  position, rotation and scale.
         */
        assertEquals(3, animSetBuilder.getAnimations(0).getTracksCount());

        /*
         *  We go through all tracks and verify they behave as expected.
         *  - Positions and scale don't change
         *  - Rotation is only decreased on X-axis
         */
        int trackCount = animSetBuilder.getAnimations(0).getTracksCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(trackIndex);
            assertEquals(0, track.getBoneIndex());

            /*
             *  The collada file does not animate either position or scale for the bones,
             *  but since the input animations are matrices we don't "know" that. But we
             *  will verify that they do not change.
             */
            assertAnimationNoPosScale(track);

            if (track.getRotationsCount() > 0) {

                // Verify that the first keyframe is not rotated.
                assertAnimationRotation(track, 0, new Quat4d(0.0, 0.0, 0.0, 1.0));

                // Assert that the rotation keyframes keeps increasing rotation around X
                Quat4d rQ = new Quat4d(track.getRotations(8), track.getRotations(9), track.getRotations(10), track.getRotations(11));
                double lastXRot = rQ.getX();

                int rotCount = track.getRotationsCount() / 4;
                for (int i = 2; i < rotCount; i++) {
                    rQ = new Quat4d(track.getRotations(i*4), track.getRotations(i*4+1), track.getRotations(i*4+2), track.getRotations(i*4+3));
                    if (rQ.getX() > lastXRot) {
                        fail("Rotation is not decreasing. Previously: " + lastXRot + ", now: " + rQ.getX());
                    }

                    lastXRot = rQ.getX();
                }
            }
        }
    }

    /*
     *
     */
    @Test
    public void testMultipleBones() throws Exception {
        Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("bone_box5.dae"), meshBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        assertEquals(3, skeletonBuilder.getBonesCount());

        /*
         *  Each bone is animated with a matrix track in the DAE,
         *  which expands into 3 separate tracks in our format.
         */
        assertEquals(9, animSetBuilder.getAnimations(0).getTracksCount());

        /*
         *  We go through all tracks and verify they behave as expected.
         */
        int trackCount = animSetBuilder.getAnimations(0).getTracksCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(trackIndex);
            int boneIndex = track.getBoneIndex();

            /*
             *  The collada file does not animate either position or scale for the bones,
             *  but since the input animations are matrices we don't "know" that. But we
             *  will verify that they do not change.
             */
            assertAnimationNoPosScale(track);

            if (boneIndex == 0) {
                // Bone 0 doesn't have any "real" rotation animation.
                Quat4d rotIdentity = new Quat4d(0.0, 0.0, 0.0, 1.0);
                int rotationKeys = track.getRotationsCount() / 4;
                for (int i = 0; i < rotationKeys; i++) {
                    assertAnimationRotation(track, i, rotIdentity);
                }

            } else if (boneIndex == 1) {
                if (track.getRotationsCount() > 0) {
                    assertAnimationRotationChanges(track, 0.0f, 0.0f, 1.0f);
                }
            } else if (boneIndex == 2) {
                if (track.getRotationsCount() > 0) {
                    assertAnimationRotationChanges(track, 1.0f, 0.0f, 0.0f);
                }

            } else {
                // There should only be animation on the bones specified above.
                fail("Animation on invalid bone index: " + boneIndex);
            }
        }
    }

    /*
     *
     */
    @Test
    public void testBlenderRotation() throws Exception {
        Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("two_bone_two_triangles.dae"), meshBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        assertEquals(2, skeletonBuilder.getBonesCount());
        assertEquals(3, animSetBuilder.getAnimations(0).getTracksCount());

        /*
         *  We go through all tracks and verify they behave as expected.
         */
        int trackCount = animSetBuilder.getAnimations(0).getTracksCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(trackIndex);
            int boneIndex = track.getBoneIndex();

            /*
             *  The collada file does not animate either position or scale for the bones,
             *  but since the input animations are matrices we don't "know" that. But we
             *  will verify that they do not change.
             */
            assertAnimationNoPosScale(track);

            if (boneIndex == 1) {
                if (track.getRotationsCount() > 0) {
                    assertAnimationRotationChanges(track, 0.0f, 0.0f, 1.0f);
                }
            } else {
                // There should only be animation on the bones specified above.
                fail("Animation on invalid bone index: " + boneIndex);
            }
        }
    }

    /*
     * TODO
     * Future tests:
     * - Position and scale animation on bones.
     * - Tests for collada files with non-matrix animations.
     * - Anim clips.
     */

}
