package com.dynamo.bob.pipeline;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;

import java.util.List;

import org.eclipse.swt.widgets.Display;
import org.junit.Test;

import com.dynamo.rig.proto.Rig.Mesh;

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
        Mesh mesh = ColladaUtil.loadMesh(getClass().getResourceAsStream("maya_quad.dae"));
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
        Mesh mesh = ColladaUtil.loadMesh(getClass().getResourceAsStream("blender_polylist_quad.dae"));

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

}
