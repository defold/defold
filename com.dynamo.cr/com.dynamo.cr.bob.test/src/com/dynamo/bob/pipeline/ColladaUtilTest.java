package com.dynamo.bob.pipeline;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;

import org.eclipse.swt.widgets.Display;
import org.jagatoo.loaders.models.collada.stax.XMLCOLLADA;
import org.junit.Test;

import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.AnimationTrack;
import com.dynamo.rig.proto.Rig.Bone;
import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.rig.proto.Rig.Skeleton;

public class ColladaUtilTest {

    public ColladaUtilTest(){
        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }
    }


    @Test
    public void testMayaQuad() throws Exception {
        Mesh.Builder mesh = Mesh.newBuilder();
        ColladaUtil.loadMesh(getClass().getResourceAsStream("maya_quad.dae"), mesh);
        List<Float> pos = mesh.getPositionsList();
        List<Float> nrm = mesh.getNormalsList();
        List<Float> uvs = mesh.getTexcoord0List();
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

        assertUV(uvs, 0, 0, 0);
        assertUV(uvs, 1, 1, 0);
        assertUV(uvs, 2, 0, 1);
        assertUV(uvs, 3, 0, 1);
        assertUV(uvs, 4, 1, 0);
        assertUV(uvs, 5, 1, 1);
    }

    @Test
    public void testBlenderPolylistQuad() throws Exception {
        Mesh.Builder mesh = Mesh.newBuilder();
        ColladaUtil.loadMesh(getClass().getResourceAsStream("blender_polylist_quad.dae"), mesh);

        List<Float> pos = mesh.getPositionsList();
        List<Float> nrm = mesh.getNormalsList();
        List<Float> uvs = mesh.getTexcoord0List();
        assertThat(2 * 3 * 3, is(pos.size()));
        assertThat(2 * 3 * 3, is(nrm.size()));

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
        Skeleton.Builder skeleton = Skeleton.newBuilder();
        ColladaUtil.loadSkeleton(getClass().getResourceAsStream("blender_animated_cube.dae"), skeleton, new ArrayList<String>());
        AnimationSet.Builder animation = AnimationSet.newBuilder();
        ColladaUtil.loadAnimations(getClass().getResourceAsStream("blender_animated_cube.dae"), animation, skeleton.clone().build(), 16.0f, new ArrayList<String>());
        //assert(0.0, animation.getDuration());
        assertEquals(1, animation.getAnimationsCount());
    }

    private void assertVtx(List<Float> pos, int i, double xe, double ye) {
        float x = 100 * pos.get(i * 3 + 0);
        float y = 100 * pos.get(i * 3 + 1);

        assertEquals(xe, x, 0.0001);
        assertEquals(ye, y, 0.0001);
    }

    private void assertNrm(List<Float> nrm, int i, double xe, double ye, float ze) {
        float x = nrm.get(i * 3 + 0);
        float y = nrm.get(i * 3 + 1);
        float z = nrm.get(i * 3 + 2);

        assertEquals(xe, x, 0.0001);
        assertEquals(ye, y, 0.0001);
        assertEquals(ze, z, 0.0001);
    }

    private void assertUV(List<Float> nrm, int i, double ue, double ve) {
        float u = nrm.get(i * 2 + 0);
        float v = nrm.get(i * 2 + 1);

        assertEquals(ue, u, 0.0001);
        assertEquals(ve, v, 0.0001);
    }

    @Test
    public void testSkeleton() throws Exception {
        // Temp test (and temp data)
        Mesh.Builder mesh = Mesh.newBuilder();
        ColladaUtil.loadMesh(getClass().getResourceAsStream("simple_anim.dae"), mesh);

        Skeleton.Builder skeleton = Skeleton.newBuilder();
        ColladaUtil.loadSkeleton(getClass().getResourceAsStream("simple_anim.dae"), skeleton, new ArrayList<String>());
        List<Bone> bones = skeleton.getBonesList();
        assertEquals(65535, bones.get(0).getParent()); // fake root
        assertEquals(0,     bones.get(1).getParent()); // "real" root
        assertEquals(1,     bones.get(2).getParent());
        assertEquals(2,     bones.get(3).getParent());
        assertEquals(3,     bones.get(4).getParent());
        assertEquals(4,     bones.get(5).getParent());
        assertEquals(1,     bones.get(6).getParent());
        assertEquals(6,     bones.get(7).getParent());
        assertEquals(7,     bones.get(8).getParent());
        assertEquals(8,     bones.get(9).getParent());
        assertEquals(9,     bones.get(10).getParent());
        assertEquals(7,     bones.get(11).getParent());
        assertEquals(11,    bones.get(12).getParent());
        assertEquals(12,    bones.get(13).getParent());
        assertEquals(7,     bones.get(14).getParent());
        assertEquals(14,    bones.get(15).getParent());
        assertEquals(1,     bones.get(16).getParent());
        assertEquals(16,    bones.get(17).getParent());
        assertEquals(17,    bones.get(18).getParent());
        assertEquals(18,    bones.get(19).getParent());
    }

    @Test
    public void testAnimClip() throws Exception {
        Skeleton.Builder skeleton = Skeleton.newBuilder();
        ColladaUtil.loadSkeleton(getClass().getResourceAsStream("simple_anim.dae"), skeleton, new ArrayList<String>());
        AnimationSet.Builder animation = AnimationSet.newBuilder();
        ArrayList<String> idList1 = new ArrayList<String>();
        ColladaUtil.loadAnimations(getClass().getResourceAsStream("simple_anim.dae"), animation, skeleton.clone().build(), 16.0f, idList1);
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
        Mesh.Builder meshBuilder = Mesh.newBuilder();
        AnimationSet.Builder animSetBuilder = AnimationSet.newBuilder();
        Skeleton.Builder skeletonBuilder = Skeleton.newBuilder();
        ColladaUtil.load(getClass().getResourceAsStream("one_vertice_bone.dae"), meshBuilder, animSetBuilder, skeletonBuilder);
        assertEquals(1, animSetBuilder.getAnimationsCount());
        assertEquals(1, animSetBuilder.getAnimations(0).getTracksCount());
        
        AnimationTrack track = animSetBuilder.getAnimations(0).getTracks(0);
        assertEquals(0, track.getBoneIndex());
        
        for (int i = 0; i < track.getPositionsCount(); i++) {
            assertEquals(0.0, track.getPositions(i), 0.0001);
        }
        
        for (int i = 0; i < track.getScaleCount(); i++) {
            assertEquals(1.0, track.getScale(i), 0.0001);
        }
        
        // Assert that the rotation keyframes keeps increasing rotation around X
        Point3d rot = new Point3d(0.0,0.0,0.0);
        Quat4d rQ = new Quat4d(track.getRotations(0), track.getRotations(1), track.getRotations(2), track.getRotations(3));
        quatToEuler(rQ, rot);
        double lastXRot = rot.getX();
        for (int i = 1; i < track.getRotationsCount() / 4; i++) {
            rQ = new Quat4d(track.getRotations(i*4), track.getRotations(i*4+1), track.getRotations(i*4+2), track.getRotations(i*4+3));
            quatToEuler(rQ, rot);
            
            assertTrue(rot.getX() > lastXRot);
            lastXRot = rot.getX();
        }
        
        assertEquals(1, skeletonBuilder.getBonesCount());
        assertEquals(0.0, (double)skeletonBuilder.getBones(0).getPosition().getX(), 0.0001);
        assertEquals(0.0, (double)skeletonBuilder.getBones(0).getPosition().getY(), 0.0001);
        assertEquals(0.0, (double)skeletonBuilder.getBones(0).getPosition().getZ(), 0.0001);

        // File is exported with blender
        System.out.println(skeletonBuilder.getBones(0).getRotation());
//        assertEquals(0.0, (double)skeletonBuilder.getBones(0).getRotation().getX(), 0.0001);
//        assertEquals(0.0, (double)skeletonBuilder.getBones(0).getRotation().getY(), 0.0001);
//        assertEquals(-0.70710677, (double)skeletonBuilder.getBones(0).getRotation().getZ(), 0.0001);
//        assertEquals( 0.70710677, (double)skeletonBuilder.getBones(0).getRotation().getW(), 0.0001);
        
    }

}
