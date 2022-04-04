// Copyright 2020 The Defold Foundation
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

//https://assimp-docs.readthedocs.io/en/latest/usage/use_the_lib.html#ai-data

package com.dynamo.bob.pipeline;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.FileInputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Vector;
import java.util.Map.Entry;

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.lang3.ArrayUtils;

import javax.vecmath.Matrix4d;
import javax.vecmath.Matrix4f;
import javax.vecmath.Point3d;
import javax.vecmath.Point3f;
import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;
import javax.vecmath.Tuple4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector3f;
import javax.vecmath.Vector4d;

import com.dynamo.bob.util.MathUtil;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.RigUtil;
import com.dynamo.bob.util.RigUtil.AnimationKey;
import com.dynamo.bob.util.RigUtil.Weight;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.proto.DdfMath.Matrix4;
import com.dynamo.proto.DdfMath.Transform;

import com.dynamo.bob.pipeline.ModelImporter.Bone;
import com.dynamo.bob.pipeline.ModelImporter.Mesh;
import com.dynamo.bob.pipeline.ModelImporter.Model;
import com.dynamo.bob.pipeline.ModelImporter.Node;
import com.dynamo.bob.pipeline.ModelImporter.Options;
import com.dynamo.bob.pipeline.ModelImporter.Scene;


import com.dynamo.rig.proto.Rig;
import com.dynamo.rig.proto.Rig.AnimationInstanceDesc;
import com.dynamo.rig.proto.Rig.AnimationSetDesc;
import com.google.protobuf.ByteString;

public class ModelUtil {

    static private class AssetSpace
    {
        public Matrix4d rotation;
        public double unit;
        AssetSpace() {
            rotation = new Matrix4d();
            rotation.setIdentity();
            unit = 1.0;
        }
    }

    private static void printMatrix4d(String name, Matrix4d mat) {
        System.out.printf("Mat %s:\n", name);
        System.out.printf("    %f, %f, %f, %f\n", mat.m00,mat.m01,mat.m02,mat.m03);
        System.out.printf("    %f, %f, %f, %f\n", mat.m10,mat.m11,mat.m12,mat.m13);
        System.out.printf("    %f, %f, %f, %f\n", mat.m20,mat.m21,mat.m22,mat.m23);
        System.out.printf("    %f, %f, %f, %f\n", mat.m30,mat.m31,mat.m32,mat.m33);
        System.out.printf("\n");
    }

    public static AssetSpace getAssetSpace(Scene scene)
    {
        AssetSpace assetSpace = new AssetSpace();
        if (scene != null) {

            //UpAxis guessedUpAxis = null;
            {
                // AINode scene_root = scene.mRootNode();
                // AIMatrix4x4 mat = scene_root.mTransformation();

                //printMatrix4x4(scene_root.mName().dataString(), mat);

                // if (mat.a1() == 1.0 && mat.b3() == 1.0 && mat.c2() == -1.0) {
                // }

                // assetSpace.rotation.setRow(0, new double[] {mat.a1(), mat.a2(), mat.a3(), 0.0});
                // assetSpace.rotation.setRow(1, new double[] {mat.b1(), mat.b2(), mat.b3(), 0.0});
                // assetSpace.rotation.setRow(2, new double[] {mat.c1(), mat.c2(), mat.c3(), 0.0});
                //assetSpace.rotation.transpose();

                // Blender usually defaults to Z_UP
                // assetSpace.rotation.setRow(0, new double[] {1.0, 0.0, 0.0, 0.0});
                // assetSpace.rotation.setRow(1, new double[] {0.0, 0.0, 1.0, 0.0});
                // assetSpace.rotation.setRow(2, new double[] {0.0, -1.0, 0.0, 0.0});

                //printMatrix4d(scene_root.mName().dataString(), assetSpace.rotation);
            }


            // UpAxis upAxis = guessedUpAxis != null ? guessedUpAxis : UpAxis.Y_UP;
            // if (upAxis.equals(UpAxis.Z_UP)) {
            //     assetSpace.rotation.setRow(0, new double[] {1.0, 0.0, 0.0, 0.0});
            //     assetSpace.rotation.setRow(1, new double[] {0.0, 0.0, 1.0, 0.0});
            //     assetSpace.rotation.setRow(2, new double[] {0.0, -1.0, 0.0, 0.0});
            // } else if (upAxis.equals(UpAxis.X_UP)) {
            //     assetSpace.rotation.setRow(0, new double[] {0.0, -1.0, 0.0, 0.0});
            //     assetSpace.rotation.setRow(1, new double[] {1.0, 0.0, 0.0, 0.0});
            //     assetSpace.rotation.setRow(2, new double[] {0.0, 0.0, 1.0, 0.0});
            // } else {
            //     assetSpace.rotation.setRow(0, new double[] {1.0, 0.0, 0.0, 0.0});
            //     assetSpace.rotation.setRow(1, new double[] {0.0, 1.0, 0.0, 0.0});
            //     assetSpace.rotation.setRow(2, new double[] {0.0, 0.0, 1.0, 0.0});
            // }
        }

        return assetSpace;
    }

    public static Scene loadScene(byte[] content, String suffix, Options options) {
        ModelImporter.CheckSizes(); // Make sure the Java/C struct sizes are the same
        if (options == null)
            options = new Options();
        return ModelImporter.LoadFromBuffer(options, suffix, ByteBuffer.wrap(content), content.length);
    }

    public static Scene loadScene(InputStream stream, String suffix, Options options) throws IOException {
        byte[] bytes = IOUtils.toByteArray(stream);
        return loadScene(bytes, suffix, options);
    }

    public static void unloadScene(Scene scene) {
        ModelImporter.DestroyScene(scene);
    }

    private static AnimationKey createKey(float t, boolean stepped, int componentSize) {
        AnimationKey key = new AnimationKey();
        key.t = t;
        key.stepped = stepped;
        key.value = new float[componentSize];
        return key;
    }

    private static void toFloats(Tuple3d v, float[] f) {
        f[0] = (float)v.getX();
        f[1] = (float)v.getY();
        f[2] = (float)v.getZ();
    }

