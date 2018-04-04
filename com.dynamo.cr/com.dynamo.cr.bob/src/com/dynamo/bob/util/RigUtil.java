package com.dynamo.bob.util;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;

import javax.vecmath.AxisAngle4d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

import com.dynamo.bob.textureset.TextureSetGenerator.UVTransform;
import com.dynamo.bob.util.RigUtil.AnimationCurve.CurveIntepolation;
import com.dynamo.rig.proto.Rig.MeshAnimationTrack;

/**
 * Convenience class for loading spine json data.
 *
 * Should preferably have been an extension to Bob rather than located inside it.
 */
public class RigUtil {
    static double EPSILON = 0.0001;
    
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

    public static class MeshAttachment {
        public String path;
        public int index;
        // format is: x0, y0, z0, u0, v0, ...
        public float[] vertices;
        public int[] triangles;
        public float[] boneWeights;
        public int[] boneIndices;

        public float[] color = new float[] {1.0f, 1.0f, 1.0f, 1.0f};
    }

    public static class BaseSlot {
        public String name;
        public int index;
        public Bone bone;
        public String defaultAttachmentString;
        public float[] color = new float[] {1.0f, 1.0f, 1.0f, 1.0f};

        public List<String> attachments = new ArrayList<>();
        public Map<String, Integer> attachmentsLut = new HashMap<>();
        public int activeAttachment = -1;

        public BaseSlot() {
            name = null;
            index = -1;
            bone = null;
            defaultAttachmentString = null;
        }
        public BaseSlot(String name, int index, Bone bone, String defaultAttachmentString) {
            this.name = name;
            this.index = index;
            this.bone = bone;
            this.defaultAttachmentString = defaultAttachmentString;

            if (defaultAttachmentString != null && !defaultAttachmentString.isEmpty())
            {
                this.attachments.add(defaultAttachmentString);
                this.attachmentsLut.put(defaultAttachmentString, 0);
            }
        }
    }

    public static class SkinSlot {
        public BaseSlot baseSlot;
        public int activeAttachment = -1;
        public List<Integer> meshAttachments = new ArrayList<>();
        public float[] color = new float[] {1.0f, 1.0f, 1.0f, 1.0f};

        public SkinSlot(BaseSlot baseSlot) {
            this.baseSlot = baseSlot;
            this.activeAttachment = baseSlot.activeAttachment;
            this.color = baseSlot.color;
            for (int i = 0; i < baseSlot.attachments.size(); i++) {
                this.meshAttachments.add(-1);
            }
        }

        public SkinSlot(SkinSlot copy) {
            this.baseSlot = copy.baseSlot;
            this.activeAttachment = copy.activeAttachment;
            this.color = copy.color;
            for (int i = 0; i < copy.meshAttachments.size(); i++) {
                this.meshAttachments.add(copy.meshAttachments.get(i));
            }
        }
    }

    public static class AnimationCurve {
        public enum CurveIntepolation {
            BEZIER, LINEAR
        }
        public float x0;
        public float y0;
        public float x1;
        public float y1;
        public CurveIntepolation interpolation;

        public AnimationCurve() { interpolation = CurveIntepolation.BEZIER; }

    }

    public static class AnimationKey {
        public double t;
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
        public int attachment;
        public int orderOffset;
    }

    public static class SlotAnimationTrack extends AbstractAnimationTrack<SlotAnimationKey> {
        public enum Property {
            ATTACHMENT, COLOR, DRAW_ORDER
        }
        public int slot;
        public Property property;
    }

    public static class Event {
        public String name;
        public String stringPayload;
        public float floatPayload;
        public int intPayload;
    }

    public static class EventKey {
        public double t;
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
        public double duration;
        public List<AnimationTrack> tracks = new ArrayList<AnimationTrack>();
        public List<IKAnimationTrack> iKTracks = new ArrayList<IKAnimationTrack>();
        public List<SlotAnimationTrack> slotTracks = new ArrayList<SlotAnimationTrack>();
        public List<EventTrack> eventTracks = new ArrayList<EventTrack>();
    }

    public static class Weight implements Comparable<Weight> {
        public Point3d p;
        public int boneIndex;
        public float weight;

