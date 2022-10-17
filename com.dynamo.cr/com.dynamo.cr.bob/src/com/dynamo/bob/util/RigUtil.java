// Copyright 2020-2022 The Defold Foundation
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
        void duplicateLast();
        T toComposite(Key key);
        T interpolate(double t, T a, T b);
    }

    public static abstract class AbstractPropertyBuilder<T> implements PropertyBuilder<T, RigUtil.AnimationKey> {
        protected com.dynamo.rig.proto.Rig.AnimationTrack.Builder builder;

        public AbstractPropertyBuilder(com.dynamo.rig.proto.Rig.AnimationTrack.Builder builder) {
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
        public void duplicateLast() {
            int length = builder.getPositionsCount();
            builder.addPositions(builder.getPositions(length-3));
            builder.addPositions(builder.getPositions(length-2));
            builder.addPositions(builder.getPositions(length-1));
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
        public void duplicateLast() {
            int length = builder.getRotationsCount();
            builder.addRotations(builder.getRotations(length-4));
            builder.addRotations(builder.getRotations(length-3));
            builder.addRotations(builder.getRotations(length-2));
            builder.addRotations(builder.getRotations(length-1));
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
        public void duplicateLast() {
            int length = builder.getRotationsCount();
            builder.addRotations(builder.getRotations(length-4));
            builder.addRotations(builder.getRotations(length-3));
            builder.addRotations(builder.getRotations(length-2));
            builder.addRotations(builder.getRotations(length-1));
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
        public void duplicateLast() {
            int length = builder.getScaleCount();
            builder.addScale(builder.getScale(length-3));
            builder.addScale(builder.getScale(length-2));
            builder.addScale(builder.getScale(length-1));
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

    public static <T,Key extends RigUtil.AnimationKey> void sampleTrack(RigUtil.AbstractAnimationTrack<Key> track, RigUtil.PropertyBuilder<T, Key> propertyBuilder, double startTime, double duration, double sampleRate, double spf, boolean interpolate) {
        if (track.keys.isEmpty()) {
            return;
        }

        if (track.keys.size() == 1) {
            T value = propertyBuilder.toComposite(track.keys.get(0));
            propertyBuilder.addComposite(value);
            return;
        }

        // We add one extra frame (+1) to have a keyframe when t == duration, we also need
        // to duplicate the last keyframe (see end of function) so that the linear
        // interpolation works correctly in runtime.
        int sampleCount = (int)Math.ceil(duration * sampleRate) + 1;
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
                // No valid key yet, use first value
                propertyBuilder.addComposite(propertyBuilder.toComposite(next));
            }
        }

        // Create duplicate of last keyframe
        propertyBuilder.duplicateLast();
    }
}