    private static void toFloats(Tuple4d v, float[] f) {
        f[0] = (float)v.getX();
        f[1] = (float)v.getY();
        f[2] = (float)v.getZ();
        f[3] = (float)v.getW();
    }

/*
    private static void ExtractMatrixKeys(AINodeAnim anim, double ticksPerSecond, Matrix4d localToParent, AssetSpace assetSpace, RigUtil.AnimationTrack posTrack, RigUtil.AnimationTrack rotTrack, RigUtil.AnimationTrack scaleTrack) {

        Vector3d bindP = new Vector3d();
        Quat4d bindR = new Quat4d();
        Vector3d bindS = new Vector3d();
        MathUtil.decompose(localToParent, bindP, bindR, bindS);
        bindR.inverse();

        Vector3d assetSpaceP = new Vector3d();
        Quat4d assetSpaceR = new Quat4d();
        Vector3d assetSpaceS = new Vector3d();
        MathUtil.decompose(assetSpace.rotation, assetSpaceP, assetSpaceR, assetSpaceS);
        assetSpaceR.inverse();

        //System.out.printf("  bindR %f, %f, %f, %f\n", bindR.x, bindR.y, bindR.z, bindR.w);

        //System.out.printf("  assetSpaceR %f, %f, %f, %f\n", assetSpaceR.x, assetSpaceR.y, assetSpaceR.z, assetSpaceR.w);

        // https://github.com/LWJGL/lwjgl3/blob/02e5523774b104866e502e706141ae1a468183e6/modules/lwjgl/assimp/src/generated/java/org/lwjgl/assimp/AINodeAnim.java
        // Highlights:
        // * The keys are absolute, and not relative the parent

        int num_position_keys = anim.mNumPositionKeys();
        int num_rotation_keys = anim.mNumRotationKeys();
        int num_scale_keys = anim.mNumScalingKeys();

        //System.out.printf("  BONE: %s nKeys: t %d, r %d, s %d\n", anim.mNodeName().dataString(), num_position_keys, num_rotation_keys, num_scale_keys);

        {
            int keyIndex = 0;
            AIVectorKey.Buffer buffer = anim.mPositionKeys();
            while (buffer.remaining() > 0) {
                AIVectorKey key = buffer.get();
                double time = key.mTime() / ticksPerSecond;
                AIVector3D v = key.mValue();

                Vector3d p = new Vector3d();
                // TODO: Use the assetspace scaling: assetSpace.unit
                p.set(v.x() - bindP.getX(), v.y() - bindP.getY(), v.z() - bindP.getZ());

                if (keyIndex == 0 && time > 0)
                {
                    // Always make sure we have a key at t == 0
                    AnimationKey first = createKey(0.0f, false, 3);
                    toFloats(p, first.value);
                    posTrack.keys.add(first);
                    keyIndex++;
                }

                AnimationKey posKey = createKey((float)time, false, 3);
                toFloats(p, posKey.value);
                posTrack.keys.add(posKey);
                keyIndex++;
            }
        }

        {
            Vector4d lastR = new Vector4d(0.0, 0.0, 0.0, 0.0);

            int keyIndex = 0;
            AIQuatKey.Buffer buffer = anim.mRotationKeys();
            while (buffer.remaining() > 0) {
                AIQuatKey key = buffer.get();
                double time = key.mTime() / ticksPerSecond;
                AIQuaternion v = key.mValue();

                Quat4d r = new Quat4d(v.x(), v.y(), v.z(), v.w());

                // Check if dot product of decomposed rotation and previous frame is < 0,
                // if that is the case; flip rotation.
                // This is to avoid a problem that can occur when we decompose the matrix and
                // we get a quaternion representing the same rotation but in the opposite direction.
                // See this answer on SO: http://stackoverflow.com/a/2887128
                Vector4d rv = new Vector4d(r.x, r.y, r.z, r.w);
                if (lastR.dot(rv) < 0.0) {
                    r.scale(-1.0);
                    rv.scale(-1.0);
                }
                lastR = rv;

                //r.mul(assetSpaceR, r);
                //r.mul(bindR, r);

                if (keyIndex == 0 && time > 0)
                {
                    // Always make sure we have a key at t == 0
                    AnimationKey first = createKey(0.0f, false, 4);
                    toFloats(r, first.value);
                    rotTrack.keys.add(first);
                    keyIndex++;

        //System.out.printf("  rotKey %d: t: %f   %f, %f, %f, %f   -> %f, %f, %f, %f\n", keyIndex, (float)first.t, (float)v.x(), (float)v.y(), (float)v.z(), (float)v.w(), (float)first.value[0], (float)first.value[1], (float)first.value[2], (float)first.value[3]);

                }

                AnimationKey rot = createKey((float)time, false, 4);
                toFloats(r, rot.value);
                rotTrack.keys.add(rot);
                keyIndex++;

        //System.out.printf("  rotKey %d: t: %f   %f, %f, %f, %f   -> %f, %f, %f, %f\n", keyIndex, (float)rot.t, (float)v.x(), (float)v.y(), (float)v.z(), (float)v.w(), (float)rot.value[0], (float)rot.value[1], (float)rot.value[2], (float)rot.value[3]);

        //System.out.printf("      bindR %d: %f, %f, %f, %f\n", keyIndex, bindR.x, bindR.y, bindR.z, bindR.w);

            }
        }

        {
            int keyIndex = 0;
            AIVectorKey.Buffer buffer = anim.mScalingKeys();
            while (buffer.remaining() > 0) {
                AIVectorKey key = buffer.get();
                double time = key.mTime() / ticksPerSecond;
                AIVector3D v = key.mValue();

                Vector3d s = new Vector3d();
                s.set(v.x() / bindS.getX(), v.y() / bindS.getY(), v.z() / bindS.getZ());

                if (keyIndex == 0 && time > 0)
                {
                    // Always make sure we have a key at t == 0
                    AnimationKey first = createKey((float)time, false, 3);
                    toFloats(s, first.value);
                    scaleTrack.keys.add(first);
                    keyIndex++;
                }

                AnimationKey scaleKey = createKey((float)time, false, 3);
                toFloats(s, scaleKey.value);
                scaleTrack.keys.add(scaleKey);
                keyIndex++;
            }
        }
    }
    */