        public Weight(int boneIndex, float weight) {
            this.p = new Point3d(0.0, 0.0, 0.0);
            this.boneIndex = boneIndex;
            this.weight = weight;
        }

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


    public interface PropertyBuilder<T, Key extends RigUtil.AnimationKey> {
        void addComposite(T v);
        void add(double v);
        T toComposite(Key key);
        T interpolate(double t, T a, T b);
    }

    public static abstract class AbstractPropertyBuilder<T> implements PropertyBuilder<T, RigUtil.AnimationKey> {
        protected com.dynamo.rig.proto.Rig.AnimationTrack.Builder builder;

        public AbstractPropertyBuilder(com.dynamo.rig.proto.Rig.AnimationTrack.Builder builder) {
            this.builder = builder;
        }
    }

    public static abstract class AbstractIKPropertyBuilder<T> implements PropertyBuilder<T, RigUtil.IKAnimationKey> {
        protected com.dynamo.rig.proto.Rig.IKAnimationTrack.Builder builder;

        public AbstractIKPropertyBuilder(com.dynamo.rig.proto.Rig.IKAnimationTrack.Builder builder) {
            this.builder = builder;
        }
    }

    public static abstract class AbstractMeshPropertyBuilder<T> implements PropertyBuilder<T, RigUtil.SlotAnimationKey> {
        protected MeshAnimationTrack.Builder builder;

        public AbstractMeshPropertyBuilder(MeshAnimationTrack.Builder builder) {
            this.builder = builder;
        }
    }

    public static class PositionBuilder extends AbstractPropertyBuilder<Point3d> {
        public PositionBuilder(com.dynamo.rig.proto.Rig.AnimationTrack.Builder builder) {
            super(builder);
        }

        @Override
        public void addComposite(Point3d v) {
            builder.addPositions((float)v.x).addPositions((float)v.y).addPositions((float)v.z);
        }

        @Override
        public void add(double v) {
            builder.addPositions((float)v);
        }

        @Override
        public Point3d toComposite(RigUtil.AnimationKey key) {
            float[] v = key.value;
            if (v.length == 3) {
                return new Point3d(v[0], v[1], v[2]);
            } else {
                return new Point3d(v[0], v[1], 0.0);
            }

        }

        @Override
        public Point3d interpolate(double t, Point3d a, Point3d b)
        {
            Point3d out = new Point3d(a);
            out.interpolate(b, t);
            return out;
        }
    }

    public static Quat4d toQuat(double angle) {
        double halfRad = 0.5 * angle * Math.PI / 180.0;
        double c = Math.cos(halfRad);
        double s = Math.sin(halfRad);
        return new Quat4d(0.0, 0.0, s, c);
    }

    public static class RotationBuilder extends AbstractPropertyBuilder<Quat4d> {
        public RotationBuilder(com.dynamo.rig.proto.Rig.AnimationTrack.Builder builder) {
            super(builder);
        }

        @Override
        public void addComposite(Quat4d v) {
            this.builder.addRotations((float)v.x).addRotations((float)v.y).addRotations((float)v.z).addRotations((float)v.w);
        }

        @Override
        public void add(double v) {
            addComposite(toQuat(v));
        }

        @Override
        public Quat4d toComposite(RigUtil.AnimationKey key) {
            float[] v = key.value;
            return toQuat(v[0]);
        }

        @Override
        public Quat4d interpolate(double t, Quat4d a, Quat4d b)
        {
            Quat4d out = new Quat4d();
            out.interpolate(a, b, t);
            return out;
        }
    }

    public static class QuatRotationBuilder extends AbstractPropertyBuilder<Quat4d> {
        public QuatRotationBuilder(com.dynamo.rig.proto.Rig.AnimationTrack.Builder builder) {
            super(builder);
        }

        @Override
        public void addComposite(Quat4d v) {
            this.builder.addRotations((float)v.x).addRotations((float)v.y).addRotations((float)v.z).addRotations((float)v.w);
        }

        @Override
        public void add(double v) {
            this.builder.addRotations((float)v);
        }

