package com.dynamo.bob.test.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.InputStream;
import java.net.URL;
import java.util.Enumeration;

import javax.vecmath.Point2d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;
import javax.vecmath.Tuple4d;
import javax.vecmath.Vector2d;
import javax.vecmath.Vector3d;

import org.apache.commons.io.IOUtils;
import org.junit.Test;
import org.osgi.framework.Bundle;
import org.osgi.framework.FrameworkUtil;

import com.dynamo.bob.textureset.TextureSetGenerator.UVTransform;
import com.dynamo.bob.util.SpineSceneUtil;
import com.dynamo.bob.util.RigUtil.Animation;
import com.dynamo.bob.util.RigUtil.AnimationCurve;
import com.dynamo.bob.util.RigUtil.AnimationTrack;
import com.dynamo.bob.util.RigUtil.AnimationTrack.Property;
import com.dynamo.bob.util.RigUtil.Bone;
import com.dynamo.bob.util.RigUtil.EventTrack;
import com.dynamo.bob.util.RigUtil.Mesh;
import com.dynamo.bob.util.RigUtil.Transform;
import com.dynamo.bob.util.RigUtil.UVTransformProvider;

public class SpineSceneTest {
    private static final double EPSILON = 0.000001;

    private static class TestUVTProvider implements UVTransformProvider {
        @Override
        public UVTransform getUVTransform(String animId) {
            return new UVTransform(new Point2d(0.0, 0.0), new Vector2d(0.5, 1.0), false);
        }
    }

    private static void assertTuple3(double x, double y, double z, Tuple3d t) {
        assertEquals(x, t.x, EPSILON);
        assertEquals(y, t.y, EPSILON);
        assertEquals(z, t.z, EPSILON);
    }

    private static void assertTuple3(Tuple3d expected, Tuple3d actual) {
        assertTuple3(expected.x, expected.y, expected.z, actual);
    }

    private static void assertTuple4(double x, double y, double z, double w, Tuple4d t) {
        assertEquals(x, t.x, EPSILON);
        assertEquals(y, t.y, EPSILON);
        assertEquals(z, t.z, EPSILON);
        assertEquals(w, t.w, EPSILON);
    }

    private static void assertTuple4(Tuple4d expected, Tuple4d actual) {
        assertTuple4(expected.x, expected.y, expected.z, expected.w, actual);
    }

    private static void assertTransform(Transform t, Point3d position, Quat4d rotation, Vector3d scale) {
        if (position != null) {
            assertTuple3(position, t.position);
        }
        if (rotation != null) {
            assertTuple4(rotation, t.rotation);
        }
        if (scale != null) {
            assertTuple3(scale, t.scale);
        }
    }

    private static void assertFloatArrays(float[] expected, float[] actual) {
        assertEquals(expected.length, actual.length);
        for (int i = 0; i < expected.length; ++i) {
            assertEquals(expected[i], actual[i], EPSILON);
        }
    }

    private static void assertIntArrays(int[] expected, int[] actual) {
        assertEquals(expected.length, actual.length);
        for (int i = 0; i < expected.length; ++i) {
            assertEquals(expected[i], actual[i]);
        }
    }

    @Test
    public void testTransformRot() {
        Transform t = new Transform();
        t.setZAngleDeg(90);
        double halfSq2 = 0.5 * Math.sqrt(2.0);
        assertTuple4(0.0, 0.0, halfSq2, halfSq2, t.rotation);
    }

    @Test
    public void testTransformMul() {
        Transform t1 = new Transform();
        t1.position.set(1.0, 2.0, 3.0);
        t1.setZAngleDeg(90.0);
        t1.scale.set(1.0, 2.0, 3.0);
        Transform t2 = new Transform();
        t2.position.set(2.0, 3.0, 4.0);
        t2.scale.set(2.0, 3.0, 4.0);
        t1.mul(t2);
        assertTuple3(-5.0, 4.0, 15.0, t1.position);
        double halfSq2 = 0.5 * Math.sqrt(2.0);
        assertTuple4(0.0, 0.0, halfSq2, halfSq2, t1.rotation);
        assertTuple3(2.0, 6.0, 12.0, t1.scale);
    }

    @Test
    public void testTransformInv() {
        Transform t = new Transform();
        t.position.set(1.0, 2.0, 3.0);
        t.setZAngleDeg(90.0);
        t.scale.set(1.0, 2.0, 3.0);
        Transform identity = new Transform(t);
        identity.inverse();
        identity.mul(t);
        assertTuple3(0.0, 0.0, 0.0, identity.position);
        assertTuple4(0.0, 0.0, 0.0, 1.0, identity.rotation);
        assertTuple3(1.0, 1.0, 1.0, identity.scale);
    }