    private static void samplePosTrack(Rig.RigAnimation.Builder animBuilder, RigUtil.AnimationTrack track, int boneIndex, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
        if (track.keys.isEmpty())
            return;

        Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
        animTrackBuilder.setBoneIndex(boneIndex);
        RigUtil.PositionBuilder positionBuilder = new RigUtil.PositionBuilder(animTrackBuilder);
        RigUtil.sampleTrack(track, positionBuilder, new Point3d(0.0, 0.0, 0.0), startTime, duration, sampleRate, spf, true);
        animBuilder.addTracks(animTrackBuilder.build());
    }

    private static void sampleRotTrack(Rig.RigAnimation.Builder animBuilder, RigUtil.AnimationTrack track, int boneIndex, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
        if (track.keys.isEmpty())
            return;

        Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
        animTrackBuilder.setBoneIndex(boneIndex);
        RigUtil.QuatRotationBuilder rotationBuilder = new RigUtil.QuatRotationBuilder(animTrackBuilder);

        //System.out.printf("sampleTrack rotations:  startTime %f\n", (float)startTime);
        RigUtil.sampleTrack(track, rotationBuilder, new Quat4d(0.0, 0.0, 0.0, 1.0), startTime, duration, sampleRate, spf, true);

        // Rig.AnimationTrack track2 = animTrackBuilder.build();
        // int num_rotations = track2.getRotationsCount() / 4;
        // System.out.printf("rotations count: %d\n", num_rotations);
        // for (int i = 0; i < num_rotations; ++i) {

        //     System.out.printf(" track rot: %d  %f, %f, %f, %f\n", i, track2.getRotations(i*4+0), track2.getRotations(i*4+1), track2.getRotations(i*4+2), track2.getRotations(i*4+3));
        // }

        animBuilder.addTracks(animTrackBuilder.build());
    }

    private static void sampleScaleTrack(Rig.RigAnimation.Builder animBuilder, RigUtil.AnimationTrack track, int boneIndex, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
        if (track.keys.isEmpty())
            return;

        Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
        animTrackBuilder.setBoneIndex(boneIndex);
        RigUtil.ScaleBuilder scaleBuilder = new RigUtil.ScaleBuilder(animTrackBuilder);
        RigUtil.sampleTrack(track, scaleBuilder, new Vector3d(1.0, 1.0, 1.0), startTime, duration, sampleRate, spf, interpolate);
        animBuilder.addTracks(animTrackBuilder.build());
    }

    private static void copyKeys(ModelImporter.KeyFrame keys[], int componentSize, List<RigUtil.AnimationKey> outKeys) {
        for (ModelImporter.KeyFrame key : keys) {
            RigUtil.AnimationKey outKey = createKey(key.time, false, componentSize);

            for (int i = 0; i < componentSize; ++i)
            {
                outKey.value[i] = key.value[i];
            }
            outKeys.add(outKey);
        }
    }


    public static void loadAnimationTracks(Rig.RigAnimation.Builder animBuilder, ModelImporter.NodeAnimation nodeAnimation, ModelImporter.Bone bone, double duration, double startTime, double sampleRate) {
        double spf = 1.0 / sampleRate;

        if (nodeAnimation.translationKeysCount > 0) {
            RigUtil.AnimationTrack sparseTrack = new RigUtil.AnimationTrack();
            sparseTrack.property = RigUtil.AnimationTrack.Property.POSITION;
            copyKeys(nodeAnimation.getTranslationKeys(), 3, sparseTrack.keys);

            samplePosTrack(animBuilder, sparseTrack, bone.index, duration, startTime, sampleRate, spf, true);
        }

        if (nodeAnimation.rotationKeysCount > 0) {
            RigUtil.AnimationTrack sparseTrack = new RigUtil.AnimationTrack();
            sparseTrack.property = RigUtil.AnimationTrack.Property.ROTATION;
            copyKeys(nodeAnimation.getRotationKeys(), 4, sparseTrack.keys);

            sampleRotTrack(animBuilder, sparseTrack, bone.index, duration, startTime, sampleRate, spf, true);
        }

        if (nodeAnimation.scaleKeysCount > 0) {
            RigUtil.AnimationTrack sparseTrack = new RigUtil.AnimationTrack();
            sparseTrack.property = RigUtil.AnimationTrack.Property.SCALE;
            copyKeys(nodeAnimation.getTranslationKeys(), 3, sparseTrack.keys);

            sampleScaleTrack(animBuilder, sparseTrack, bone.index, duration, startTime, sampleRate, spf, true);
        }
    }


    // private static double getAnimationStartTime(AIAnimation anim) {
    //     double startTime = 10000.0;

    //     double ticksPerSecond = anim.mTicksPerSecond();
    //     PointerBuffer aiChannels = anim.mChannels();
    //     int num_channels = anim.mNumChannels();
    //     for (int c = 0; c < num_channels; ++c) {
    //         AINodeAnim nodeAnim = AINodeAnim.create(aiChannels.get(c));
    //         {
    //             AIVectorKey.Buffer buffer = nodeAnim.mPositionKeys();
    //             while (buffer.remaining() > 0) {
    //                 AIVectorKey key = buffer.get();
    //                 double time = key.mTime() / ticksPerSecond;
    //                 if (time < startTime) {
    //                     startTime = time;
    //                 }
    //                 break;
    //             }
    //         }
    //         {
    //             AIQuatKey.Buffer buffer = nodeAnim.mRotationKeys();
    //             while (buffer.remaining() > 0) {
    //                 AIQuatKey key = buffer.get();
    //                 double time = key.mTime() / ticksPerSecond;
    //                 if (time < startTime) {
    //                     startTime = time;
    //                 }
    //                 break;
    //             }
    //         }
    //         {
    //             AIVectorKey.Buffer buffer = nodeAnim.mScalingKeys();
    //             while (buffer.remaining() > 0) {
    //                 AIVectorKey key = buffer.get();
    //                 double time = key.mTime() / ticksPerSecond;
    //                 if (time < startTime) {
    //                     startTime = time;
    //                 }
    //                 break;
    //             }
    //         }
    //     }
    //     if (startTime == 10000.0) {
    //         startTime = 0.0;
    //     }
    //     return startTime;
    // }

    // private static ModelImporter.Bone findBone(ArrayList<ModelImporter.Bone> bones, String name) {
    //     for (ModelImporter.Bone bone : bones) {
    //         if (bone.name.equals(name)) {
    //             return bone;
    //         }
    //     }
    //     return null;
    // }