        @Override
        public Quat4d toComposite(RigUtil.AnimationKey key) {
            float[] v = key.value;
            return new Quat4d(v[0], v[1], v[2], v[3]);
        }

        @Override
        public Quat4d interpolate(double t, Quat4d a, Quat4d b)
        {
            Quat4d out = new Quat4d();
            out.interpolate(a, b, t);
            return out;
        }
    }

    public static class ScaleBuilder extends AbstractPropertyBuilder<Vector3d> {
        public ScaleBuilder(com.dynamo.rig.proto.Rig.AnimationTrack.Builder builder) {
            super(builder);
        }

        @Override
        public void addComposite(Vector3d v) {
            builder.addScale((float)v.x).addScale((float)v.y).addScale((float)v.z);
        }

        @Override
        public void add(double v) {
            builder.addScale((float)v);
        }

        @Override
        public Vector3d toComposite(RigUtil.AnimationKey key) {
            float[] v = key.value;
            return new Vector3d(v[0], v[1], v[2]);
        }

        @Override
        public Vector3d interpolate(double t, Vector3d a, Vector3d b)
        {
            Vector3d out = new Vector3d(a);
            out.interpolate(b, t);
            return out;
        }
    }

    public static class IKMixBuilder extends AbstractIKPropertyBuilder<Float> {
        public IKMixBuilder(com.dynamo.rig.proto.Rig.IKAnimationTrack.Builder builder) {
            super(builder);
        }

        @Override
        public void addComposite(Float value) {
            builder.addMix(value);
        }

        @Override
        public void add(double v) {
            builder.addMix((float)v);
        }

        @Override
        public Float toComposite(RigUtil.IKAnimationKey key) {
            return new Float(key.mix);
        }

        @Override
        public Float interpolate(double t, Float a, Float b)
        {
            return ((1.0f - (float)t) * a + (float)t * b);
        }
    }

    public static class IKPositiveBuilder extends AbstractIKPropertyBuilder<Boolean> {
        public IKPositiveBuilder(com.dynamo.rig.proto.Rig.IKAnimationTrack.Builder builder) {
            super(builder);
        }

        @Override
        public void addComposite(Boolean value) {
            builder.addPositive(value);
        }

        @Override
        public void add(double v) {
            throw new RuntimeException("Not supported");
        }

        @Override
        public Boolean toComposite(RigUtil.IKAnimationKey key) {
            return new Boolean(key.positive);
        }

        @Override
        public Boolean interpolate(double t, Boolean a, Boolean b)
        {
            if (t <= 0.5) {
                return a;
            } else {
                return b;
            }
        }
    }

    public static class ColorBuilder extends AbstractMeshPropertyBuilder<float[]> {
        public ColorBuilder(MeshAnimationTrack.Builder builder) {
            super(builder);
        }

        @Override
        public void addComposite(float[] c) {
            builder.addColors(c[0]).addColors(c[1]).addColors(c[2]).addColors(c[3]);
        }

        @Override
        public void add(double v) {
            builder.addColors((float)v);
        }

        @Override
        public float[] toComposite(RigUtil.SlotAnimationKey key) {
            return key.value;
        }

        @Override
        public float[] interpolate(double t, float[] a, float[] b)
        {
            float out[] = {0.0f, 0.0f, 0.0f, 0.0f};
            out[0] = ((1.0f - (float)t) * a[0] + (float)t * b[0]);
            out[1] = ((1.0f - (float)t) * a[1] + (float)t * b[1]);
            out[2] = ((1.0f - (float)t) * a[2] + (float)t * b[2]);
            out[3] = ((1.0f - (float)t) * a[3] + (float)t * b[3]);
            return out;
        }
    }

    public static class AttachmentBuilder extends AbstractMeshPropertyBuilder<Integer> {

        public AttachmentBuilder(MeshAnimationTrack.Builder builder) {
            super(builder);
        }

        @Override
        public void addComposite(Integer value) {
            builder.addMeshAttachment(value);
        }

        @Override
        public void add(double v) {
            throw new RuntimeException("Not supported");
        }

        @Override
        public Integer toComposite(RigUtil.SlotAnimationKey key) {
            return key.attachment;
        }

