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

import org.junit.Test;

import com.dynamo.bob.util.BezierUtil;

import static org.junit.Assert.*;

public class BezierUtilTest {

    private static final double EPSILON = 0.000001;

    private static class BezierCurve {
        private double v0, v1, v2, v3;

        public BezierCurve(double v0, double v1, double v2, double v3) {
            this.v0 = v0;
            this.v1 = v1;
            this.v2 = v2;
            this.v3 = v3;
        }

        public double eval(double t) {
            return BezierUtil.curve(t, this.v0, this.v1, this.v2, this.v3);
        }

        public double findT(double v) {
            return BezierUtil.findT(v, this.v0, this.v1, this.v2, this.v3);
        }
    }

    @Test
    public void testCurve() {
        // "extreme" curve
        BezierCurve c = new BezierCurve(0.0, 1.0, 0.0, 1.0);
        assertEquals(0.0, c.eval(0.0), EPSILON);
        assertTrue(c.eval(0.25) > 0.2);
        assertEquals(0.5, c.eval(0.5), EPSILON);
        assertTrue(c.eval(0.75) < 0.8);
        assertEquals(1.0, c.eval(1.0), EPSILON);

        int iterations = 16;
        for (int i = 0; i <= iterations; ++i) {
            double v = i / (double)iterations;
            double t = c.findT(v);
            assertEquals(v, c.eval(t), 0.001);
        }
    }
}