    public static void setBoneList(Rig.AnimationSet.Builder animationSetBuilder, ArrayList<Bone> bones) {
        for (Bone bone : bones) {
            animationSetBuilder.addBoneList(MurmurHash.hash64(bone.name));
        }
    }

    public static void loadAnimations(byte[] content, String suffix, ModelImporter.Options options, ArrayList<ModelImporter.Bone> bones, Rig.AnimationSet.Builder animationSetBuilder, String parentAnimationId, ArrayList<String> animationIds) {
        Scene scene = loadScene(content, suffix, options);
        loadAnimations(scene, bones, animationSetBuilder, parentAnimationId, animationIds);
    }

    private static Bone findBone(ArrayList<ModelImporter.Bone> bones, ModelImporter.Node node) {
        for (ModelImporter.Bone bone : bones) {
            if (bone.node.name.equals(node.name)) {
                return bone;
            }
        }
        return null;
    }

    public static void loadAnimations(Scene scene, ArrayList<ModelImporter.Bone> bones, Rig.AnimationSet.Builder animationSetBuilder, String parentAnimationId, ArrayList<String> animationIds) {
        for (ModelImporter.Animation animation : scene.getAnimations()) {

            Rig.RigAnimation.Builder animBuilder = Rig.RigAnimation.newBuilder();

            float sampleRate = 30.0f; // TODO: Retrieve this properly from either asset file or Options
            float startTime = 1000000.0f;
            float endTime = 0.0f;

            animBuilder.setSampleRate(sampleRate);

            // Each file is supposed to only have one animation
            // And the animation name is created from outside of this function, depending on the source
            // E.g. if the animation is from within nested .animationset files
            // So we _dont_ do this:
            //   animationName = animation.name;
            // but instead do this:
            String animationName = parentAnimationId;
            animBuilder.setId(MurmurHash.hash64(animationName));
            animationIds.add(animationName);

            // TODO: add the duration, and start time to the Animation struct!
            for (ModelImporter.NodeAnimation nodeAnimation : animation.getNodeAnimations()) {

                int count = 0;
                ModelImporter.KeyFrame keys[] = null;

                if (nodeAnimation.translationKeysCount > 0) {
                    keys = nodeAnimation.getTranslationKeys();
                    count = nodeAnimation.translationKeysCount;
                }

                if (nodeAnimation.rotationKeysCount > 0) {
                    keys = nodeAnimation.getRotationKeys();
                    count = nodeAnimation.rotationKeysCount;
                }

                if (nodeAnimation.scaleKeysCount > 0) {
                    keys = nodeAnimation.getScaleKeys();
                    count = nodeAnimation.scaleKeysCount;
                }

                if (keys == null) {
                    System.err.printf("Animation contains no keys!");
                    return;
                }

                ModelImporter.KeyFrame firstKey = keys[0];
                ModelImporter.KeyFrame lastKey = keys[count-1];
                startTime = firstKey.time;
                endTime = lastKey.time;
            }

            float duration = endTime - startTime;
            animBuilder.setDuration(duration);

    System.out.printf("ANIMATION: %s  dur: %f  sampleRate: %f  startTime: %f\n", animation.name, duration, sampleRate, startTime);

            for (ModelImporter.NodeAnimation nodeAnimation : animation.getNodeAnimations()) {

                Bone bone = findBone(bones, nodeAnimation.node);
                assert(bone != null);

                loadAnimationTracks(animBuilder, nodeAnimation, bone, duration, startTime, sampleRate);
            }

            animationSetBuilder.addAnimations(animBuilder.build());
        }
    }

    // public static void loadAnimations(AIScene scene, ArrayList<ModelImporter.Bone> bones, Rig.AnimationSet.Builder animationSetBuilder, String parentAnimationId, ArrayList<String> animationIds) {
    //     AssetSpace assetSpace = getAssetSpace(scene);
    //     assert(bones != null);

    //     //System.out.printf("loadAnimations skeleton: size: %d\n", bones.size());

    //     int num_animations = scene.mNumAnimations();

    //     //System.out.printf("ModelUtil: loadAnimations: %d\n", num_animations);

    //     PointerBuffer aianimations = scene.mAnimations();
    //     for (int i = 0; i < num_animations; ++i) {
    //         AIAnimation aianimation = AIAnimation.create(aianimations.get(i));
    //         String animation_name = aianimation.mName().dataString();

    //         double duration = aianimation.mDuration();
    //         double ticksPerSecond = aianimation.mTicksPerSecond();
    //         double durationTime = duration/ticksPerSecond;
    //         double sampleRate = 30.0;

    //         int num_channels = aianimation.mNumChannels();

    //         Rig.RigAnimation.Builder animBuilder = Rig.RigAnimation.newBuilder();
    //         animBuilder.setDuration((float)durationTime);

    //         // We use the supplied framerate (if available) as samplerate to get correct timings when
    //         // sampling the animation data. We used to have a static sample rate of 30, which would mean
    //         // if the scene was saved with a different framerate the animation would either be too fast or too slow.
    //         animBuilder.setSampleRate((float)sampleRate);

    //         double startTime = 0.0;//getAnimationStartTime(aianimation);

    //         //System.out.printf("ANIMATION: %s  dur: %f  sampleRate: %f  startTime: %f  mNumChannels: %d\n", animation_name, (float)durationTime, (float)sampleRate, (float)startTime, num_channels);

    //         PointerBuffer aiChannels = aianimation.mChannels();
    //         for (int c = 0; c < num_channels; ++c) {
    //             AINodeAnim aiNodeAnim = AINodeAnim.create(aiChannels.get(c));
    //             String node_name = aiNodeAnim.mNodeName().dataString();

    //             ModelImporter.Bone bone = findBone(bones, node_name);
    //             AssetSpace curAssetSpace = assetSpace;
    //             if (bone.index != 0)
    //             {
    //                 // Only apply up axis rotation for first bone.
    //                 curAssetSpace.rotation.setIdentity();
    //             }

    //             Matrix4d localToParent = bone.transform;
    //             loadAnimationTracks(animBuilder, curAssetSpace, aiNodeAnim, localToParent, bone.index, durationTime, startTime, sampleRate, ticksPerSecond);
    //         }

    //         // The file name is usually based on the file name itself
    //         animBuilder.setId(MurmurHash.hash64(parentAnimationId));
    //         animationIds.add(parentAnimationId);
    //         animationSetBuilder.addAnimations(animBuilder.build());
    //     }
    // }

