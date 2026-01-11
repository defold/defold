// Copyright 2020-2026 The Defold Foundation
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

package com.dynamo.bob.test.util;

import static org.junit.Assert.assertEquals;

import javax.vecmath.Matrix4d;
import javax.vecmath.Matrix4f;
import javax.vecmath.Quat4d;
import javax.vecmath.Quat4f;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector3f;

import org.junit.Test;

import com.dynamo.bob.util.MathUtil;

public class MathUtilTest {

    static float EPSILON = 0.01f;

    @Test
    public void testVecmath2ToVecmath1() throws Exception {
        org.openmali.vecmath2.Matrix4f mv2 = new org.openmali.vecmath2.Matrix4f(org.openmali.vecmath2.Matrix4f.IDENTITY);
        mv2.setTranslation(new org.openmali.vecmath2.Point3f(10.0f, 0.0f, 0.0f));

        Matrix4f mv1 = MathUtil.vecmath2ToVecmath1(mv2);
        Vector3f p = new Vector3f();
        mv1.get(p);

        assertEquals(10.0f, p.getX(), EPSILON);
    }

    @Test
    public void testVecmath1ToVecmath2() throws Exception {
        Matrix4f mv1 = new Matrix4f();
        mv1.setIdentity();
        mv1.set(new Vector3f(10.0f, 0.0f, 0.0f));

        org.openmali.vecmath2.Matrix4f mv2 = MathUtil.vecmath1ToVecmath2(mv1);
        org.openmali.vecmath2.Vector3f p = new org.openmali.vecmath2.Vector3f();
        mv2.get(p);

        assertEquals(10.0f, p.getX(), EPSILON);
    }

    private void assertVector3f(Vector3f expected, Vector3f actual) {
        assertEquals(expected.getX(), actual.getX(), EPSILON);
        assertEquals(expected.getY(), actual.getY(), EPSILON);
        assertEquals(expected.getZ(), actual.getZ(), EPSILON);
    }

    private void assertQuat4f(Quat4f expected, Quat4f actual) {
        assertEquals(expected.getX(), actual.getX(), EPSILON);
        assertEquals(expected.getY(), actual.getY(), EPSILON);
        assertEquals(expected.getZ(), actual.getZ(), EPSILON);
        assertEquals(expected.getW(), actual.getW(), EPSILON);
    }

    private void assertVector3d(Vector3d expected, Vector3d actual) {
        assertEquals(expected.getX(), actual.getX(), EPSILON);
        assertEquals(expected.getY(), actual.getY(), EPSILON);
        assertEquals(expected.getZ(), actual.getZ(), EPSILON);
    }

    private void assertQuat4d(Quat4d expected, Quat4d actual) {
        assertEquals(expected.getX(), actual.getX(), EPSILON);
        assertEquals(expected.getY(), actual.getY(), EPSILON);
        assertEquals(expected.getZ(), actual.getZ(), EPSILON);
        assertEquals(expected.getW(), actual.getW(), EPSILON);
    }

    private void assertDecomposeDouble(Vector3d translation, Quat4d rotation, Vector3d scale) throws AssertionError {

        // Create TRS matrices
        Matrix4d combinedMtx = new Matrix4d();
        combinedMtx.setIdentity();
        Matrix4d translationMtx = new Matrix4d(combinedMtx);
        Matrix4d rotationMtx = new Matrix4d(combinedMtx);
        Matrix4d scaleMtx = new Matrix4d(combinedMtx);
        translationMtx.setIdentity();
        translationMtx.setElement(0, 3, translation.getX());
        translationMtx.setElement(1, 3, translation.getY());
        translationMtx.setElement(2, 3, translation.getZ());
        rotationMtx.setRotation(rotation);
        scaleMtx.setElement(0, 0, scale.getX());
        scaleMtx.setElement(1, 1, scale.getY());
        scaleMtx.setElement(2, 2, scale.getZ());

        // Combine T*R*S
        combinedMtx.mul(rotationMtx, scaleMtx);
        combinedMtx.mul(translationMtx, combinedMtx);

        // Decompose matrix
        Vector3d decomposedTranslation = new Vector3d();
        Quat4d decomposedRotation = new Quat4d();
        Vector3d decomposedScale = new Vector3d();
        MathUtil.decompose(combinedMtx, decomposedTranslation, decomposedRotation, decomposedScale);

        // Verify the decomposed values are the same as the input values
        assertVector3d(translation, decomposedTranslation);
        assertQuat4d(rotation, decomposedRotation);
        assertVector3d(scale, decomposedScale);
    }

    @Test
    public void testDecomposeDouble() throws Exception {

        Vector3d translation = new Vector3d(0.0, 0.0, 0.0);
        Quat4d   rotation    = new Quat4d(0.0, 0.0, 0.0, 1.0);
        Vector3d scale       = new Vector3d(1.0, 1.0, 1.0);

        // Test identity values
        assertDecomposeDouble(translation, rotation, scale);

        // Translation changes
        translation.set(0.0, 10.0, 0.0);
        assertDecomposeDouble(translation, rotation, scale);

        translation.set(0.0, -10.0, 0.0);
        assertDecomposeDouble(translation, rotation, scale);

        translation.set(10000.0, 0.0, -10000.0);
        assertDecomposeDouble(translation, rotation, scale);

        // Rotation changes
        rotation.set(0.0, 0.0, 1.0, 0.0);
        assertDecomposeDouble(translation, rotation, scale);

        rotation.set(0.0, 1.0, 0.0, 0.0);
        assertDecomposeDouble(translation, rotation, scale);

        rotation.set(1.0, 0.0, 0.0, 0.0);
        assertDecomposeDouble(translation, rotation, scale);

        rotation.set(0.0, 0.0, 0.707, 0.707);
        assertDecomposeDouble(translation, rotation, scale);

        rotation.set(0.0, 0.0, -0.707, 0.707);
        assertDecomposeDouble(translation, rotation, scale);

        // Scale changes
        scale.set(10.0, 1.0, 1.0);
        assertDecomposeDouble(translation, rotation, scale);

        // FIXME: Negative scale does not work correctly currently when decomposing matrices.
//        scale.set(10.0, -1.0, 1.0);
//        assertDecomposeDouble(translation, rotation, scale);
    }

}