    private SpineSceneUtil load() throws Exception {
        InputStream input = null;
        try {
            Bundle bundle = FrameworkUtil.getBundle(getClass());
            Enumeration<URL> entries = bundle.findEntries("/test", "skeleton.json", false);
            if (entries.hasMoreElements()) {
                input = entries.nextElement().openStream();
                return SpineSceneUtil.loadJson(input, new TestUVTProvider());
            }
            return null;
        } catch (Exception e) {
            throw e;
        } finally {
            IOUtils.closeQuietly(input);
        }
    }

    @Test
    public void testLoadingBones() throws Exception {
        SpineSceneUtil scene = load();
        assertEquals(9, scene.bones.size());
        Bone root = scene.getBone("root");
        Bone animated = scene.getBone("bone_animated");
        Bone scale = scene.getBone("bone_scale");
        Bone noScale = scene.getBone("bone_noscale");
        Bone rotated = scene.getBone("bone_rotated");
        assertEquals(scene.getBone(0), root);
        assertEquals(scene.getBone(1), animated);
        assertEquals(scene.getBone(2), noScale);
        assertEquals(scene.getBone(3), rotated);
        assertEquals(scene.getBone(4), scale);
        assertEquals(2, noScale.index);
        assertTrue(scale.inheritScale);
        assertFalse(noScale.inheritScale);

        // Bone transforms
        assertTransform(root.localT, null, null, new Vector3d(2.0, 1.0, 1.0));
        assertTransform(root.worldT, null, null, new Vector3d(2.0, 1.0, 1.0));
        assertTransform(root.invWorldT, null, null, new Vector3d(0.5, 1.0, 1.0));

        assertTransform(scale.localT, new Point3d(100.0, 100.0, 0.0), null, new Vector3d(1.0, 1.0, 1.0));
        assertTransform(scale.worldT, new Point3d(200.0, 150.0, 0.0), null, new Vector3d(2.0, 1.0, 1.0));

        assertTransform(noScale.localT, new Point3d(100.0, 100.0, 0.0), null, new Vector3d(1.0, 1.0, 1.0));
        assertTransform(noScale.worldT, new Point3d(200.0, 150.0, 0.0), null, new Vector3d(1.0, 1.0, 1.0));

        double halfSqrt2 = 0.5 * Math.sqrt(2.0);
        assertTransform(rotated.localT, null, new Quat4d(0.0, 0.0, halfSqrt2, halfSqrt2), null);
    }

    private void assertMesh(Mesh mesh, String path, float[] vertices, int[] triangles, int[] boneIndices, float[] boneWeights) {
        assertEquals(path, mesh.path);
        assertFloatArrays(vertices, mesh.vertices);
        assertIntArrays(triangles, mesh.triangles);
        assertIntArrays(boneIndices, mesh.boneIndices);
        assertFloatArrays(boneWeights, mesh.boneWeights);
    }

    @Test
    public void testLoadingMeshes() throws Exception {
        SpineSceneUtil scene = load();
        assertEquals(3, scene.meshes.size());
        assertMesh(scene.meshes.get(0), "test_sprite",
                new float[] {
                    100.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                    100.0f, 100.0f, 0.0f, 0.0f, 0.0f,
                    300.0f, 0.0f, 0.0f, 0.5f, 1.0f,
                    300.0f, 100.0f, 0.0f, 0.5f, 0.0f,
                },
                new int[] {
                    0, 1, 2,
                    2, 1, 3,
                },
                new int[] {
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                },
                new float[] {
                    1.0f, 0.0f, 0.0f, 0.0f,
                    1.0f, 0.0f, 0.0f, 0.0f,
                    1.0f, 0.0f, 0.0f, 0.0f,
                    1.0f, 0.0f, 0.0f, 0.0f,
                }
        );
        assertMesh(scene.meshes.get(1), "test_sprite",
                new float[] {
                    100.0f, 0.0f, 0.0f, 0.5f, 1.0f,
                    -100.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                    -100.0f, 100.0f, 0.0f, 0.0f, 0.0f,
                    100.0f, 100.0f, 0.0f, 0.5f, 0.0f
                },
                new int[] {
                    1, 2, 3,
                    1, 3, 0,
                },
                new int[] {
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                },
                new float[] {
                    1.0f, 0.0f, 0.0f, 0.0f,
                    1.0f, 0.0f, 0.0f, 0.0f,
                    1.0f, 0.0f, 0.0f, 0.0f,
                    1.0f, 0.0f, 0.0f, 0.0f,
                }
        );
        assertMesh(scene.meshes.get(2), "test_sprite",
                new float[] {
                    100.0f, 100.0f, 0.0f, 0.5f, 1.0f,
                    -100.0f, 100.0f, 0.0f, 0.0f, 1.0f,
                    -100.0f, 200.0f, 0.0f, 0.0f, 0.0f,
                    100.0f, 200.0f, 0.0f, 0.5f, 0.0f,
                },
                new int[] {
                        1, 2, 3,
                        1, 3, 0,
                },
                new int[] {
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                    0, 4, 0, 0, // third vertex mapped 50% to bone 0 and 4
                    0, 0, 0, 0,
                },
                new float[] {
                    1.0f, 0.0f, 0.0f, 0.0f,
                    1.0f, 0.0f, 0.0f, 0.0f,
                    0.5f, 0.5f, 0.0f, 0.0f, // third vertex mapped 50% to bone 0 and 4
                    1.0f, 0.0f, 0.0f, 0.0f,
                }
        );
    }