    // private static ArrayList<Float> getBufferData(AIVector3D.Buffer buffer, int numComponents) {
    //     if (buffer == null)
    //         return null;
    //     ArrayList<Float> out = new ArrayList<>();
    //     while (buffer.remaining() > 0) {
    //         AIVector3D v = buffer.get();
    //         out.add(v.x());
    //         if (numComponents >= 2)
    //             out.add(v.y());
    //         if (numComponents >= 3)
    //             out.add(v.z());
    //     }
    //     return out;
    // }

    // public static void loadWeights(AIMesh mesh, ArrayList<ModelImporter.Bone> skeleton, Rig.Mesh.Builder meshBuilder) {
    //     int num_vertices = mesh.mNumVertices();

    //     int num_bones = mesh.mNumBones();
    //     if (num_bones == 0)
    //         return;

    //     ArrayList<Float>[] bone_weights = new ArrayList[num_vertices];
    //     ArrayList<Integer>[] bone_indices = new ArrayList[num_vertices];
    //     for (int i = 0; i < num_vertices; ++i) {
    //         bone_weights[i] = new ArrayList<Float>();
    //         bone_indices[i] = new ArrayList<Integer>();
    //     }

    //     PointerBuffer bones = mesh.mBones();
    //     for (int i = 0; i < num_bones; ++i) {
    //         AIBone bone = AIBone.create(bones.get(i));

    //         // find the index of this bone in the skeleton list
    //         int bone_index = 0;
    //         for (ModelImporter.Bone skeleton_bone : skeleton) {
    //             if (skeleton_bone.name.equals(bone.mName().dataString())) {
    //                 break;
    //             }
    //             bone_index++;
    //         }

    //         AIVertexWeight.Buffer aiWeights = bone.mWeights();
    //         while (aiWeights.remaining() > 0) {
    //             AIVertexWeight bone_weight = aiWeights.get();
    //             int vertex_index = bone_weight.mVertexId();
    //             float vertex_weight = bone_weight.mWeight();

    //             bone_weights[vertex_index].add(vertex_weight);
    //             bone_indices[vertex_index].add(bone_index);
    //         }
    //     }

    //     // Pad each list to the desired length (currently 4 weights per vertex)
    //     for (int i = 0; i < num_vertices; ++i) {
    //         ArrayList<Float> weights = bone_weights[i];
    //         while (weights.size() < 4) {
    //             weights.add(0.0f);
    //             bone_indices[i].add(0);
    //         }
    //     }

    //     // flatten the lists
    //     ArrayList<Float> flattened_weights = new ArrayList<>();
    //     ArrayList<Integer> flattened_indices = new ArrayList<>();
    //     for (int i = 0; i < num_vertices; ++i) {
    //         flattened_weights.addAll(bone_weights[i]);
    //         flattened_indices.addAll(bone_indices[i]);

    //         // System.out.printf("%4d: %f, %f, %f, %f = %f   %d, %d, %d, %d\n", i, bone_weights[i].get(0), bone_weights[i].get(1), bone_weights[i].get(2), bone_weights[i].get(3),
    //         //     bone_weights[i].get(0) + bone_weights[i].get(1) + bone_weights[i].get(2) + bone_weights[i].get(3),
    //         //         bone_indices[i].get(0), bone_indices[i].get(1), bone_indices[i].get(2), bone_indices[i].get(3));
    //     }
    //     meshBuilder.addAllWeights(flattened_weights);
    //     meshBuilder.addAllBoneIndices(flattened_indices);
    // }

    public static ArrayList<String> loadMaterialNames(Scene scene) {
        ArrayList<String> materials = new ArrayList<>();

        for (Model model : scene.getModels()) {
            for (Mesh mesh : model.getMeshes()) {
                materials.add(mesh.material);
            }
        }
        return materials;
    }

    private static void createMeshIndices(Rig.Mesh.Builder meshBuilder,
                                        int triangle_count,
                                        boolean optimize,
                                        List<Integer> position_indices_list,
                                        List<Integer> normal_indices_list,
                                        List<Integer> texcoord0_indices_list) {
        class MeshVertexIndex {
            public int position, texcoord0, normal;
            public boolean equals(Object o) {
                MeshVertexIndex m = (MeshVertexIndex) o;
                return (this.position == m.position && this.texcoord0 == m.texcoord0 && this.normal == m.normal);
            }
        }

        // Build an optimized list of triangles from indices and instance (make unique) any vertices common attributes (position, normal etc.).
        // We can then use this to quickly build an optimized indexed vertex buffer of any selected vertex elements in run-time without any sorting.
        boolean mesh_has_normals = normal_indices_list.size() > 0;
        List<MeshVertexIndex> shared_vertex_indices = new ArrayList<MeshVertexIndex>(triangle_count*3);
        List<Integer> mesh_index_list = new ArrayList<Integer>(triangle_count*3);
        for (int i = 0; i < triangle_count*3; ++i) {
            MeshVertexIndex ci = new MeshVertexIndex();
            ci.position = position_indices_list.get(i);
            ci.texcoord0 = texcoord0_indices_list.get(i);
            ci.normal = mesh_has_normals ? normal_indices_list.get(i) : 0;
            int index = optimize ? shared_vertex_indices.indexOf(ci) : -1;
            if(index == -1) {
                // create new vertex as this is not equal to any existing in generated list
                mesh_index_list.add(shared_vertex_indices.size());
                shared_vertex_indices.add(ci);
            } else {
                // shared vertex, add index to existing vertex in generating list instead of adding new
                mesh_index_list.add(index);
            }
        }

        // We use this list to recreate a vertex buffer, in those cases that the index buffer is 32-bit,
        // but the platform doesn't support 32-bit index buffers. See res_model.cpp
        List<Rig.MeshVertexIndices> mesh_vertex_indices = new ArrayList<Rig.MeshVertexIndices>(triangle_count*3);
        for (int i = 0; i < shared_vertex_indices.size() ; ++i) {
            Rig.MeshVertexIndices.Builder b = Rig.MeshVertexIndices.newBuilder();
            MeshVertexIndex ci = shared_vertex_indices.get(i);
            b.setPosition(ci.position);
            b.setTexcoord0(ci.texcoord0);
            b.setNormal(ci.normal);
            mesh_vertex_indices.add(b.build());
        }

        Rig.IndexBufferFormat indices_format;
        ByteBuffer indices_bytes;
        if(shared_vertex_indices.size() <= 65536)
        {
            // if we only need 16-bit indices, use this primarily. Less data to upload to GPU and ES2.0 core functionality.
            indices_format = Rig.IndexBufferFormat.INDEXBUFFER_FORMAT_16;
            indices_bytes = ByteBuffer.allocateDirect(mesh_index_list.size() * 2);
            indices_bytes.order(ByteOrder.LITTLE_ENDIAN).asShortBuffer();
            for (int i = 0; i < mesh_index_list.size();) {
                indices_bytes.putShort(mesh_index_list.get(i++).shortValue());
            }
        }
        else
        {
            indices_format = Rig.IndexBufferFormat.INDEXBUFFER_FORMAT_32;
            indices_bytes = ByteBuffer.allocateDirect(mesh_index_list.size() * 4);
            indices_bytes.order(ByteOrder.LITTLE_ENDIAN).asIntBuffer();
            for (int i = 0; i < mesh_index_list.size();) {
                indices_bytes.putInt(mesh_index_list.get(i++));
            }
        }
        indices_bytes.rewind();

        meshBuilder.addAllVertices(mesh_vertex_indices);
        meshBuilder.setIndices(ByteString.copyFrom(indices_bytes));
        meshBuilder.setIndicesFormat(indices_format);

        //System.out.printf("opt: mesh_vertex_indices: %d\n", mesh_vertex_indices.size());
        //System.out.printf("opt: num_indices:         %d\n", mesh_index_list.size());
    }