        @Override
        public Integer interpolate(double t, Integer a, Integer b)
        {
            if (t <= 0.5) {
                return a;
            } else {
                return b;
            }
        }
    }

    public static class DrawOrderBuilder extends AbstractMeshPropertyBuilder<Integer> {
        public DrawOrderBuilder(MeshAnimationTrack.Builder builder) {
            super(builder);
        }

        @Override
        public void addComposite(Integer value) {
            builder.addOrderOffset(value);
        }

        @Override
        public void add(double v) {
            throw new RuntimeException("Not supported");
        }

        @Override
        public Integer toComposite(RigUtil.SlotAnimationKey key) {
            return key.orderOffset;
        }

        @Override
        public Integer interpolate(double t, Integer a, Integer b)
        {
            if (t <= 0.5) {
                return a;
            } else {
                return b;
            }
        }
    }


    private static double evalCurve(RigUtil.AnimationCurve curve, double x) {
        if (curve == null) {
            return x;
        }
        double t = BezierUtil.findT(x, 0.0, curve.x0, curve.x1, 1.0);
        return BezierUtil.curve(t, 0.0, curve.y0, curve.y1, 1.0);
    }

    private static <T, Key extends RigUtil.AnimationKey> void sampleCurve(RigUtil.AnimationCurve curve, RigUtil.PropertyBuilder<T,Key> builder, double cursor, double t0, Key v0, double t1, Key v1) {
        double length = t1 - t0;
        double t = (cursor - t0) / length;
        if (curve != null && curve.interpolation == CurveIntepolation.BEZIER) {
            t = evalCurve(curve, t);
        }
        builder.addComposite(builder.interpolate(t, builder.toComposite(v0), builder.toComposite(v1)));
    }

    public static <T,Key extends RigUtil.AnimationKey> void sampleTrack(RigUtil.AbstractAnimationTrack<Key> track, RigUtil.PropertyBuilder<T, Key> propertyBuilder, T defaultValue, double startTime, double duration, double sampleRate, double spf, boolean interpolate) {
        if (track.keys.isEmpty()) {
            return;
        }
        // We add one extra frame at the end (+1) so that we always get a copy of a the last keyframe,
        // in turn we can safely interpolate at end of the animation in runtime when the cursor is the same as duration.
        // If the animation has a duration of zero (ie only one keyframe at time 0), we need to make sure
        // we always have 2 keyframes (ie a duplicate of the last one here as well).
        int sampleCount = Math.max(2, (int)Math.ceil(duration * sampleRate) + 1);
        double halfSample = spf / 2.0;
        int keyIndex = 0;
        int keyCount = track.keys.size();
        Key key = null;
        Key next = track.keys.get(keyIndex);
        T endValue = propertyBuilder.toComposite(track.keys.get(keyCount-1));
        int startI = (int)(startTime*sampleRate);
        for (int i = startI; i < startI+sampleCount; ++i) {
            double cursor = i * spf;
            // Skip passed keys. Also handles corner case where the cursor is sufficiently close to the very first key frame.
            while ((next != null && next.t <= cursor) || (key == null && Math.abs(next.t - cursor) < EPSILON)) {
                key = next;
                ++keyIndex;
                if (keyIndex < keyCount) {
                    next = track.keys.get(keyIndex);
                } else {
                    next = null;
                }
            }
            if (key != null) {
                if (next != null) {
                    if (key.stepped || !interpolate) {
                        // Calculate point where we should sample next instead of key
                        // Check if cursor is past keyChangePoint, in that case sample next instead
                        double keyChangePoint = next.t - halfSample;
                        if (cursor > keyChangePoint) {
                            propertyBuilder.addComposite(propertyBuilder.toComposite(next));
                        } else {
                            propertyBuilder.addComposite(propertyBuilder.toComposite(key));
                        }
                    } else {
                        // Normal sampling
                        sampleCurve(key.curve, propertyBuilder, cursor, key.t, key, next.t, next);
                    }
                } else {
                    // Last key reached, use its value for remaining samples
                    propertyBuilder.addComposite(endValue);
                }
            } else {
                // No valid key yet, use default value
                propertyBuilder.addComposite(defaultValue);
            }
        }
    }
}
