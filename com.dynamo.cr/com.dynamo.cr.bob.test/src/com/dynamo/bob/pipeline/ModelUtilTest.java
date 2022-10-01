// Copyright 2020-2022 The Defold Foundation
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
import java.io.IOException;
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

import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.proto.DdfMath.Transform;
import com.dynamo.rig.proto.Rig;
import com.dynamo.rig.proto.Rig.RigAnimation;

public class ModelUtilTest {

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
        if (track.getPositionsCount() >= 3) {
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

        if (track.getScaleCount() >= 3) {
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

   ModelImporter.Scene loadScene(String path) {
        try {
            return ModelUtil.loadScene(getClass().getResourceAsStream(path), path, new ModelImporter.Options());
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
    }

    private ModelImporter.Scene loadBuiltScene(String path,
                                         Rig.MeshSet.Builder meshSetBuilder,
                                         Rig.AnimationSet.Builder animSetBuilder,
                                         Rig.Skeleton.Builder skeletonBuilder) {
        ModelImporter.Scene scene = loadScene(path);
        if (scene != null)
        {
            ModelUtil.loadModels(scene, meshSetBuilder);
            ModelUtil.loadSkeleton(scene, skeletonBuilder);

            ArrayList<String> animationIds = new ArrayList<>();
            ArrayList<ModelImporter.Bone> bones = ModelUtil.loadSkeleton(scene);
            ModelUtil.loadAnimations(scene, bones, animSetBuilder, "top_anim", animationIds);
        }
        return scene;
    }

    private ModelImporter.Scene loadBuiltScene(String path,
                                         Rig.MeshSet.Builder meshSetBuilder) {
        ModelImporter.Scene scene = loadScene(path);
        ModelUtil.loadModels(scene, meshSetBuilder);
        return scene;
    }

    private ModelImporter.Scene loadBuiltScene(String path,
                                         Rig.Skeleton.Builder skeletonBuilder) {
        ModelImporter.Scene scene = loadScene(path);
        ModelUtil.loadSkeleton(scene, skeletonBuilder);
        return scene;
    }

    // @Test
    // public void testMayaQuad() throws Exception {
    //     Rig.MeshSet.Builder meshSet = Rig.MeshSet.newBuilder();
    //     loadBuiltScene("maya_quad.dae", meshSet);
    //     Rig.Mesh mesh = meshSet.getModels(0).getMeshes(0);

    //     List<Float> pos = mesh.getPositionsList();
    //     List<Float> nrm = mesh.getNormalsList();
    //     List<Float> uvs = mesh.getTexcoord0List();
    //     assertThat(2 * 3 * 3, is(pos.size()));
    //     assertThat(2 * 3 * 3, is(nrm.size()));

    //     assertVtx(pos, 0, -0.005, -0.005, 0);
    //     assertVtx(pos, 1,  0.005, -0.005, 0);
    //     assertVtx(pos, 2, -0.005,  0.005, 0);
    //     assertVtx(pos, 3, -0.005,  0.005, 0);
    //     assertVtx(pos, 4,  0.005, -0.005, 0);
    //     assertVtx(pos, 5,  0.005,  0.005, 0);

    //     assertNrm(nrm, 0, 0, 0, 1);
    //     assertNrm(nrm, 1, 0, 0, 1);
    //     assertNrm(nrm, 2, 0, 0, 1);
    //     assertNrm(nrm, 3, 0, 0, 1);
    //     assertNrm(nrm, 4, 0, 0, 1);
    //     assertNrm(nrm, 5, 0, 0, 1);

    //     assertUV(uvs, 0, 0, 0);
    //     assertUV(uvs, 1, 1, 0);
    //     assertUV(uvs, 2, 0, 1);
    //     assertUV(uvs, 3, 0, 1);
    //     assertUV(uvs, 4, 1, 0);
    //     assertUV(uvs, 5, 1, 1);
    // }

    /*
     * Tests a collada file with fewer, and more, than 4 bone influences per vertex.
     */
    @Test
    public void testBoneInfluences() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ModelImporter.Scene scene = loadBuiltScene("bend2bones.gltf", meshSetBuilder, animSetBuilder, skeletonBuilder);

        Rig.Mesh mesh = meshSetBuilder.getModels(0).getMeshes(0);

        // Should have exactly 4 influences per vertex
        int vertCount = mesh.getPositionsCount() / 3;
        assertEquals(vertCount*4, mesh.getBoneIndicesCount());
        assertEquals(vertCount*4, mesh.getWeightsCount());

        List<Integer> boneIndices = mesh.getBoneIndicesList();
        List<Float> boneWeights = mesh.getWeightsList();

        assertEquals(3, skeletonBuilder.getBonesCount());
        assertBone(skeletonBuilder.getBones(0), new Vector3d(0.0, 0.0, 0.0), new Quat4d(0.0, 0.0, 0.0, 1.0));
        assertBone(skeletonBuilder.getBones(1), new Vector3d(0.0, 3.0, 0.0), new Quat4d(0.0, 0.0, 0.0, 1.0));
        assertBone(skeletonBuilder.getBones(2), new Vector3d(0.0, 2.0, 0.0), new Quat4d(0.0, 0.0, 0.0, 1.0));

        // Test the max bone count is correct, should be 4 for this mesh, which is the highest indexed bone + 1 in any of the meshes in the mesh set
        assertEquals(Collections.max(boneIndices).longValue(), meshSetBuilder.getMaxBoneCount()-1);

        int v = 0;
        assertVertBone(new Point4i(2, 1, 0, 0), boneIndices.subList(v*4, v*4+4));
        assertVertWeight(new Vector4f(0.980107f, 0.019893f, 0.0f, 0.0f), boneWeights.subList(v*4, v*4+4));

        v = 38;
        assertVertBone(new Point4i(1, 0, 2, 0), boneIndices.subList(v*4, v*4+4));
        assertVertWeight(new Vector4f(0.637669f, 0.355733f, 0.006598f, 0.0f), boneWeights.subList(v*4, v*4+4));

        v = 72;
        assertVertBone(new Point4i(1, 2, 0, 0), boneIndices.subList(v*4, v*4+4));
        assertVertWeight(new Vector4f(0.761502f, 0.185560f, 0.052938f, 0.0f), boneWeights.subList(v*4, v*4+4));
    }

    @Test
    public void testSkeleton() throws Exception {

        String[] boneIds   = {"Bottom", "Middle", "Top"};
        String[] parentIds = {null,     "Bottom", "Middle"};

        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ModelImporter.Scene scene = loadBuiltScene("bend2bones.gltf", skeletonBuilder);

        List<Rig.Bone> bones = skeletonBuilder.getBonesList();
        assertEquals(boneIds.length, bones.size());
        HashMap<String,Integer> idToIndex = new HashMap<String,Integer>();
        for (int index = 0; index < boneIds.length; ++index) {
            idToIndex.put(boneIds[index], index);
        }
        for (int index = 0; index < boneIds.length; ++index) {
            Rig.Bone bone = bones.get(index);
            long id = MurmurHash.hash64(boneIds[index]);
            assertEquals(boneIds[index], bone.getName());
            assertEquals(id, bone.getId());
            assertEquals(idToIndex.getOrDefault(parentIds[index], -1).intValue(), bone.getParent());
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
        loadBuiltScene("bend2bones.gltf", meshSetBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        /*
         *  The file includes two bones with a matrix animation on each.
         */
        assertEquals(3, skeletonBuilder.getBonesCount());
        assertEquals(3, animSetBuilder.getAnimations(0).getTracksCount());

        // Bone 0 is located at origin and has no rotation (after up-axis has been applied).
        assertBone(skeletonBuilder.getBones(0), new Vector3d(0.0, 0.0, 0.0), new Quat4d(0.0, 0.0, 0.0, 1.0));

        // Bone 1 is located at (0, 3, 0), without any rotation.
        assertBone(skeletonBuilder.getBones(1), new Vector3d(0.0, 3.0, 0.0), new Quat4d(0.0, 0.0, 0.0, 1.0));

        // Bone 2 is located at (0, 2, 0), without any rotation.
        assertBone(skeletonBuilder.getBones(2), new Vector3d(0.0, 2.0, 0.0), new Quat4d(0.0, 0.0, 0.0, 1.0));

        /*
         * Test bone animations. The file has the following animations:
         *   - First half of the animation consist of bone 1 animating rotation to -90 on Z-axis.
         *   - Second half of the animation consist of bone 2 animating rotation to -90 on Z-axis.
         */
        Quat4d rotIdentity = new Quat4d(0.0, 0.0, 0.0, 1.0);

        int trackCount = animSetBuilder.getAnimations(0).getTracksCount();
        float duration = animSetBuilder.getAnimations(0).getDuration();
        float sampleRate = animSetBuilder.getAnimations(0).getSampleRate();

        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(trackIndex);
            int boneIndex = track.getBoneIndex();

            // There should be no position or scale animation.
            assertAnimationSamePosScale(track);

            int rotKeyframeCount = track.getRotationsCount()/4;

            System.out.printf("testTwoBoneAnimation: bone: %d. keyframeCount: %d, %d  #keyframes: %d\n", boneIndex, rotKeyframeCount, rotKeyframeCount/2, track.getRotationsCount()/4);

            if (track.getRotationsCount() > 0) {
                if (boneIndex == 0) {
                    // The root bone isn't animated so it only has one key
                    assertEquals(4, track.getRotationsCount());
                    assertAnimationRotation(track, 0, rotIdentity);

                } else if (boneIndex == 1) {
                    assertAnimationRotation(track, 0, rotIdentity);
                    assertAnimationRotation(track, rotKeyframeCount/2, new Quat4d(0.0, 0.0, -0.706773, 0.707440));
                    assertAnimationRotation(track, rotKeyframeCount-1, new Quat4d(0.0, 0.0, -0.706773, 0.707440));

                } else if (boneIndex == 2) {
                    assertAnimationRotation(track, 0, rotIdentity);
                    assertAnimationRotation(track, 28, rotIdentity);
                    assertAnimationRotation(track, rotKeyframeCount-1, new Quat4d(0.684243, -0.000646, -0.000000, 0.729254));

                } else {
                    fail("Animations on invalid bone index: " + boneIndex);
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
        loadBuiltScene("bend2bones.gltf", meshSetBuilder, animSetBuilder, skeletonBuilder);

        // Bone scale should be unaffected
        Vector3 boneScale = skeletonBuilder.getBones(1).getLocal().getScale();
        assertEquals(1.0, boneScale.getX(), EPSILON);
        assertEquals(1.0, boneScale.getY(), EPSILON);
        assertEquals(1.0, boneScale.getZ(), EPSILON);

        // Bone positions should be orig_position * unit
        Vector3 bonePosition = skeletonBuilder.getBones(1).getLocal().getTranslation();
        assertEquals(0.0, bonePosition.getX(), EPSILON);
        assertEquals(3.0, bonePosition.getY(), EPSILON);
        assertEquals(0.0, bonePosition.getZ(), EPSILON);

        // Mesh vertex position should also be scaled with unit
        Rig.Mesh mesh = meshSetBuilder.getModels(0).getMeshes(0);

        float minX = 1000000.0f;
        float minY = 1000000.0f;
        float minZ = 1000000.0f;
        float maxX = -1000000.0f;
        float maxY = -1000000.0f;
        float maxZ = -1000000.0f;
        for (int i = 0; i < mesh.getPositionsCount()/3; ++i)
        {
            float x = mesh.getPositions(i*3+0);
            float y = mesh.getPositions(i*3+1);
            float z = mesh.getPositions(i*3+2);

            maxX = Math.max(maxX, x);
            maxY = Math.max(maxY, y);
            maxZ = Math.max(maxZ, z);
            minX = Math.min(minX, x);
            minY = Math.min(minY, y);
            minZ = Math.min(minZ, z);
        }

        assertTrue(minX >= -1.0f);
        assertTrue(minY >=  0.0f);
        assertTrue(minZ >= -1.0f);
        assertTrue(maxX <=  1.0f);
        assertTrue(maxY <=  7.5f);
        assertTrue(maxZ <=  1.0f);
    }

    /*
     * Tests that an invalid collada file is handled
     */
    @Test
    public void testInvalidFile() throws Exception {
        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ModelImporter.Scene scene = loadBuiltScene("broken.gltf", meshSetBuilder, animSetBuilder, skeletonBuilder);
        assertTrue(scene == null);
    }
}