    public static List<Integer> toList(int[] array) {
        return Arrays.asList(ArrayUtils.toObject(array));
    }

    public static List<Float> toList(float[] array) {
        return Arrays.asList(ArrayUtils.toObject(array));
    }

    public static Rig.Mesh loadMesh(Mesh mesh, ArrayList<ModelImporter.Bone> skeleton, ArrayList<String> materials) {

        String name = mesh.name;
// if (name.equals("Cube-mesh"))
// {
// System.out.printf("Mesh positions: %s\n", mesh.mName().dataString());
//             for (int i = 0; i < positions.size()/3; ++i)
//             {
// System.out.printf("  position %d: %f %f %f \n", i, positions.get(i*3+0), positions.get(i*3+1), positions.get(i*3+2));
//             }
// }

        Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();

        float[] positions = mesh.getPositions();
        float[] normals = mesh.getNormals();
        float[] tangents = mesh.getTangents();
        float[] colors = mesh.getColors();
        float[] weights = mesh.getWeights();
        int[] bones = mesh.getBones();
        float[] texCoords0 = mesh.getTexCoords(0);
        float[] texCoords1 = mesh.getTexCoords(1);

        // new ArrayList<Float> (Arrays.asList(value));

        if (positions != null)
            meshBuilder.addAllPositions(toList(positions));

        if (normals != null)
            meshBuilder.addAllNormals(toList(normals));

        if (tangents != null)
            meshBuilder.addAllTangents(toList(tangents));

        if (texCoords0 != null) {
            meshBuilder.addAllTexcoord0(toList(texCoords0));
            meshBuilder.setNumTexcoord0Components(mesh.texCoords0NumComponents);
        }
        if (texCoords1 != null) {
            meshBuilder.addAllTexcoord1(toList(texCoords1));
            meshBuilder.setNumTexcoord0Components(mesh.texCoords1NumComponents);
        }

        int[] indices = mesh.getIndices();
        int num_indices = mesh.indexCount;

        List<Integer> position_indices_list = new ArrayList<Integer>(num_indices);
        List<Integer> normal_indices_list = new ArrayList<Integer>(num_indices);
        List<Integer> texcoord0_indices_list = new ArrayList<Integer>(num_indices);

        for (int i = 0; i < num_indices; ++i) {
            int vertex_index = indices[i];

            position_indices_list.add(vertex_index);

            if (normals != null) {
                normal_indices_list.add(vertex_index);
            }

            if (texCoords0 != null) {
                texcoord0_indices_list.add(vertex_index);
            } else {
                texcoord0_indices_list.add(0);
            }
        }

        if(normals != null) {
            meshBuilder.addAllNormalsIndices(normal_indices_list);
        }
        meshBuilder.addAllPositionIndices(position_indices_list);
        meshBuilder.addAllTexcoord0Indices(texcoord0_indices_list);

        int triangle_count = num_indices / 3;

        createMeshIndices(meshBuilder, triangle_count, true, position_indices_list, normal_indices_list, texcoord0_indices_list);

        int material_index = 0;
        for (int i = 0; i < materials.size(); ++i) {
            String material = materials.get(i);
            if (material.equals(mesh.material)) {
                material_index = i;
                break;
            }
        }
        meshBuilder.setMaterialIndex(material_index);

        //loadWeights(mesh, skeleton, meshBuilder);

        return meshBuilder.build();
    }

    private static Rig.Model loadModel(Node node, Model model, ArrayList<ModelImporter.Bone> skeleton, ArrayList<String> materials) {

        Rig.Model.Builder modelBuilder = Rig.Model.newBuilder();

        for (Mesh mesh : model.getMeshes()) {
            modelBuilder.addMeshes(loadMesh(mesh, skeleton, materials));
        }

        modelBuilder.setId(MurmurHash.hash64(model.name));

        Vector3d translation = new Vector3d(node.transform.translation.x, node.transform.translation.y, node.transform.translation.z);
        Vector3d scale = new Vector3d(node.transform.scale.x, node.transform.scale.y, node.transform.scale.z);
        Quat4d rotation = new Quat4d(node.transform.rotation.x, node.transform.rotation.y, node.transform.rotation.z, node.transform.rotation.w);
        Transform ddfTransform = MathUtil.vecmathToDDF(translation, rotation, scale);

        modelBuilder.setTransform(ddfTransform);

        return modelBuilder.build();
    }

    private static void loadModelInstances(Node node, ArrayList<ModelImporter.Bone> skeleton, ArrayList<String> materials,
                                            ArrayList<Rig.Model> models) {

        if (node.model != null)
        {
            models.add(loadModel(node, node.model, skeleton, materials));
        }

        for (Node child : node.getChildren()) {
            loadModelInstances(child, skeleton, materials, models);
        }
    }

