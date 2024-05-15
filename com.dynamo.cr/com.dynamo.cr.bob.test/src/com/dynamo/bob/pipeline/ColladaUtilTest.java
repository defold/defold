// Copyright 2020-2024 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.pipeline;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.InputStream;
import java.nio.ByteOrder;
import java.nio.ShortBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;

import javax.vecmath.Point4i;
import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;
import javax.vecmath.Tuple4d;
import javax.vecmath.Tuple4f;
import javax.vecmath.Tuple4i;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4f;

import org.junit.Test;

import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.MurmurHash;

import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.rig.proto.Rig;
import com.dynamo.rig.proto.Rig.RigAnimation;

public class ColladaUtilTest {

    static double EPSILON = 0.0001;

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
        if ((expected.x - actual.x) > EPSILON ||
            (expected.y - actual.y) > EPSILON ||
            (expected.z - actual.z) > EPSILON )
        {
            System.err.printf("Expected: %f, %f, %f  but got %f, %f, %f\n",
                expected.x, expected.y, expected.z,
                actual.x, actual.y, actual.z);
        }
        assertEquals(expected.x, actual.x, EPSILON);
        assertEquals(expected.y, actual.y, EPSILON);
        assertEquals(expected.z, actual.z, EPSILON);
    }

    private void assertV(Tuple4d expected, Tuple4d actual) {
        if ((expected.x - actual.x) > EPSILON ||
            (expected.y - actual.y) > EPSILON ||
            (expected.z - actual.z) > EPSILON ||
            (expected.w - actual.w) > EPSILON )
        {
            System.err.printf("Expected: %f, %f, %f, %f  but got %f, %f, %f, %f\n",
                expected.x, expected.y, expected.z, expected.w,
                actual.x, actual.y, actual.z, actual.w);
        }

        assertEquals(expected.x, actual.x, EPSILON);
        assertEquals(expected.y, actual.y, EPSILON);
        assertEquals(expected.z, actual.z, EPSILON);
        assertEquals(expected.w, actual.w, EPSILON);
    }

    private void assertVertWeight(Tuple4f expected, List<Float> actual) {
        assertEquals(expected.x, actual.get(0), EPSILON);
        assertEquals(expected.y, actual.get(1), EPSILON);
        assertEquals(expected.z, actual.get(2), EPSILON);
        assertEquals(expected.w, actual.get(3), EPSILON);
    }

    private void assertVertBone(Tuple4i expected, List<Integer> actual) {
        assertEquals(expected.x, actual.get(0), EPSILON);
        assertEquals(expected.y, actual.get(1), EPSILON);
        assertEquals(expected.z, actual.get(2), EPSILON);
        assertEquals(expected.w, actual.get(3), EPSILON);
    }


    private InputStream load(String path) {
        return getClass().getResourceAsStream(path);
    }

    /*
     * Helper to test that a bone has the expected position and rotation.
     */
    private void assertBone(Rig.Bone bone, Vector3d expectedPosition, Quat4d expectedRotation) {
        assertV(expectedPosition, MathUtil.ddfToVecmath(bone.getLocal().getTranslation()));
        assertV(expectedRotation, MathUtil.ddfToVecmath(bone.getLocal().getRotation()));
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
     * Helper to test that a track has a certain position at a specific keyframe.
     */
    private void assertAnimationPosition(Rig.AnimationTrack track, int keyframe, Vector3d expectedPosition) {
        int i = keyframe * 3;
        Vector3d actualPosition = new Vector3d(track.getPositions(i), track.getPositions(i+1), track.getPositions(i+2));
        assertV(expectedPosition, actualPosition);
    }

    /*
     * Testing that it's the same key through out the track
     */
    private void assertAnimationSamePosScale(Rig.AnimationTrack track) {
        if (track.getPositionsCount() > 3) {
            int posCount = track.getPositionsCount();
            float x = track.getPositions(0);
            float y = track.getPositions(1);
            float z = track.getPositions(2);
            for (int i = 3; i < posCount; i += 3) {
                assertEquals(x, track.getPositions(i+0), EPSILON);
                assertEquals(y, track.getPositions(i+1), EPSILON);
                assertEquals(z, track.getPositions(i+2), EPSILON);
            }
        } else {
            assertEquals(0, track.getPositionsCount());
        }

        if (track.getScaleCount() > 3) {
            float x = track.getScale(0);
            float y = track.getScale(1);
            float z = track.getScale(2);
            int scaleCount = track.getScaleCount();
            for (int i = 3; i < scaleCount; i += 3) {
                assertEquals(x, track.getScale(i+0), EPSILON);
                assertEquals(y, track.getScale(i+1), EPSILON);
                assertEquals(z, track.getScale(i+2), EPSILON);
            }
        } else {
            assertEquals(0, track.getScaleCount());
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
        int keyframeCount = track.getRotationsCount() / 4 - 2;

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
        Rig.MeshSet.Builder meshSet = Rig.MeshSet.newBuilder();
        ColladaUtil.loadMesh(load("maya_quad.dae"), meshSet);
        Rig.Mesh mesh = meshSet.getModels(0).getMeshes(0);

        List<Float> pos = mesh.getPositionsList();
        List<Float> nrm = mesh.getNormalsList();
        List<Float> uvs = mesh.getTexcoord0List();
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

        assertUV(uvs, 0, 0, 0);
        assertUV(uvs, 1, 1, 0);
        assertUV(uvs, 2, 0, 1);
        assertUV(uvs, 3, 0, 1);
        assertUV(uvs, 4, 1, 0);
        assertUV(uvs, 5, 1, 1);
    }

    @Test
    public void testBlenderPolylistQuad() throws Exception {
        Rig.MeshSet.Builder meshSet = Rig.MeshSet.newBuilder();
        ColladaUtil.loadMesh(load("blender_polylist_quad.dae"), meshSet);
        Rig.Mesh mesh = meshSet.getModels(0).getMeshes(0);

        List<Float> pos = mesh.getPositionsList();
        List<Float> nrm = mesh.getNormalsList();
        List<Float> uvs = mesh.getTexcoord0List();

        assertThat(2 * 3 * 3, is(pos.size()));
        assertThat(2 * 3 * 3, is(nrm.size()));
        assertThat(2 * 3 * 2, is(uvs.size()));

        assertVtx(pos, 0, -1, 0,  1);
        assertVtx(pos, 1,  1, 0,  1);
        assertVtx(pos, 2,  1, 0, -1);
        assertVtx(pos, 3, -1, 0,  1);
        assertVtx(pos, 4,  1, 0, -1);
        assertVtx(pos, 5, -1, 0, -1);

        assertNrm(nrm, 0, 0, 1, 0);
        assertNrm(nrm, 1, 0, 1, 0);
        assertNrm(nrm, 2, 0, 1, 0);
        assertNrm(nrm, 3, 0, 1, 0);
        assertNrm(nrm, 4, 0, 1, 0);
        assertNrm(nrm, 5, 0, 1, 0);

        assertUV(uvs, 0, 0, 0);
        assertUV(uvs, 1, 0, 0);
        assertUV(uvs, 2, 0, 0);
        assertUV(uvs, 3, 0, 0);
        assertUV(uvs, 4, 0, 0);
        assertUV(uvs, 5, 0, 0);
    }

    /*
     * Verify that up-axis are applied on normals.
     */
    @Test
    public void testBlenderTriangleNormals() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(getClass().getResourceAsStream("quad_normals.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);
        Rig.Mesh mesh = meshSetBuilder.getModels(0).getMeshes(0);

        List<Float> pos = mesh.getPositionsList();
        List<Float> nrm = mesh.getNormalsList();

        // face 0:
        assertVtx(pos, 0, -1,  0, -1);
        assertVtx(pos, 1, -1,  0,  1);
        assertVtx(pos, 2,  1,  0,  1);
        assertNrm(nrm, 0,  0,  1,  0);
        assertNrm(nrm, 0,  0,  1,  0);
        assertNrm(nrm, 0,  0,  1,  0);

        // face 1:
        assertVtx(pos, 3,  1,  0, -1);
        assertVtx(pos, 4, -1,  0, -1);
        assertVtx(pos, 5,  1,  0,  1);
        assertNrm(nrm, 0,  0,  1,  0);
        assertNrm(nrm, 0,  0,  1,  0);
        assertNrm(nrm, 0,  0,  1,  0);
    }

    /*
     * Tests mesh node scaling vs mesh scaled vertices
     */
    @Test
    public void testScaledMeshNode() throws Exception {
        Rig.MeshSet.Builder meshSet = Rig.MeshSet.newBuilder();
        ColladaUtil.loadMesh(load("chest_model.dae"), meshSet);
        Rig.MeshSet.Builder meshSetNoSkin = Rig.MeshSet.newBuilder();
        ColladaUtil.loadMesh(load("chest_model_noskin.dae"), meshSetNoSkin);

        Rig.Mesh mesh = meshSet.getModels(0).getMeshes(0);
        Rig.Mesh meshNoSkin = meshSetNoSkin.getModels(0).getMeshes(0);

        List<Float> pos = mesh.getPositionsList();
        List<Float> posNoSkin = meshNoSkin.getPositionsList();
        for(int i = 0; i < pos.size(); ++i) {
            assertEquals(pos.get(i), posNoSkin.get(i));
        }
    }

    /*
     * Tests a collada file with fewer, and more, than 4 bone influences per vertex.
     */
    @Test
    public void testBoneInfluences() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("bone_influences.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);
        Rig.Mesh mesh = meshSetBuilder.getModels(0).getMeshes(0);

        // Should have exactly 4 influences per vertex
        int vertCount = mesh.getPositionsCount() / 3;
        assertEquals(vertCount*4, mesh.getBoneIndicesCount());
        assertEquals(vertCount*4, mesh.getWeightsCount());

        List<Integer> boneIndices = mesh.getBoneIndicesList();
        List<Float> boneWeights = mesh.getWeightsList();

        // Test the max bone count is correct, should be 4 for this mesh, which is the highest indexed bone + 1 in any of the meshes in the mesh set
        assertEquals(Collections.max(boneIndices).longValue(), meshSetBuilder.getMaxBoneCount()-1);

        /*
         * The DAE has the following bones and weights for each vertex:
         *
         * -------------------------
         * | Vert | (Bone, Weight) |
         * -------------------------
         * |  v0  |     0: 0.5     |
         * |      |     1: 0.1     |
         * |      |     2: 0.2     |
         * |      |     3: 0.3     |
         * |      |     4: 0.4     |
         * -------------------------
         * |  v1  |     0: 0.1     |
         * |      |     1: 0.2     |
         * |      |     2: 0.3     |
         * |      |     3: 0.4     |
         * |      |     4: 0.5     |
         * -------------------------
         * -------------------------
         * |  v2  |     0: 0.25    |
         * -------------------------
         *
         * Influences for v0 will be expanded into 3 more with zero weights.
         * Influences for v1 will be reordered, influence of bone 1 (lowest weight) will be skipped.
         * Influences for v2 will be reordered, influence of bone 0 (lowest weight) will be skipped.
         */

        assertVertBone(new Point4i(0, 4, 3, 2), boneIndices.subList(0, 4));
        assertVertWeight(new Vector4f(0.5f, 0.4f, 0.3f, 0.2f), boneWeights.subList(0, 4));

        assertVertBone(new Point4i(4, 3, 2, 1), boneIndices.subList(4, 8));
        assertVertWeight(new Vector4f(0.5f, 0.4f, 0.3f, 0.2f), boneWeights.subList(4, 8));

        assertVertBone(new Point4i(0, 0, 0, 0), boneIndices.subList(8, 12));
        assertVertWeight(new Vector4f(0.25f, 0.0f, 0.0f, 0.0f), boneWeights.subList(8, 12));
    }

    @Test
    public void testObjectAnimations() throws Exception {
        Rig.Skeleton.Builder skeleton = Rig.Skeleton.newBuilder();
        ColladaUtil.loadSkeleton(load("blender_animated_cube.dae"), skeleton, new ArrayList<String>());
        Rig.AnimationSet.Builder animation = Rig.AnimationSet.newBuilder();
        ColladaUtil.loadAnimations(load("blender_animated_cube.dae"), animation, "", new ArrayList<String>());

        // We only support bone animations currently, this collada file include
        // animations directly on the object. The resulting output will be zero animations.
        assertEquals(0, animation.getAnimationsCount());
    }

    @Test
    public void testSkeleton() throws Exception {

        String[] boneIds   = {"root", "Bone_001", "Bone_002", "Bone_003", "Bone_004"};
        String[] parentIds =  {null, "root", "root", "root", "root"};

        Rig.Skeleton.Builder skeleton = Rig.Skeleton.newBuilder();
        ColladaUtil.loadSkeleton(load("bone_influences.dae"), skeleton, new ArrayList<String>());
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
            assertEquals(idToIndex.getOrDefault(parentIds[index], -1).intValue(), bone.getParent());
        }
    }

    // We don't support collada files with multiple "root joints" in their skeleton.
    // Make sure the we throw an error if we find more than one root joint/bone.
    @Test(expected = LoaderException.class)
    public void testMultipleRoots() throws Exception {
        Rig.Skeleton.Builder skeleton = Rig.Skeleton.newBuilder();
        ColladaUtil.loadSkeleton(load("two_roots.dae"), skeleton, new ArrayList<String>());
    }

    @Test
    public void testBoneNoAnimation() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("one_vertice_bone_noanim.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);

        assertEquals(0, animSetBuilder.getAnimationsCount());

        // The animation is exported from Blender and has Z as up-axis.
        // The bone is originally in blender located in origo, rotated -90 degrees around it's local Z-axis.
        // After the up-axis has been taken into account the final rotation of the bone is;
        // x: -90, y: 0, z:-90
        assertEquals(1, skeletonBuilder.getBonesCount());
        assertBone(skeletonBuilder.getBones(0), new Vector3d(0.0, 0.0, 0.0), new Quat4d(0.5, 0.5, 0.5, -0.5));
    }

    /*
     *  Tests a collada with only one bone with animation (matrix input track).
     */
    @Test
    public void testOneBoneAnimation() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("one_vertice_bone.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        // Same bone setup as testBoneNoAnimation().
        assertEquals(1, skeletonBuilder.getBonesCount());
        assertBone(skeletonBuilder.getBones(0), new Vector3d(0.0, 0.0, 0.0), new Quat4d(0.5, 0.5, 0.5, -0.5));

        assertEquals(1, animSetBuilder.getAnimations(0).getTracksCount());

        /*
         *  We go through all tracks and verify they behave as expected.
         *  - Positions and scale don't change
         *  - Rotation is only increased on X-axis
         */
        long boneId = skeletonBuilder.getBones(0).getId();
        int trackCount = animSetBuilder.getAnimations(0).getTracksCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(trackIndex);
            assertEquals(boneId, track.getBoneId());

            /*
             *  The collada file does not animate either position or scale for the bones,
             *  but since the input animations are matrices we don't "know" that. But we
             *  will verify that they do not change.
             */
            assertAnimationSamePosScale(track);

            if (track.getRotationsCount() > 0) {
                // Assert that the rotation keyframes keeps increasing rotation around X
                assertAnimationRotationChanges(track, -1.0f, 1.0f, -1.0f);
            }
        }
    }

    /*
     *  Tests a collada with two connected bones, each with their own animation track.
     */
    @Test
    public void testTwoBoneAnimation() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("two_bone.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        /*
         *  The file includes two bones with a matrix animation on each.
         */
        assertEquals(2, skeletonBuilder.getBonesCount());
        assertEquals(2, animSetBuilder.getAnimations(0).getTracksCount());

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

        long boneId0 = skeletonBuilder.getBones(0).getId();
        long boneId1 = skeletonBuilder.getBones(1).getId();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(trackIndex);
            long boneId = track.getBoneId();

            // There should be no position or scale animation.
            assertAnimationSamePosScale(track);

            if (track.getRotationsCount() > 0) {
                if (boneId == boneId0) {
                    // Verify animations on root bone
                    assertAnimationRotation(track, 0, rotIdentity);
                    assertAnimationRotation(track, keyframeCount/2, rotIdentity);
                    assertAnimationRotation(track, keyframeCount, rot90ZAxis);

                } else if (boneId == boneId1) {
                    // Verify animation on secondary bone
                    assertAnimationRotation(track, 0, rotIdentity);
                    assertAnimationRotation(track, keyframeCount/2, rot90ZAxis);
                    assertAnimationRotation(track, keyframeCount, rotIdentity);

                } else {
                    fail("Animations on invalid bone index: " + boneId);
                }
            }
        }
    }

    /*
     *  Tests a collada with two connected bones without a mesh.
     */
    @Test
    public void testTwoBoneNoMesh() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("no_mesh.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        /*
         *  The file includes two bones with a matrix animation on each.
         */
        assertEquals(2, skeletonBuilder.getBonesCount());
        assertEquals(2, animSetBuilder.getAnimations(0).getTracksCount());

        // Bone 0 is located at origo an has no rotation (after up-axis has been applied).
        assertBone(skeletonBuilder.getBones(0), new Vector3d(0.0, 0.0, 0.0), new Quat4d(0.0, 0.0, 0.0, 1.0));

        // Bone 1 is located at (0, 2, 0), without any rotation.
        assertBone(skeletonBuilder.getBones(1), new Vector3d(0.0, 1.0, 1.0), new Quat4d(0.0, 0.0, 0.0, 1.0));

        /*
         * Test bone animations animations. The file has the following animations:
         *   - No scale animations.
         *   - First half of the animation consist of bone 1 animating position (0,1,1) to (0,1,0), the animation delta is thus (0,0,-1).
         *   - Second half of the animation consist of bone 2 animating rotation to -90 on X-axis.
         */
        Quat4d rotIdentity = new Quat4d(0.0, 0.0, 0.0, 1.0);
        Quat4d rot90ZAxis = new Quat4d(-0.707, 0.0, 0.0, 0.707);

        int trackCount = animSetBuilder.getAnimations(0).getTracksCount();
        float duration = animSetBuilder.getAnimations(0).getDuration();
        float sampleRate = animSetBuilder.getAnimations(0).getSampleRate();
        int keyframeCount = (int)Math.ceil(duration*sampleRate);

        long boneId0 = skeletonBuilder.getBones(0).getId();
        long boneId1 = skeletonBuilder.getBones(1).getId();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(trackIndex);
            long boneId = track.getBoneId();

            if (boneId == boneId0 && track.getRotationsCount() > 0) {
                // Verify animations on root bone
                assertAnimationRotation(track, 0, rotIdentity);
                assertAnimationRotation(track, keyframeCount/2, rotIdentity);
                assertAnimationRotation(track, keyframeCount, rot90ZAxis);

            } else if (boneId == boneId1 && track.getPositionsCount() > 0) {
                // Verify animation on secondary bone
                assertAnimationPosition(track, 0, new Vector3d(0.0, 1.0, 1.0));
                assertAnimationPosition(track, keyframeCount/2, new Vector3d(0.0, 1.0, 0.0));
                assertAnimationPosition(track, keyframeCount, new Vector3d(0.0, 1.0, 0.0));

            }
        }
    }

    /*
     *  Tests a collada with two animated bones + one non-animated bone.
     */
    @Test
    public void testAnimAndNonAnimBones() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("non_animated_bones.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        /*
         *  The file includes three bones, but only two of them have animations
         */
        assertEquals(3, skeletonBuilder.getBonesCount());
        assertEquals(2, animSetBuilder.getAnimations(0).getTracksCount());

        // Bone 0 is located at origo an has no rotation (after up-axis has been applied).
        assertBone(skeletonBuilder.getBones(0), new Vector3d(0.0, 0.0, 0.0), new Quat4d(0.0, 0.0, 0.0, 1.0));

        // Bone 1 is located at (0, 1, 0), without any rotation.
        assertBone(skeletonBuilder.getBones(1), new Vector3d(0.0, 1.0, 0.0), new Quat4d(0.0, 0.0, 0.0, 1.0));
    }

    /*
     *  Tests a collada with only one bone with animation that doesn't have a keyframe at t=0.
     */
    @Test
    public void testDefaultKeyframes() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("one_bone_no_initial_keyframe.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        // Same bone setup as testBoneNoAnimation().
        assertEquals(1, skeletonBuilder.getBonesCount());
        assertBone(skeletonBuilder.getBones(0), new Vector3d(0.0, 0.0, 0.0), new Quat4d(0.5, 0.5, 0.5, -0.5));

        assertEquals(1, animSetBuilder.getAnimations(0).getTracksCount());

        /*
         *  We go through all tracks and verify they behave as expected.
         *  - Positions and scale don't change
         *  - Rotation is only decreased on X-axis
         */
        long boneId = skeletonBuilder.getBones(0).getId();
        int trackCount = animSetBuilder.getAnimations(0).getTracksCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(trackIndex);
            assertEquals(boneId, track.getBoneId());

            /*
             *  The collada file does not animate either position or scale for the bones,
             *  but since the input animations are matrices we don't "know" that. But we
             *  will verify that they do not change.
             */
            assertAnimationSamePosScale(track);

            if (track.getRotationsCount() > 0) {

                // Assert that the rotation keyframes keeps decreasing rotation around Y
                Quat4d rQ = new Quat4d(track.getRotations(8), track.getRotations(9), track.getRotations(10), track.getRotations(11));
                double lastRot = rQ.getY();

                int rotCount = track.getRotationsCount() / 4;
                for (int i = 2; i < rotCount; i++) {
                    rQ = new Quat4d(track.getRotations(i*4), track.getRotations(i*4+1), track.getRotations(i*4+2), track.getRotations(i*4+3));
                    if (rQ.getY() < lastRot) {
                        fail("Rotation is not decreasing. Previously: " + lastRot + ", now: " + rQ.getY());
                    }

                    lastRot = rQ.getY();
                }
            }
        }
    }

    /*
     *
     */
    @Test
    public void testMultipleBones() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("bone_box5.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        assertEquals(3, skeletonBuilder.getBonesCount());

        assertEquals(3, animSetBuilder.getAnimations(0).getTracksCount());

        /*
         *  We go through all tracks and verify they behave as expected.
         */

        long boneId0 = skeletonBuilder.getBones(0).getId();
        long boneId1 = skeletonBuilder.getBones(1).getId();
        long boneId2 = skeletonBuilder.getBones(2).getId();
        int trackCount = animSetBuilder.getAnimations(0).getTracksCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(trackIndex);
            long boneId = track.getBoneId();

            /*
             *  The collada file does not animate either position or scale for the bones,
             *  but since the input animations are matrices we don't "know" that. But we
             *  will verify that they do not change.
             */
            assertAnimationSamePosScale(track);

            if (boneId == boneId0) {
                // Bone 0 doesn't have any "real" rotation animation.
                Quat4d rotKey = new Quat4d(-0.495852, 0.499983, 0.499983, 0.504148);
                int rotationKeys = track.getRotationsCount() / 4;
                for (int i = 0; i < rotationKeys; i++) {
                    assertAnimationRotation(track, i, rotKey);
                }

            } else if (boneId == boneId1) {
                if (track.getRotationsCount() > 0) {
                    assertAnimationRotationChanges(track, 0.0f, 0.0f, 1.0f);
                }
            } else if (boneId == boneId2) {
                if (track.getRotationsCount() > 0) {
                    assertAnimationRotationChanges(track, 1.0f, 0.0f, 0.0f);
                }

            } else {
                // There should only be animation on the bones specified above.
                fail("Animation on invalid bone index: " + boneId);
            }
        }
    }

    /*
     *
     */
    @Test
    public void testBlenderRotation() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("two_bone_two_triangles.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        assertEquals(2, skeletonBuilder.getBonesCount());
        assertEquals(1, animSetBuilder.getAnimations(0).getTracksCount());

        /*
         *  We go through all tracks and verify they behave as expected.
         */

        long boneId1 = skeletonBuilder.getBones(1).getId();
        int trackCount = animSetBuilder.getAnimations(0).getTracksCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(trackIndex);
            long boneId = track.getBoneId();

            /*
             *  The collada file does not animate either position or scale for the bones,
             *  but since the input animations are matrices we don't "know" that. But we
             *  will verify that they do not change.
             */
            assertAnimationSamePosScale(track);

            if (boneId == boneId1) {
                if (track.getRotationsCount() > 0) {
                    assertAnimationRotationChanges(track, 0.0f, 0.0f, 1.0f);
                }
            } else {
                // There should only be animation on the bones specified above.
                fail("Animation on invalid bone index: " + boneId);
            }
        }
    }

    /*
     *  Test collada file with a bone animation that includes both translation and rotation.
     */
    @Test
    public void testTranslationRotation() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("translate_rot_test.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);

        RigAnimation animation = animSetBuilder.getAnimations(0);

        /*
         *  We go through all tracks and verify they behave as expected.
         *  - Positions will decrease on Z axis
         *  - Rotation don't change, is static the inverse of bind pose.
         */
        long boneId1 = skeletonBuilder.getBones(1).getId();
        int trackCount = animation.getTracksCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animation.getTracks(trackIndex);

            if (track.getBoneId() == boneId1) {

                if (track.getRotationsCount() > 0) {
                    Quat4d rotIdentity = new Quat4d(0.0, 0.0, 0.0, 1.0);

                    int rotCount = track.getRotationsCount() / 4;
                    for (int i = 0; i < rotCount; i++) {
                        assertEquals(rotIdentity.getX(), track.getRotations(i*4), EPSILON);
                        assertEquals(rotIdentity.getY(), track.getRotations(i*4+1), EPSILON);
                        assertEquals(rotIdentity.getZ(), track.getRotations(i*4+2), EPSILON);
                        assertEquals(rotIdentity.getW(), track.getRotations(i*4+3), EPSILON);
                    }
                } else if (track.getPositionsCount() > 0) {

                    // Position changes only in negative Z
                    float lastZ = track.getPositions(2);
                    int posCount = track.getPositionsCount() / 3 - 2;
                    for (int i = 1; i < posCount; i++) {
                        assertEquals(0.0f, track.getPositions(i*3), EPSILON);
                        assertEquals(0.0f, track.getPositions(i*3+1), EPSILON);
                        float currentZ = track.getPositions(i*3+2);
                        if (lastZ <= currentZ) {
                            fail("Position not decreasing on Z, last value was " + lastZ + ", current is " + currentZ + ".");
                        }

                        lastZ = currentZ;
                    }

                }
            } else {
                fail("Invalid bone index!");
            }
        }
    }

    /*
     *  Test collada file with rotation going from 0 to 180 degrees, that previously made the rotation quaternion being flipped on last keyframe.
     */
    @Test
    public void testDecomposeRotationFlip() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("rotating_box.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);

        RigAnimation animation = animSetBuilder.getAnimations(0);

        /*
         *  We go through rotating tracks and verify they always rotate in correct direction.
         */
        int trackCount = animation.getTracksCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animation.getTracks(trackIndex);
            if (track.getRotationsCount() > 0) {
                int rotationsCount = track.getRotationsCount();
                List<Float> rotations = track.getRotationsList();

                // Check that the Y component never goes below zero.
                for (int i = 0; i < rotationsCount; i+=4) {
                    assertTrue(rotations.get(i+1) >= 0.0f);
                }
            }
        }
    }

    /*
     * Test collada file with scale applied on its skeleton.
     */
    @Test
    public void testSkeletonScale() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("skeleton_scale.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);

        RigAnimation animation = animSetBuilder.getAnimations(0);

        Vector3 boneScale = skeletonBuilder.getBones(0).getLocal().getScale();
        assertEquals(0.1, boneScale.getX(), EPSILON);
        assertEquals(0.1, boneScale.getY(), EPSILON);
        assertEquals(0.1, boneScale.getZ(), EPSILON);

        int trackCount = animation.getTracksCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animation.getTracks(trackIndex);
            if (track.getScaleCount() > 0) {
                int scalesCount = track.getScaleCount();
                List<Float> scales = track.getScaleList();

                for (int i = 0; i < scalesCount; ++i) {
                    assertEquals(1.0, scales.get(i), EPSILON);
                }
            }
        }
    }

    /*
     * Test collada file with scene start/end time
     */
    @Test
    public void testSceneTime() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("scene_time_test.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);

        RigAnimation animation = animSetBuilder.getAnimations(0);

        int trackCount = animation.getTracksCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animation.getTracks(trackIndex);
            int positionCount = track.getPositionsCount();

            // Make sure all Z positions are between 1 and 2.
            // The full animation of Z stored in the scene goes from 0 to 3, but the start and end time should
            // restrict the values to be between 1 and 2.
            if (positionCount > 0) {
                List<Float> positions = track.getPositionsList();
                for (int i = 0; i < positionCount; i+=3) {
                    float actualZ = positions.get(i+2);
                    assertTrue(actualZ >= 1.0 && actualZ <= 2.0);
                }
            }
        }
    }

    /*
     * Collada file with a asset unit scale set to 0.01.
     */
    @Test
    public void testAssetUnit() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("asset_unit.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);

        double expectedUnit = 0.01;

        // Bone scale should be unaffected
        Vector3 boneScale = skeletonBuilder.getBones(0).getLocal().getScale();
        assertEquals(1.0, boneScale.getX(), EPSILON);
        assertEquals(1.0, boneScale.getY(), EPSILON);
        assertEquals(1.0, boneScale.getZ(), EPSILON);

        // Bone positions should be orig_position * unit
        Vector3 bonePosition = skeletonBuilder.getBones(0).getLocal().getTranslation();
        assertEquals(0.0, bonePosition.getX(), EPSILON);
        assertEquals(1.0 * expectedUnit, bonePosition.getY(), EPSILON);
        assertEquals(0.0, bonePosition.getZ(), EPSILON);

        // Mesh vertex position should also be scaled with unit
        Rig.Mesh mesh = meshSetBuilder.getModels(0).getMeshes(0);

        float vertPosX = mesh.getPositions(0);
        float vertPosY = mesh.getPositions(1);
        float vertPosZ = mesh.getPositions(2);
        assertEquals(0.0, vertPosX, EPSILON);
        assertEquals(1.0 * expectedUnit, vertPosY, EPSILON);
        assertEquals(0.0, vertPosZ, EPSILON);
    }

    /*
     * Collada file with invalid <float_array> data
     */
    @Test
    public void testInvalidFloatArrayData() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("chest_open.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);
        Rig.Mesh mesh = meshSetBuilder.getModels(0).getMeshes(0);

        // Check that we get all the positions, since some have the "-1.#IND00" value in the Collada file.
        // Invalid <float_array> values will be replaced with 0.0.
        // (Without the fix in XMLFloatArray.java this would have thrown an exception.)

        int vertexCount = mesh.getPositionsCount() / 3;
        assertEquals(786, vertexCount);

        //MAWE/ There's no good way of knowing which index it ended up on
        //and as the comment above says, it would have thrown an exception. So I disabled this test.

        // The test file has the first Z component of the positions array set to "-1.#IND00",
        // make sure it was parsed as 0.0 instead.
        //assertEquals(0.0, mesh.getPositions(2), EPSILON);
    }

    /*
     * Tests a collada file is optimized properly when creating an interleaved vertex index list
     */
    @Test
    public void testInterleavedVertexList() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(load("maya_quad.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);
        Rig.Mesh mesh = meshSetBuilder.getModels(0).getMeshes(0);

        assertEquals(4, mesh.getPositionsCount()/3);
        assertEquals(6, mesh.getIndices().size()>>1);
        assertEquals(Rig.IndexBufferFormat.INDEXBUFFER_FORMAT_16, mesh.getIndicesFormat());

        ShortBuffer indices = mesh.getIndices().asReadOnlyByteBuffer().order(ByteOrder.LITTLE_ENDIAN).asShortBuffer();
        assertEquals(0, indices.get(0));
        assertEquals(1, indices.get(1));
        assertEquals(2, indices.get(2));
        assertEquals(2, indices.get(3));
        assertEquals(1, indices.get(4));
        assertEquals(3, indices.get(5));
    }

    /*
     * Tests that an invalid collada file is handled
     */
    @Test
    public void testInvalidFile() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        boolean hasThrown = false;
        try {
            ColladaUtil.load(load("invalid.dae"), meshSetBuilder, animSetBuilder, skeletonBuilder);
        } catch (java.lang.IndexOutOfBoundsException e) {
            hasThrown = true;
        }
        assertTrue(hasThrown);
    }

    /*
     * TODO
     * Future tests:
     * - Position and scale animation on bones.
     * - Tests for collada files with non-matrix animations.
     * - Anim clips.
     */

}
