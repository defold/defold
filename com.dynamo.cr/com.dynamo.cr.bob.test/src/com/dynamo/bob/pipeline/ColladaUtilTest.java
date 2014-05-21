package com.dynamo.bob.pipeline;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;

import java.nio.FloatBuffer;

import org.junit.Test;

public class ColladaUtilTest {
/*
    @Test
    public void testMayaQuad() throws Exception {
        Mesh mesh = ColladaUtil.loadMesh(getClass().getResourceAsStream("maya_quad.dae"));

        FloatBuffer pos = mesh.getPositions();
        FloatBuffer nrm = mesh.getNormals();
        FloatBuffer uvs = mesh.getTexcoord0();
        assertThat(2 * 3 * 3, is(pos.capacity()));
        assertThat(2 * 3 * 3, is(nrm.capacity()));

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
*/
    @Test
    public void testBlenderPolylistQuad() throws Exception {
        Mesh mesh = ColladaUtil.loadMesh(getClass().getResourceAsStream("blender_polylist_quad.dae"));

        FloatBuffer pos = mesh.getPositions();
        FloatBuffer nrm = mesh.getNormals();
        FloatBuffer uvs = mesh.getTexcoord0();
        assertThat(2 * 3 * 3, is(pos.capacity()));
        assertThat(2 * 3 * 3, is(nrm.capacity()));

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

    private void assertVtx(FloatBuffer pos, int i, double xe, double ye) {
        float x = 100 * pos.get(i * 3 + 0);
        float y = 100 * pos.get(i * 3 + 1);

        assertEquals(xe, x, 0.0001);
        assertEquals(ye, y, 0.0001);
    }

    private void assertNrm(FloatBuffer nrm, int i, double xe, double ye, float ze) {
        float x = nrm.get(i * 3 + 0);
        float y = nrm.get(i * 3 + 1);
        float z = nrm.get(i * 3 + 2);

        assertEquals(xe, x, 0.0001);
        assertEquals(ye, y, 0.0001);
        assertEquals(ze, z, 0.0001);
    }

    private void assertUV(FloatBuffer nrm, int i, double ue, double ve) {
        float u = nrm.get(i * 2 + 0);
        float v = nrm.get(i * 2 + 1);

        assertEquals(ue, u, 0.0001);
        assertEquals(ve, v, 0.0001);
    }

}