    public static void loadModels(Scene scene, Rig.MeshSet.Builder meshSetBuilder) {
// TODO: Compare with the skeleton that is set as the "skeleton" !
// Report error if
// * this internal skeleton contains nodes that are not part of the external skeleton
// Remap indices if the layout is different?

        ArrayList<ModelImporter.Bone> skeleton = loadSkeleton(scene);

        ArrayList<String> materials = loadMaterialNames(scene);
        meshSetBuilder.addAllMaterials(materials);

        ArrayList<Rig.Model> models = new ArrayList<>();
        for (Node root : scene.getRootNodes()) {
            loadModelInstances(root, skeleton, materials, models);

            break; // TODO: Support more than one root node
        }

        meshSetBuilder.addAllModels(models);
        meshSetBuilder.setMaxBoneCount(skeleton.size());

        for (ModelImporter.Bone bone : skeleton) {
            meshSetBuilder.addBoneList(MurmurHash.hash64(bone.name));
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // private static void createNodeMap(AINode node, Map<String, AINode> map, int indent) {
    //     map.put(node.mName().dataString(), node);

    //     // for (int i = 0; i < indent; ++i)
    //     // {
    //     //     System.out.printf("  ");
    //     // }
    //     // System.out.printf("Node: %s  numMeshes: %d\n", node.mName().dataString(), node.mNumMeshes());

    //     PointerBuffer children = node.mChildren();
    //     for (int i = 0; i < node.mNumChildren(); ++i) {
    //         AINode child = AINode.create(children.get(i));
    //         createNodeMap(child, map, indent+1);
    //     }
    // }

    // private static void gatherMeshOwners(AIScene aiScene, AINode node, HashMap<String, AINode> meshOwners) {

    //     if (node.mNumMeshes() > 0)
    //     {
    //         String name = node.mName().dataString();
    //         PointerBuffer meshes = aiScene.mMeshes();
    //         int numMeshes = node.mNumMeshes();
    //         IntBuffer meshrefs = node.mMeshes();
    //         for (int i = 0; i < numMeshes; ++i) {
    //             int meshref = meshrefs.get(i);
    //             AIMesh mesh = AIMesh.create(meshes.get(meshref));
    //             String mesh_name = mesh.mName().dataString();

    //             meshOwners.put(mesh_name, node);
    //         }
    //     }

    //     PointerBuffer children = node.mChildren();
    //     for (int i = 0; i < node.mNumChildren(); ++i) {
    //         AINode child = AINode.create(children.get(i));
    //         gatherMeshOwners(aiScene, child, meshOwners);
    //     }
    // }

    // private static void markNecessary(AINode node, AINode mesh_owner, HashSet<String> boneSet) {
    //     // The iteration step from https://assimp-docs.readthedocs.io/en/latest/usage/use_the_lib.html#bones
    //     AINode mesh_owner_parent = mesh_owner.mParent();
    //     if (node == null ||
    //         node.mName().equals(mesh_owner.mName()) ||
    //         node.mName().equals(mesh_owner_parent.mName()))
    //         return;

    //     String name = node.mName().dataString();
    //     boneSet.add(name);

    //     markNecessary(node.mParent(), mesh_owner, boneSet);
    // }

    // private static void gatherSkeleton(AINode node, int parentIndex, HashSet<String> boneSet, List<ModelImporter.Bone> skeleton) {
    //     String name = node.mName().dataString();
    //     if (!boneSet.contains(name))
    //         return;

    //     assert(false); // Use the skeleton returned from the modelimporter!

    //     // ModelImporter.Bone bone = new ModelImporter.Bone();
    //     // bone.name = name;
    //     // bone.node = node;
    //     // bone.index = skeleton.size();
    //     // bone.parentIndex = parentIndex;

    //     // parentIndex = skeleton.size();
    //     // skeleton.add(bone);

    //     // for (Node child : node.getChildren()) {
    //     //     gatherSkeleton(child, parentIndex, boneSet, skeleton);
    //     // }
    // }

    // private static ArrayList<ModelImporter.Bone> findSkeleton(AIScene aiScene) {

    //     // Possible improvements:
    //     // * check the bone.mArmature to only gather the first skeleton found
    //     // * use bone.mNode to find the corresponding node


    //     AINode root = aiScene.mRootNode();

    //     HashSet<String> boneSet = new HashSet<>();
    //     HashMap<String, AIBone> boneMap = new HashMap<>();
    //     HashMap<String, AINode> nodeMap = new HashMap<>();
    //     createNodeMap(root, nodeMap, 0);


    //     HashMap<String, AINode> meshOwners = new HashMap<>();
    //     gatherMeshOwners(aiScene, root, meshOwners);

    //     // for (String key : meshOwners.keySet()) {
    //     //     AINode node = meshOwners.get(key);
    //     //     String name = node.mName().dataString();
    //     // }

    //     int numMeshes = aiScene.mNumMeshes();
    //     PointerBuffer meshes = aiScene.mMeshes();
    //     for (int i = 0; i < numMeshes; ++i) {
    //         AIMesh mesh = AIMesh.create(meshes.get(i));
    //         String mesh_name = mesh.mName().dataString();

    //         //System.out.printf("MESH: %s\n", mesh_name);

    //         int numBones = mesh.mNumBones();
    //         PointerBuffer bones = mesh.mBones();
    //         for (int b = 0; b < numBones; ++b) {
    //             AIBone bone = AIBone.create(bones.get(b));
    //             String bone_name = bone.mName().dataString();
    //             //System.out.printf("  bone: %s 0x%016X\n", bone_name, MurmurHash.hash64(bone_name));

    //             boneMap.put(bone_name, bone);

    //             AINode bone_node = nodeMap.get(bone_name);
    //             AINode mesh_owner = meshOwners.get(mesh_name);
    //             markNecessary(bone_node, mesh_owner, boneSet);
    //         }
    //     }

    //     if (boneSet.isEmpty()) {
    //         return new ArrayList<ModelImporter.Bone>();
    //     }

    //     // Find the root bone
    //     String some_bone_name = boneSet.iterator().next();
    //     AINode root_bone = nodeMap.get(some_bone_name);
    //     AINode parent = root_bone.mParent();
    //     String parent_name = parent.mName().dataString();
    //     while (boneSet.contains(parent_name)) {
    //         root_bone = parent;
    //         parent = parent.mParent();
    //         parent_name = parent.mName().dataString();
    //     }

    //     //System.out.printf("root bone: %s\n", root_bone.mName().dataString());

    //     ArrayList<ModelImporter.Bone> skeleton = new ArrayList<>();
    //     gatherSkeleton(root_bone, BONE_NO_PARENT, boneSet, skeleton);

    //     int index = 0;
    //     for (ModelImporter.Bone bone : skeleton) {
    //         // Note that some bones might not actually influence any vertices, and won't have an AIBone
    //         bone.bone = boneMap.getOrDefault(bone.name, null);

    //         //System.out.printf("Skeleton %2d: %s       0x%016X\n", index++, bone.name, MurmurHash.hash64(bone.name));

    //         // TODO: check that the bone.bone.mNumWeights() is <= 4, otherwise output a warning
    //     }

    //     return skeleton;
    // }

    public static ArrayList<ModelImporter.Bone> loadSkeleton(Scene scene) {
        ArrayList<ModelImporter.Bone> skeleton = new ArrayList<>();

        if (scene.skinsCount == 0)
        {
            return skeleton;
        }

        // get the first skeleton
        ModelImporter.Skin skin = scene.getSkins()[0];
        for (Bone bone : skin.getBones()) {
            skeleton.add(bone);
        }

        return skeleton;
    }

    public static ArrayList<ModelImporter.Bone> loadSkeleton(byte[] content, String suffix, Options options) {
        Scene scene = loadScene(content, suffix, options);
        return loadSkeleton(scene);
    }

    // Generate skeleton DDF data of bones.
    // It will extract the position, rotation and scale from the bone transform as needed by the runtime.
    private static void boneToDDF(ModelImporter.Bone bone, ArrayList<Rig.Bone> ddfBones) {
        Rig.Bone.Builder b = com.dynamo.rig.proto.Rig.Bone.newBuilder();

        b.setParent(bone.parentIndex);
        b.setId(MurmurHash.hash64(bone.name));
        b.setName(bone.name);

        b.setLength(0.0f);
        b.setInheritScale(true);

        ModelImporter.Transform transform = bone.node.transform;
        Vector3d translation = new Vector3d(transform.translation.x, transform.translation.y, transform.translation.z);
        Vector3d scale = new Vector3d(transform.scale.x, transform.scale.y, transform.scale.z);
        Quat4d rotation = new Quat4d(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
        Transform ddfTransform = MathUtil.vecmathToDDF(translation, rotation, scale);

        b.setTransform(ddfTransform);
        // Vector3d translation = new Vector3d(bone.transform.translation.x, bone.transform.translation.y, bone.transform.translation.z);
        // Vector3d scale = new Vector3d(bone.transform.scale.x, bone.transform.scale.y, bone.transform.scale.z);
        // Quat4d rotation = new Quat4d(bone.transform.rotation.x, bone.transform.rotation.y, bone.transform.rotation.z, bone.transform.rotation.w);

        //Transform ddfTransform = MathUtil.vecmathToDDF(translation, rotation, scale);

        // // Decompose pos, rot and scale from bone matrix.
        // Vector3d position = new Vector3d();
        // Quat4d rotation = new Quat4d();
        // Vector3d scale = new Vector3d();
        // MathUtil.decompose(bone.transform, position, rotation, scale);

        // Point3 ddfpos = MathUtil.vecmathToDDF(new Point3d(position));
        // b.setPosition(ddfpos);

        // Quat ddfrot = MathUtil.vecmathToDDF(rotation);
        // b.setRotation(ddfrot);

        // Vector3 ddfscale = MathUtil.vecmathToDDF(scale);
        // b.setScale(ddfscale);

        ddfBones.add(b.build());
    }

    public static boolean loadSkeleton(Scene scene, com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder) {
        ArrayList<Bone> boneList = loadSkeleton(scene);

        // Generate DDF representation of bones.
        ArrayList<Rig.Bone> ddfBones = new ArrayList<Rig.Bone>();
        for (Bone bone : boneList) {
            boneToDDF(bone, ddfBones);
        }
        skeletonBuilder.addAllBones(ddfBones);
        return ddfBones.size() > 0;
    }

    public static void skeletonToDDF(ArrayList<ModelImporter.Bone> bones, com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder) {
        // Generate DDF representation of bones.
        ArrayList<Rig.Bone> ddfBones = new ArrayList<>();
        for (ModelImporter.Bone bone : bones) {
            boneToDDF(bone, ddfBones);
        }
        skeletonBuilder.addAllBones(ddfBones);
    }

// $ java -cp ~/work/defold/tmp/dynamo_home/share/java/bob-light.jar com.dynamo.bob.pipeline.ModelUtil model_asset.dae
    public static void main(String[] args) throws IOException {
        if (args.length < 1) {
            System.err.println("No model specified!");
            return;
        }

        File file = new File(args[0]);
        System.out.printf("Loading '%s'\n", file);
        if (!file.exists()) {
            System.out.printf("File '%s' does not exist!\n", file);
            return;
        }

        Scene scene;
        try {
            String suffix = FilenameUtils.getExtension(file.getName());
            InputStream is = new FileInputStream(file);
            scene = loadScene(is, suffix, new ModelImporter.Options());
        } catch (Exception e) {
            System.out.printf("Failed reading'%s':\n%s\n", file, e.getMessage());
            return;
        }


        // System.out.printf("Scene Root Transform:\n");
        // printMatrix4x4(scene.mRootNode().mName().dataString(), scene.mRootNode().mTransformation());
        // System.out.printf("\n");

        System.out.printf("--------------------------------------------\n");

        // System.out.printf("Bones:\n");

        // ArrayList<ModelImporter.Bone> bones = loadSkeleton(scene);
        // for (Bone bone : bones) {
        //     System.out.printf("  Bone: %s  index: %d  parent: %d\n", bone.name, bone.index, bone.parentIndex);
        // }
        // System.out.printf("--------------------------------------------\n");

        System.out.printf("Materials:\n");
        ArrayList<String> materials = loadMaterialNames(scene);
        for (String material : materials) {
            System.out.printf("  Material: %s\n", material);
        }
        System.out.printf("--------------------------------------------\n");

        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        loadModels(scene, meshSetBuilder);

        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        loadSkeleton(scene, skeletonBuilder);
    }

}
