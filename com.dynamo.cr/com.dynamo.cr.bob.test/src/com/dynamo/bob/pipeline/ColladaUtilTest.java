package com.dynamo.bob.pipeline;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;

import org.eclipse.swt.widgets.Display;
import org.jagatoo.loaders.models.collada.datastructs.animation.Bone;
import org.junit.Test;

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

    @Test
    public void testMayaQuad() throws Exception {
        Rig.Mesh.Builder mesh = Rig.Mesh.newBuilder();
        ColladaUtil.loadMesh(getClass().getResourceAsStream("maya_quad.dae"), mesh);
        List<Float> pos = bake(mesh.getIndicesList(), mesh.getPositionsList(), 3);
        List<Float> nrm = bake(mesh.getNormalsIndicesList(), mesh.getNormalsList(), 3);
        List<Float> uvs = bake(mesh.getTexcoord0IndicesList(), mesh.getTexcoord0List(), 2);
        assertThat(2 * 3 * 3, is(pos.size()));
        assertThat(2 * 3 * 3, is(nrm.size()));

        assertVtx(pos, 0, -0.5, -0.5);
        assertVtx(pos, 1, 0.5, -0.5);
        assertVtx(pos, 2, -0.5, 0.5);
        assertVtx(pos, 3, -0.5, 0.5);
        assertVtx(pos, 4, 0.5, -0.5);
        assertVtx(pos, 5, 0.5, 0.5);

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
        ColladaUtil.loadMesh(getClass().getResourceAsStream("blender_polylist_quad.dae"), mesh);

        List<Float> pos = bake(mesh.getIndicesList(), mesh.getPositionsList(), 3);
        List<Float> nrm = bake(mesh.getNormalsIndicesList(), mesh.getNormalsList(), 3);
        List<Float> uvs = bake(mesh.getTexcoord0IndicesList(), mesh.getTexcoord0List(), 2);

        assertThat(2 * 3 * 3, is(pos.size()));
        assertThat(2 * 3 * 3, is(nrm.size()));
        assertThat(2 * 3 * 2, is(uvs.size()));

        assertVtx(pos, 0, -100, -100);
        assertVtx(pos, 1, 100, -100);
        assertVtx(pos, 2, 100, 100);
        assertVtx(pos, 3, -100, -100);
        assertVtx(pos, 4, 100, 100);
        assertVtx(pos, 5, -100, 100);

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
        ArrayList<Bone> bones = ColladaUtil.loadSkeleton(getClass().getResourceAsStream("blender_animated_cube.dae"), skeleton, new ArrayList<String>());
        Rig.AnimationSet.Builder animation = Rig.AnimationSet.newBuilder();
        ColladaUtil.loadAnimations(getClass().getResourceAsStream("blender_animated_cube.dae"), animation, bones, 16.0f, new ArrayList<String>());
        //assert(0.0, animation.getDuration());
        assertEquals(1, animation.getAnimationsCount());
    }

    private void assertVtx(List<Float> pos, int i, double xe, double ye) {
        float x = 100 * pos.get(i * 3 + 0);
        float y = 100 * pos.get(i * 3 + 1);

        assertEquals(xe, x, EPSILON);
        assertEquals(ye, y, EPSILON);
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

    @Test
    public void testSkeleton() throws Exception {
        // Temp test (and temp data)
        Rig.Mesh.Builder mesh = Rig.Mesh.newBuilder();
        ColladaUtil.loadMesh(getClass().getResourceAsStream("simple_anim.dae"), mesh);

        String[] boneIds   = {"root", "l_hip", "l_knee", "l_ankle", "l_null_toe", "pelvis", "spine",  "l_humerus", "l_ulna",    "l_wrist", "r_humerus", "r_ulna",    "r_wrist", "neck",  "null_head", "r_hip", "r_knee", "r_ankle", "r_null_toe"};
        String[] parentIds = {null,   "root",  "l_hip",  "l_knee",  "l_ankle",    "root",   "pelvis", "spine",     "l_humerus", "l_ulna",  "spine",     "r_humerus", "r_ulna",  "spine", "neck",      "root",  "r_hip",  "r_knee",  "r_ankle"};

        Rig.Skeleton.Builder skeleton = Rig.Skeleton.newBuilder();
        ColladaUtil.loadSkeleton(getClass().getResourceAsStream("simple_anim.dae"), skeleton, new ArrayList<String>());
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

    /* Currently throws exception due to lack of support for anim clips */
    @Test(expected=LoaderException.class)
    public void testAnimClip() throws Exception {
        Rig.Skeleton.Builder skeleton = Rig.Skeleton.newBuilder();
        ArrayList<Bone> bones = ColladaUtil.loadSkeleton(getClass().getResourceAsStream("simple_anim.dae"), skeleton, new ArrayList<String>());
        Rig.AnimationSet.Builder animation = Rig.AnimationSet.newBuilder();
        ArrayList<String> idList1 = new ArrayList<String>();
        ColladaUtil.loadAnimations(getClass().getResourceAsStream("simple_anim.dae"), animation, bones, 16.0f, idList1);
        ArrayList<String> idList2 = new ArrayList<String>();
        ColladaUtil.loadAnimationIds(getClass().getResourceAsStream("simple_anim.dae"), idList2);
        assertEquals(true, idList1.equals(idList2));
    }

    private static void quatToEuler(Quat4d quat, Tuple3d euler) {
        double heading;
        double attitude;
        double bank;
        double test = quat.x * quat.y + quat.z * quat.w;
        if (test > 0.499)
        { // singularity at north pole
            heading = 2 * Math.atan2(quat.x, quat.w);
            attitude = Math.PI / 2;
            bank = 0;
        }
        else if (test < -0.499)
        { // singularity at south pole
            heading = -2 * Math.atan2(quat.x, quat.w);
            attitude = -Math.PI / 2;
            bank = 0;
        }
        else
        {
            double sqx = quat.x * quat.x;
            double sqy = quat.y * quat.y;
            double sqz = quat.z * quat.z;
            heading = Math.atan2(2 * quat.y * quat.w - 2 * quat.x * quat.z, 1 - 2 * sqy - 2 * sqz);
            attitude = Math.asin(2 * test);
            bank = Math.atan2(2 * quat.x * quat.w - 2 * quat.y * quat.z, 1 - 2 * sqx - 2 * sqz);
        }
        euler.x = bank * 180 / Math.PI;
        euler.y = heading * 180 / Math.PI;
        euler.z = attitude * 180 / Math.PI;
    }

    @Test
    public void testBoneAnimation() throws Exception {
        Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();
        Rig.AnimationSet.Builder animSetBuilder = Rig.AnimationSet.newBuilder();
        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        ColladaUtil.load(getClass().getResourceAsStream("one_vertice_bone.dae"), meshBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());

        // The bone is animated with a matrix track in the DAE,
        // which expands into 3 separate tracks in our format;
        // position, rotation and scale.
        assertEquals(3, animSetBuilder.getAnimations(0).getTracksCount());

        int trackCount = animSetBuilder.getAnimations(0).getTracksCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++) {

            Rig.AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(trackIndex);
            assertEquals(0, track.getBoneIndex());

            // The collada file does not animate either position or scale for the bones,
            // but since the input animations are matrices we don't "know" that. But we
            // will verify that they do not change.
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
            } else if (track.getRotationsCount() > 0) {
                // Assert that the rotation keyframes keeps increasing rotation around X
                Point3d rot = new Point3d(0.0,0.0,0.0);
                Quat4d rQ = new Quat4d(track.getRotations(0), track.getRotations(1), track.getRotations(2), track.getRotations(3));
                quatToEuler(rQ, rot);
                double lastXRot = rot.getX();

                int rotCount = track.getRotationsCount() / 4;
                for (int i = 1; i < rotCount; i++) {
                    rQ = new Quat4d(track.getRotations(i*4), track.getRotations(i*4+1), track.getRotations(i*4+2), track.getRotations(i*4+3));
                    quatToEuler(rQ, rot);
                    if (rot.getX() <= lastXRot) {
                        fail("Rotation is not increasing. Previously: " + lastXRot + ", now: " + rot.getX());
                    }

                    lastXRot = rot.getX();
                }
            }

        }

        // The bone is located in origo and rotated -90 degrees around X.
        assertEquals(1, skeletonBuilder.getBonesCount());
        assertEquals(0.0, (double)skeletonBuilder.getBones(0).getPosition().getX(), EPSILON);
        assertEquals(0.0, (double)skeletonBuilder.getBones(0).getPosition().getY(), EPSILON);
        assertEquals(0.0, (double)skeletonBuilder.getBones(0).getPosition().getZ(), EPSILON);
        assertEquals(0.0, (double)skeletonBuilder.getBones(0).getRotation().getX(), EPSILON);
        assertEquals(0.0, (double)skeletonBuilder.getBones(0).getRotation().getY(), EPSILON);
        assertEquals(-0.70710677, (double)skeletonBuilder.getBones(0).getRotation().getZ(), EPSILON);
        assertEquals( 0.70710677, (double)skeletonBuilder.getBones(0).getRotation().getW(), EPSILON);

    }

}
