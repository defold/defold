package com.dynamo.bob.util;

import java.util.ArrayList;
import java.util.List;

import javax.vecmath.AxisAngle4d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

import com.dynamo.bob.textureset.TextureSetGenerator.UVTransform;

/**
 * Convenience class for loading spine json data.
 *
 * Should preferably have been an extension to Bob rather than located inside it.
 */
public class RigUtil {
    @SuppressWarnings("serial")
    public static class LoadException extends Exception {
        public LoadException(String msg) {
            super(msg);
        }
    }

    public interface UVTransformProvider {
        UVTransform getUVTransform(String animId);
    }

    public static class Transform {
        public Point3d position = new Point3d();
        public Quat4d rotation = new Quat4d();
        public Vector3d scale = new Vector3d();

        public Transform() {
            this.position.set(0.0, 0.0, 0.0);
            this.rotation.set(0.0, 0.0, 0.0, 1.0);
            this.scale.set(1.0, 1.0, 1.0);
        }

        public Transform(Transform t) {
            set(t);
        }

        public void set(Transform t) {
            this.position.set(t.position);
            this.rotation.set(t.rotation);
            this.scale.set(t.scale);
        }

        public void setZAngleDeg(double angle) {
            double rads = angle * Math.PI / 180.0;
            this.rotation.set(new AxisAngle4d(new Vector3d(0.0, 0.0, 1.0), rads));
        }

        public void apply(Point3d p) {
            p.set(this.scale.x * p.x, this.scale.y * p.y, this.scale.z * p.z);
            MathUtil.rotate(this.rotation, p);
            p.add(this.position);
        }

        public void mul(Transform t) {
            // Scale and rotate position
            Point3d p = new Point3d(t.position);
            apply(p);
            this.position.set(p);
            // Intentionally non-rotated scale to avoid shearing in the transform
            this.scale.set(this.scale.x * t.scale.x, this.scale.y * t.scale.y, this.scale.z * t.scale.z);
            // Rotation
            this.rotation.mul(t.rotation);
        }

        public void inverse() {
            this.scale.set(1.0 / this.scale.x, 1.0 / this.scale.y, 1.0 / this.scale.z);
            this.rotation.conjugate();
            Point3d p = new Point3d(this.position);
            p.scale(-1.0);
            this.position.set(0.0, 0.0, 0.0);
            apply(p);
            this.position.set(p);
        }

        public void toMatrix4d(Matrix4d m) {
            m.set(this.rotation);
            m.setElement(0, 0, m.getElement(0, 0) * this.scale.getX());
            m.setElement(1, 1, m.getElement(1, 1) * this.scale.getY());
            m.setElement(2, 2, m.getElement(2, 2) * this.scale.getZ());
            m.setColumn(3, this.position.x, this.position.y, this.position.z, 1.0);
        }
    }

    public static class Bone {
        public String name = "";
        public Transform localT = new Transform();
        public Transform worldT = new Transform();
        public Transform invWorldT = new Transform();
        public Bone parent = null;
        public int index = -1;
        public boolean inheritScale = true;
        public double length = 0.0;
    }

    public static class IK {
        public String name = "";
        public Bone parent = null;
        public Bone child = null;
        public Bone target = null;
        public boolean positive = true;
        public float mix = 1.0f;
        public int index = -1;
    }

    public static class Mesh {
        public String attachment;
        public String path;
        public Slot slot;
        public boolean visible;
        // format is: x0, y0, z0, u0, v0, ...
        public float[] vertices;
        public int[] triangles;
        public float[] boneWeights;
        public int[] boneIndices;
    }

    public static class Slot {
        public String name;
        public Bone bone;
        public int index;
        public String attachment;
        public float[] color = new float[] {1.0f, 1.0f, 1.0f, 1.0f};

        public Slot(String name, Bone bone, int index, String attachment) {
            this.name = name;
            this.bone = bone;
            this.index = index;
            this.attachment = attachment;
        }
    }

    public static class AnimationCurve {
        public float x0;
        public float y0;
        public float x1;
        public float y1;
    }

    public static class AnimationKey {
        public float t;
        public float[] value = new float[4];
        public boolean stepped;
        public AnimationCurve curve;
    }

    public static abstract class AbstractAnimationTrack<Key extends AnimationKey> {
        public List<Key> keys = new ArrayList<Key>();
    }

    public static class AnimationTrack extends AbstractAnimationTrack<AnimationKey> {
        public enum Property {
            POSITION, ROTATION, SCALE
        }
        public Bone bone;
        public Property property;
    }

    public static class IKAnimationKey extends AnimationKey {
        public float mix;
        public boolean positive;
    }

    public static class IKAnimationTrack extends AbstractAnimationTrack<IKAnimationKey> {
        public IK ik;
    }

    public static class SlotAnimationKey extends AnimationKey {
        public String attachment;
        public int orderOffset;
    }

    public static class SlotAnimationTrack extends AbstractAnimationTrack<SlotAnimationKey> {
        public enum Property {
            ATTACHMENT, COLOR, DRAW_ORDER
        }
        public Slot slot;
        public Property property;
    }

    public static class Event {
        public String name;
        public String stringPayload;
        public float floatPayload;
        public int intPayload;
    }

    public static class EventKey {
        public float t;
        public String stringPayload;
        public float floatPayload;
        public int intPayload;
    }

    public static class EventTrack {
        public String name;
        public List<EventKey> keys = new ArrayList<EventKey>();
    }

    public static class Animation {
        public String name;
        public float duration;
        public List<AnimationTrack> tracks = new ArrayList<AnimationTrack>();
        public List<IKAnimationTrack> iKTracks = new ArrayList<IKAnimationTrack>();
        public List<SlotAnimationTrack> slotTracks = new ArrayList<SlotAnimationTrack>();
        public List<EventTrack> eventTracks = new ArrayList<EventTrack>();
    }

    private class Weight implements Comparable<Weight> {
        public Point3d p;
        public int boneIndex;
        public float weight;

        public Weight(Point3d p, int boneIndex, float weight) {
            this.p = p;
            this.boneIndex = boneIndex;
            this.weight = weight;
        }

        private int toCompInt() {
            // weight is [0.0, 1.0]
            return (int)(Integer.MAX_VALUE * this.weight);
        }

        @Override
        public int compareTo(Weight o) {
            return o.toCompInt() - toCompInt();
        }
    }
}