    private static void assertSimpleAnim(SpineSceneUtil scene, String name, Property property, float[][] values) {
        Animation anim = scene.getAnimation(name);
        assertEquals(1.0, anim.duration, EPSILON);
        assertEquals(1, anim.tracks.size());
        AnimationTrack track = anim.tracks.get(0);
        assertEquals(scene.getBone("bone_animated"), track.bone);
        assertEquals(property, track.property);
        assertEquals(2, track.keys.size());
        for (int i = 0; i < values.length; ++i) {
            assertFloatArrays(values[i], track.keys.get(i).value);
        }
    }

    private static void assertEvents(SpineSceneUtil scene, String name, String eventId, Object[] values) {
        Animation anim = scene.getAnimation(name);
        assertEquals(1, anim.eventTracks.size());
        EventTrack track = anim.eventTracks.get(0);
        assertEquals(eventId, track.name);
        for (int i = 0; i < values.length; ++i) {
            Object o = values[i];
            if (o instanceof Integer) {
                assertEquals(o, track.keys.get(i).intPayload);
            } else if (o instanceof Float) {
                assertEquals(o, track.keys.get(i).floatPayload);
            } else if (o instanceof String) {
                assertEquals(o, track.keys.get(i).stringPayload);
            } else {
                assertTrue(false);
            }
        }
    }

    @Test
    public void testLoadingAnims() throws Exception {
        SpineSceneUtil scene = load();
        assertEquals(8, scene.animations.size());

        assertSimpleAnim(scene, "anim_pos", Property.POSITION, new float[][] {new float[] {0.0f, 0.0f, 0.0f}, new float[] {100.0f, 0.0f, 0.0f}});
        assertSimpleAnim(scene, "anim_rot", Property.ROTATION, new float[][] {new float[] {0.0f}, new float[] {90.0f}});
        assertSimpleAnim(scene, "anim_scale", Property.SCALE, new float[][] {new float[] {1.0f, 1.0f, 1.0f}, new float[] {2.0f, 1.0f, 1.0f}});

        Animation animCurve = scene.getAnimation("anim_curve");
        AnimationCurve curve = animCurve.tracks.get(0).keys.get(0).curve;
        assertFloatArrays(new float[] {
                0.0f, 0.0f, 1.0f, 1.0f
        }, new float[] {
                curve.x0, curve.y0, curve.x1, curve.y1
        });

        Animation animMulti = scene.getAnimation("anim_multi");
        assertEquals(3, animMulti.tracks.size());
        assertEquals(1.0, animMulti.duration, EPSILON);

        Animation animStepped = scene.getAnimation("anim_stepped");
        assertTrue(animStepped.tracks.get(0).keys.get(0).stepped);

        assertEvents(scene, "anim_event", "test_event", new Object[] {1, 0.5f, "test_string"});
    }

    @Test
    public void testEmptyScene() throws Exception {
        InputStream input = null;
        try {
            input = getClass().getResourceAsStream("empty.json");
            SpineSceneUtil scene = SpineSceneUtil.loadJson(input, new TestUVTProvider());
            assertEquals(1, scene.bones.size());
            assertEquals(0, scene.meshes.size());
            assertEquals(0, scene.animations.size());
        } finally {
            IOUtils.closeQuietly(input);
        }
    }

    @Test
    public void testSampleScenes() throws Exception {
        for (int i = 1; i < 9; ++i) {
            InputStream input = null;
            try {
                input = getClass().getResourceAsStream(String.format("sample%d.json", i));
                SpineSceneUtil scene = SpineSceneUtil.loadJson(input, new TestUVTProvider());
                assertTrue(0 < scene.bones.size());
                assertTrue(0 < scene.meshes.size());
                assertTrue(0 < scene.animations.size());
            } finally {
                IOUtils.closeQuietly(input);
            }
        }
    }
}
