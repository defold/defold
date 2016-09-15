package com.dynamo.bob.pipeline;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;

import java.util.List;

import org.eclipse.swt.widgets.Display;
import org.jagatoo.loaders.models.collada.stax.XMLCOLLADA;
import org.junit.Test;

import com.dynamo.rig.proto.Rig.AnimationSet;
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
        XMLCOLLADA collada = ColladaUtil.loadDAE(getClass().getResourceAsStream("maya_quad.dae"));
        Mesh mesh = ColladaUtil.loadMesh(collada);
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
        XMLCOLLADA collada = ColladaUtil.loadDAE(getClass().getResourceAsStream("blender_polylist_quad.dae"));
        Mesh mesh = ColladaUtil.loadMesh(collada);

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
        XMLCOLLADA collada = ColladaUtil.loadDAE(getClass().getResourceAsStream("blender_animated_cube.dae"));
        Skeleton skeleton = ColladaUtil.loadSkeleton(collada);
        AnimationSet animation = ColladaUtil.loadAnimations(collada, skeleton, 1.0f / 24.0f);
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
        XMLCOLLADA collada = ColladaUtil.loadDAE(getClass().getResourceAsStream("simple_anim.dae"));
        Skeleton skeleton = ColladaUtil.loadSkeleton(collada);
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


}
