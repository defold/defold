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

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
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

import java.util.*; //need this??
import org.lwjgl.PointerBuffer;
import org.lwjgl.assimp.*;
import static org.lwjgl.assimp.Assimp.*;

import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.RigUtil;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.RigUtil.AnimationKey;
import com.dynamo.bob.util.RigUtil.Weight;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.proto.DdfMath.Matrix4;

import com.dynamo.rig.proto.Rig;
import com.dynamo.rig.proto.Rig.AnimationInstanceDesc;
import com.dynamo.rig.proto.Rig.AnimationSetDesc;
import com.dynamo.rig.proto.Rig.MeshEntry;
import com.dynamo.rig.proto.Rig.MeshSlot;
import com.google.protobuf.ByteString;
import com.google.protobuf.TextFormat;

public class ModelUtil {

    static int BONE_NO_PARENT = 0xffff;

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

    static public class Bone {
        String      name;
        AINode      node;
        AIBone      bone;
        int         index;
        int         parentIndex;
        Matrix4d    offsetTransform; // the assimp transform
        Matrix4d    transform;       // the local transform
    }

    private static void printMatrix4x4(String name, AIMatrix4x4 mat) {
        System.out.printf("BONE %s:\n", name);
        System.out.printf("    %f, %f, %f, %f\n", mat.a1(),mat.a2(),mat.a3(),mat.a4());
        System.out.printf("    %f, %f, %f, %f\n", mat.b1(),mat.b2(),mat.b3(),mat.b4());
        System.out.printf("    %f, %f, %f, %f\n", mat.c1(),mat.c2(),mat.c3(),mat.c4());
        System.out.printf("    %f, %f, %f, %f\n", mat.d1(),mat.d2(),mat.d3(),mat.d4());
        System.out.printf("\n");
    }

    private static void printMatrix4d(String name, Matrix4d mat) {
        System.out.printf("Mat %s:\n", name);
        System.out.printf("    %f, %f, %f, %f\n", mat.m00,mat.m01,mat.m02,mat.m03);
        System.out.printf("    %f, %f, %f, %f\n", mat.m10,mat.m11,mat.m12,mat.m13);
        System.out.printf("    %f, %f, %f, %f\n", mat.m20,mat.m21,mat.m22,mat.m23);
        System.out.printf("    %f, %f, %f, %f\n", mat.m30,mat.m31,mat.m32,mat.m33);
        System.out.printf("\n");
    }

    private static Matrix4d convertMatrix4x4(AIMatrix4x4 mat) {
        return new Matrix4d(mat.a1(),mat.a2(),mat.a3(),mat.a4(),
                            mat.b1(),mat.b2(),mat.b3(),mat.b4(),
                            mat.c1(),mat.c2(),mat.c3(),mat.c4(),
                            mat.d1(),mat.d2(),mat.d3(),mat.d4());
    }

    public static AssetSpace getAssetSpace(AIScene scene)
    {
        AssetSpace assetSpace = new AssetSpace();
        if (scene != null) {

            // Since LWJGL doesn't seem to support retrieving the values of a scene's metadata
            // We instead have to determine the orientation using the root transform
            //UpAxis guessedUpAxis = null;
            {
                AINode scene_root = scene.mRootNode();
                AIMatrix4x4 mat = scene_root.mTransformation();

                //printMatrix4x4(scene_root.mName().dataString(), mat);

                // if (mat.a1() == 1.0 && mat.b3() == 1.0 && mat.c2() == -1.0) {
                // }

                assetSpace.rotation.setRow(0, new double[] {mat.a1(), mat.a2(), mat.a3(), 0.0});
                assetSpace.rotation.setRow(1, new double[] {mat.b1(), mat.b2(), mat.b3(), 0.0});
                assetSpace.rotation.setRow(2, new double[] {mat.c1(), mat.c2(), mat.c3(), 0.0});
                //assetSpace.rotation.transpose();

                // Blender usually defaults to Z_UP
                // assetSpace.rotation.setRow(0, new double[] {1.0, 0.0, 0.0, 0.0});
                // assetSpace.rotation.setRow(1, new double[] {0.0, 0.0, 1.0, 0.0});
                // assetSpace.rotation.setRow(2, new double[] {0.0, -1.0, 0.0, 0.0});

                printMatrix4d(scene_root.mName().dataString(), assetSpace.rotation);
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

    // private static XMLInput findInput(List<XMLInput> inputs, String semantic, boolean required)
    //         throws LoaderException {
    //     for (XMLInput i : inputs) {
    //         if (i.semantic.equals(semantic))
    //             return i;
    //     }
    //     if (required)
    //         throw new LoaderException(String.format("Input '%s' not found", semantic));
    //     return null;
    // }

    // public static boolean load(InputStream is, Rig.MeshSet.Builder meshSetBuilder, Rig.AnimationSet.Builder animationSetBuilder, Rig.Skeleton.Builder skeletonBuilder) throws IOException, LoaderException {
    //     System.out.printf("ModelUtil: load\n");
    //     XMLCOLLADA collada = loadDAE(is);
    //     loadMesh(collada, meshSetBuilder, true);
    //     loadSkeleton(collada, skeletonBuilder, new ArrayList<String>());
    //     loadAnimations(collada, animationSetBuilder, "", new ArrayList<String>());
    //     return true;
    // }

    // private static HashMap<String, XMLSource> getSamplersLUT(XMLAnimation animation) {
    //     XMLSampler sampler = animation.samplers.get(0);
    //     HashMap<String, XMLSource> samplersLUT = new HashMap<String, XMLSource>();
    //     for (int i = 0; i < sampler.inputs.size(); i++) {
    //         XMLInput input = sampler.inputs.get(i);

    //         // Find source for sampler
    //         XMLSource source = null;
    //         for (int j = 0; j < animation.sources.size(); j++) {
    //             if (animation.sources.get(j).id.equals(input.source)) {
    //                 source = animation.sources.get(j);
    //                 break;
    //             }
    //         }
    //         samplersLUT.put(input.semantic, source);
    //     }
    //     return samplersLUT;
    // }


    public static AIScene loadScene(byte[] content, String suffix) {
        // LWJGL only supports direct byte buffers
        ByteBuffer buffer = ByteBuffer.allocateDirect(content.length);
        buffer.put(ByteBuffer.wrap(content));
        buffer.rewind();

        // Like the texture_profiles, we might want a model_profiles
        // https://github.com/assimp/assimp/blob/e157d7550b02414834c7532143d8751fb30bc886/include/assimp/postprocess.h
        //  aiProcess_ImproveCacheLocality
        //  aiProcess_OptimizeMeshes

        // aiProcess_FlipUVs

        // aiProcess_GenBoundingBoxes - undocumented?

        return aiImportFileFromMemory(buffer, aiProcess_Triangulate | aiProcess_LimitBoneWeights, suffix);
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

        System.out.printf("  bindR %f, %f, %f, %f\n", bindR.x, bindR.y, bindR.z, bindR.w);

        System.out.printf("  assetSpaceR %f, %f, %f, %f\n", assetSpaceR.x, assetSpaceR.y, assetSpaceR.z, assetSpaceR.w);

        // https://github.com/LWJGL/lwjgl3/blob/02e5523774b104866e502e706141ae1a468183e6/modules/lwjgl/assimp/src/generated/java/org/lwjgl/assimp/AINodeAnim.java
        // Highlights:
        // * The keys are absolute, and not relative the parent

        int num_position_keys = anim.mNumPositionKeys();
        int num_rotation_keys = anim.mNumRotationKeys();
        int num_scale_keys = anim.mNumScalingKeys();

        System.out.printf("  BONE: %s nKeys: t %d, r %d, s %d\n", anim.mNodeName().dataString(), num_position_keys, num_rotation_keys, num_scale_keys);

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

        /*
        int keyCount = animation.getInput().length;
        float[] time = animation.getInput();
        float[] values = animation.getOutput();
        for (int key = 0; key < keyCount; ++key) {
            int index = key * 16;
            Matrix4d m = new Matrix4d(new Matrix4f(ArrayUtils.subarray(values, index, index + 16)));
            if (assetSpace != null) {
                m.m03 *= assetSpace.unit;
                m.m13 *= assetSpace.unit;
                m.m23 *= assetSpace.unit;
                m.mul(assetSpace.rotation, m);
            }

            Vector3d p = new Vector3d();
            Quat4d r = new Quat4d();
            Vector3d s = new Vector3d();
            MathUtil.decompose(m, p, r, s);

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

            // Get pose relative transform
            p.set(p.getX() - bindP.getX(), p.getY() - bindP.getY(), p.getZ() - bindP.getZ());
            r.mul(bindR, r);
            s.set(s.getX() / bindS.getX(), s.getY() / bindS.getY(), s.getZ() / bindS.getZ());

            float t = time[key];
            AnimationKey posKey = createKey(t, false, 3);
            toFloats(p, posKey.value);
            posTrack.keys.add(posKey);
            AnimationKey rotKey = createKey(t, false, 4);
            toFloats(r, rotKey.value);
            rotTrack.keys.add(rotKey);
            AnimationKey scaleKey = createKey(t, false, 3);
            toFloats(s, scaleKey.value);
            scaleTrack.keys.add(scaleKey);
        }
        */
    }

    // private static void ExtractKeys(Matrix4d localToParent, AssetSpace assetSpace, XMLAnimation animation, RigUtil.AnimationTrack posTrack, RigUtil.AnimationTrack rotTrack, RigUtil.AnimationTrack scaleTrack) throws LoaderException {
    //     switch (animation.getType()) {
    //     case TRANSLATE:
    //     case SCALE:
    //     case ROTATE:
    //         throw new LoaderException("Currently only collada files with matrix animations are supported.");
    //     case TRANSFORM:
    //     case MATRIX:
    //         ExtractMatrixKeys(localToParent, assetSpace, animation, posTrack, rotTrack, scaleTrack);
    //         break;
    //     default:
    //         throw new LoaderException(String.format("Animations of type %s are not supported.", animation.getType().name()));
    //     }
    // }

    // private static Matrix4d getBoneLocalToParent(Bone bone) {
    //     return new Matrix4d(MathUtil.vecmath2ToVecmath1(bone.bindMatrix));
    // }

    private static void samplePosTrack(Rig.RigAnimation.Builder animBuilder, RigUtil.AnimationTrack track, int boneIndex, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
        if (!track.keys.isEmpty()) {
            Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
            animTrackBuilder.setBoneIndex(boneIndex);
            RigUtil.PositionBuilder positionBuilder = new RigUtil.PositionBuilder(animTrackBuilder);
            RigUtil.sampleTrack(track, positionBuilder, new Point3d(0.0, 0.0, 0.0), startTime, duration, sampleRate, spf, true);
            animBuilder.addTracks(animTrackBuilder.build());
        }
    }

    private static void sampleRotTrack(Rig.RigAnimation.Builder animBuilder, RigUtil.AnimationTrack track, int boneIndex, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
        if (!track.keys.isEmpty()) {
            Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
            animTrackBuilder.setBoneIndex(boneIndex);
            RigUtil.QuatRotationBuilder rotationBuilder = new RigUtil.QuatRotationBuilder(animTrackBuilder);

            System.out.printf("sampleTrack rotations:  startTime %f\n", (float)startTime);
            RigUtil.sampleTrack(track, rotationBuilder, new Quat4d(0.0, 0.0, 0.0, 1.0), startTime, duration, sampleRate, spf, true);

            Rig.AnimationTrack track2 = animTrackBuilder.build();
            int num_rotations = track2.getRotationsCount() / 4;
            System.out.printf("rotations count: %d\n", num_rotations);
            for (int i = 0; i < num_rotations; ++i) {

                System.out.printf(" track rot: %d  %f, %f, %f, %f\n", i, track2.getRotations(i*4+0), track2.getRotations(i*4+1), track2.getRotations(i*4+2), track2.getRotations(i*4+3));
            }

            animBuilder.addTracks(track2);
            //animBuilder.addTracks(animTrackBuilder.build());
        }
    }

    private static void sampleScaleTrack(Rig.RigAnimation.Builder animBuilder, RigUtil.AnimationTrack track, int boneIndex, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
        if (!track.keys.isEmpty()) {
            Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
            animTrackBuilder.setBoneIndex(boneIndex);
            RigUtil.ScaleBuilder scaleBuilder = new RigUtil.ScaleBuilder(animTrackBuilder);
            RigUtil.sampleTrack(track, scaleBuilder, new Vector3d(1.0, 1.0, 1.0), startTime, duration, sampleRate, spf, true);
            animBuilder.addTracks(animTrackBuilder.build());
        }
    }

/*
    private static void boneAnimToDDF(XMLCOLLADA collada, Rig.RigAnimation.Builder animBuilder, ArrayList<Bone> boneList, HashMap<Long, Integer> boneRefMap, HashMap<String, ArrayList<XMLAnimation>> boneToAnimations, double duration) throws LoaderException {

        // Get scene framerate, start and end times if available
        double sceneStartTime = 0.0;
        double sceneEndTime = duration;
        double sceneFrameRate = 30.0f;
        if (collada.libraryVisualScenes.size() == 1) {
            Collection<XMLVisualScene> scenes = collada.libraryVisualScenes.get(0).scenes.values();
            XMLVisualScene scene = scenes.toArray(new XMLVisualScene[0])[0];
            if (scene != null) {
                XMLVisualSceneExtra sceneExtras = scene.extra;
                if (sceneExtras != null) {
                    if (sceneExtras.startTime != null) {
                        sceneStartTime = sceneExtras.startTime;
                    }
                    if (sceneExtras.endTime != null) {
                        sceneEndTime = sceneExtras.endTime;
                    }
                    if (sceneExtras.framerate != null) {
                        sceneFrameRate = sceneExtras.framerate;
                    }
                }
            }
        }
        if (sceneStartTime > sceneEndTime) {
            sceneEndTime = sceneStartTime;
        }

        duration = sceneEndTime - sceneStartTime;
        animBuilder.setDuration((float)duration);

        // We use the supplied framerate (if available) as samplerate to get correct timings when
        // sampling the animation data. We used to have a static sample rate of 30, which would mean
        // if the scene was saved with a different framerate the animation would either be too fast or too slow.
        animBuilder.setSampleRate((float)sceneFrameRate);

        if (boneList == null) {
            return;
        }

        // loop through each bone
        double spf = 1.0 / sceneFrameRate;
        for (int bi = 0; bi < boneList.size(); ++bi)
        {
            Bone bone = boneList.get(bi);
            String boneId = bone.node.id;

            // Lookup the index for the bone in the reference map.
            // This is the bone index to use since we can't guarantee that that
            // the bone hierarchy is stored as depth first, we need to depend on the
            // supplied "JOINT" reference list from the skin entry.
            Long boneHash = MurmurHash.hash64(bone.getSourceId());
            Integer refIndex = boneRefMap.get(boneHash);

            if (boneToAnimations.containsKey(boneId) && refIndex != null)
            {
                Matrix4d localToParent = getBoneLocalToParent(bone);

                // System.out.printf("Bone: %s\n", bone.getName());
                // System.out.printf("  localToParent:\n");
                // System.out.printf("    %f, %f, %f, %f\n", localToParent.m00,localToParent.m01,localToParent.m02,localToParent.m03);
                // System.out.printf("    %f, %f, %f, %f\n", localToParent.m10,localToParent.m11,localToParent.m12,localToParent.m13);
                // System.out.printf("    %f, %f, %f, %f\n", localToParent.m20,localToParent.m21,localToParent.m22,localToParent.m23);
                // System.out.printf("    %f, %f, %f, %f\n", localToParent.m30,localToParent.m31,localToParent.m32,localToParent.m33);
                // System.out.printf("\n");

                AssetSpace assetSpace = getAssetSpace(collada.asset);
                if (bi != 0) {
                    // Only apply up axis rotation for first bone.
                    assetSpace.rotation.setIdentity();
                }

                // search the animations for each bone
                ArrayList<XMLAnimation> anims = boneToAnimations.get(boneId);
                for (XMLAnimation animation : anims) {
                    if (animation.getType() == null) {
                        continue;
                    }

                    RigUtil.AnimationTrack posTrack = new RigUtil.AnimationTrack();
                    posTrack.property = RigUtil.AnimationTrack.Property.POSITION;
                    RigUtil.AnimationTrack rotTrack = new RigUtil.AnimationTrack();
                    rotTrack.property = RigUtil.AnimationTrack.Property.ROTATION;
                    RigUtil.AnimationTrack scaleTrack = new RigUtil.AnimationTrack();
                    scaleTrack.property = RigUtil.AnimationTrack.Property.SCALE;

                    ExtractKeys(bone, localToParent, assetSpace, animation, posTrack, rotTrack, scaleTrack);

                    samplePosTrack(animBuilder, refIndex, posTrack, duration, sceneStartTime, sceneFrameRate, spf, true);
                    sampleRotTrack(animBuilder, refIndex, rotTrack, duration, sceneStartTime, sceneFrameRate, spf, true);
                    sampleScaleTrack(animBuilder, refIndex, scaleTrack, duration, sceneStartTime, sceneFrameRate, spf, true);
                }
            }
        }
    }

*/

    public static void loadAnimationTracks(Rig.RigAnimation.Builder animBuilder, AssetSpace assetSpace, AINodeAnim anim, Matrix4d localToParent, int boneIndex, double duration, double sceneStartTime, double sampleRate, double ticksPerSecond) {
        RigUtil.AnimationTrack posTrack = new RigUtil.AnimationTrack();
        posTrack.property = RigUtil.AnimationTrack.Property.POSITION;
        RigUtil.AnimationTrack rotTrack = new RigUtil.AnimationTrack();
        rotTrack.property = RigUtil.AnimationTrack.Property.ROTATION;
        RigUtil.AnimationTrack scaleTrack = new RigUtil.AnimationTrack();
        scaleTrack.property = RigUtil.AnimationTrack.Property.SCALE;

        ExtractMatrixKeys(anim, ticksPerSecond, localToParent, assetSpace, posTrack, rotTrack, scaleTrack);

        double spf = 1.0 / sampleRate;

        samplePosTrack(animBuilder, posTrack, boneIndex, duration, sceneStartTime, sampleRate, spf, true);
        sampleRotTrack(animBuilder, rotTrack, boneIndex, duration, sceneStartTime, sampleRate, spf, true);
        sampleScaleTrack(animBuilder, scaleTrack, boneIndex, duration, sceneStartTime, sampleRate, spf, true);
    }

    public static void loadAnimations(byte[] animContent, String suffix, ArrayList<ModelUtil.Bone> bones, Rig.AnimationSet.Builder animationSetBuilder, String parentAnimationId, ArrayList<String> animationIds) {
        AIScene animScene = loadScene(animContent, suffix);
        loadAnimations(animScene, bones, animationSetBuilder, parentAnimationId, animationIds);
    }

    private static double getAnimationStartTime(AIAnimation anim) {
        double startTime = 10000.0;

        double ticksPerSecond = anim.mTicksPerSecond();
        PointerBuffer aiChannels = anim.mChannels();
        int num_channels = anim.mNumChannels();
        for (int c = 0; c < num_channels; ++c) {
            AINodeAnim nodeAnim = AINodeAnim.create(aiChannels.get(c));
            {
                AIVectorKey.Buffer buffer = nodeAnim.mPositionKeys();
                while (buffer.remaining() > 0) {
                    AIVectorKey key = buffer.get();
                    double time = key.mTime() / ticksPerSecond;
                    if (time < startTime) {
                        startTime = time;
                    }
                    break;
                }
            }
            {
                AIQuatKey.Buffer buffer = nodeAnim.mRotationKeys();
                while (buffer.remaining() > 0) {
                    AIQuatKey key = buffer.get();
                    double time = key.mTime() / ticksPerSecond;
                    if (time < startTime) {
                        startTime = time;
                    }
                    break;
                }
            }
            {
                AIVectorKey.Buffer buffer = nodeAnim.mScalingKeys();
                while (buffer.remaining() > 0) {
                    AIVectorKey key = buffer.get();
                    double time = key.mTime() / ticksPerSecond;
                    if (time < startTime) {
                        startTime = time;
                    }
                    break;
                }
            }
        }
        if (startTime == 10000.0) {
            startTime = 0.0;
        }
        return startTime;
    }

    public static void loadAnimations(AIScene scene, ArrayList<ModelUtil.Bone> skeleton, Rig.AnimationSet.Builder animationSetBuilder, String parentAnimationId, ArrayList<String> animationIds) {
        // if (collada.libraryAnimations.size() != 1) {
        //     return;
        // }

        // List<String> boneRefArray = createBoneReferenceList(collada);
        // if (boneRefArray == null || boneRefArray.isEmpty()) {
        //     return;
        // }
        // HashMap<Long, Integer> boneRefMap = new HashMap<Long, Integer>();
        // for (int i = 0; i < boneRefArray.size(); i++) {
        //     Long boneRef = MurmurHash.hash64(boneRefArray.get(i));

        //     //System.out.printf("LoadAnimations: bone_ref: %d %s  %016X\n", i, boneRefArray.get(i), boneRef);

        //     boneRefMap.put(boneRef, i);
        //     animationSetBuilder.addBoneList(boneRef);
        // }

        // for (ModelUtil.Bone bone : skeleton) {
        //     animationSetBuilder.addBoneList(MurmurHash.hash64(bone.name));
        // }

        AssetSpace assetSpace = getAssetSpace(scene);

        System.out.printf("loadAnimations skeleton: size: %d\n", skeleton.size());

        int num_animations = scene.mNumAnimations();

        System.out.printf("ModelUtil: loadAnimations: %d\n", num_animations);

        PointerBuffer aianimations = scene.mAnimations();
        for (int i = 0; i < num_animations; ++i) {
            AIAnimation aianimation = AIAnimation.create(aianimations.get(i));
            String animation_name = aianimation.mName().dataString();

            double duration = aianimation.mDuration();
            double ticksPerSecond = aianimation.mTicksPerSecond();
            double durationTime = duration/ticksPerSecond;
            double sampleRate = 30.0;

            int num_channels = aianimation.mNumChannels();

            Rig.RigAnimation.Builder animBuilder = Rig.RigAnimation.newBuilder();
            animBuilder.setDuration((float)durationTime);

            // We use the supplied framerate (if available) as samplerate to get correct timings when
            // sampling the animation data. We used to have a static sample rate of 30, which would mean
            // if the scene was saved with a different framerate the animation would either be too fast or too slow.
            animBuilder.setSampleRate((float)sampleRate);

            double startTime = 0.0;//getAnimationStartTime(aianimation);

            System.out.printf("ANIMATION: %s  dur: %f  sampleRate: %f  startTime: %f  mNumChannels: %d\n", animation_name, (float)durationTime, (float)sampleRate, (float)startTime, num_channels);

            PointerBuffer aiChannels = aianimation.mChannels();
            for (int c = 0; c < num_channels; ++c) {
                AINodeAnim aiNodeAnim = AINodeAnim.create(aiChannels.get(c));
                String node_name = aiNodeAnim.mNodeName().dataString();

                int bone_index = 0;
                ModelUtil.Bone current_bone = null;
                for (ModelUtil.Bone bone : skeleton) {
                    if (bone.name.equals(node_name)) {
                        current_bone = bone;
                        break;
                    }
                    ++bone_index;
                }

                if (current_bone == null) {
                    // If we have an empty skeleton, we have to rely on the # of animation tracks
                    bone_index = c;
                }

                Matrix4d localToParent = current_bone.transform;

                AssetSpace curAssetSpace = assetSpace;
                if (bone_index != 0)
                {
                    // Only apply up axis rotation for first bone.
                    curAssetSpace.rotation.setIdentity();
                }

                loadAnimationTracks(animBuilder, curAssetSpace, aiNodeAnim, localToParent, bone_index, durationTime, startTime, sampleRate, ticksPerSecond);
            }

            // The file name is usually based on the file name itself
            animBuilder.setId(MurmurHash.hash64(parentAnimationId));
            animationIds.add(parentAnimationId);
            animationSetBuilder.addAnimations(animBuilder.build());
        }

        // // Animation clips
        // ArrayList<XMLLibraryAnimationClips> animClips = collada.libraryAnimationClips;
        // XMLLibraryAnimations libraryAnimation = collada.libraryAnimations.get(0);

        // if(!animClips.isEmpty()) {

        //     throw new LoaderException("Anmation clips are currently not supported.");

        // } else {
        //     float totalAnimationLength = 0.0f;
        //     HashMap<String, ArrayList<XMLAnimation>> boneToAnimations = new HashMap<String, ArrayList<XMLAnimation>>();

        //     // Loop through all animations and build a bone-to-animations LUT
        //     Iterator<Entry<String, XMLAnimation>> it = libraryAnimation.animations.entrySet().iterator();
        //     while (it.hasNext()) {
        //         XMLAnimation animation = (XMLAnimation)it.next().getValue();
        //         String boneTarget = animation.getTargetBone();
        //         if (!boneToAnimations.containsKey(animation.getTargetBone())) {
        //             boneToAnimations.put(boneTarget, new ArrayList<XMLAnimation>());
        //         }
        //         boneToAnimations.get(boneTarget).add(animation);

        //         // Figure out the total duration of the animation.
        //         HashMap<String, XMLSource> samplersLUT = getSamplersLUT(animation);
        //         XMLSource inputSampler = samplersLUT.get("INPUT");
        //         float animLength = inputSampler.floatArray.floats[inputSampler.floatArray.count-1];
        //         totalAnimationLength = Math.max(totalAnimationLength, animLength);
        //     }

        //     // If no clips are provided, add a "Default" clip that is the whole animation as one clip
        //     Rig.RigAnimation.Builder animBuilder = Rig.RigAnimation.newBuilder();
        //     boneAnimToDDF(collada, animBuilder, boneList, boneRefMap, boneToAnimations, totalAnimationLength);
        //     animBuilder.setId(MurmurHash.hash64(parentAnimationId));
        //     animationIds.add(parentAnimationId);
        //     animationSetBuilder.addAnimations(animBuilder.build());
        // }
    }

    private static ArrayList<Float> getVertexData(AIVector3D.Buffer buffer) {
        if (buffer == null)
            return null;
        ArrayList<Float> out = new ArrayList<>();
        while (buffer.remaining() > 0) {
            AIVector3D v = buffer.get();
            out.add(v.x());
            out.add(v.y());
            out.add(v.z());
        }
        return out;
    }

    public static void loadWeights(AIMesh mesh, ArrayList<ModelUtil.Bone> skeleton, Rig.Mesh.Builder meshBuilder) {
        int num_vertices = mesh.mNumVertices();

        int num_bones = mesh.mNumBones();
        if (num_bones == 0)
            return;

        ArrayList<Float>[] bone_weights = new ArrayList[num_vertices];
        ArrayList<Integer>[] bone_indices = new ArrayList[num_vertices];
        for (int i = 0; i < num_vertices; ++i) {
            bone_weights[i] = new ArrayList<Float>();
            bone_indices[i] = new ArrayList<Integer>();
        }

        PointerBuffer bones = mesh.mBones();
        for (int i = 0; i < num_bones; ++i) {
            AIBone bone = AIBone.create(bones.get(i));

            // find the index of this bone in the skeleton list
            int bone_index = 0;
            for (ModelUtil.Bone skeleton_bone : skeleton) {
                if (skeleton_bone.name.equals(bone.mName().dataString())) {
                    break;
                }
                bone_index++;
            }

            AIVertexWeight.Buffer aiWeights = bone.mWeights();
            while (aiWeights.remaining() > 0) {
                AIVertexWeight bone_weight = aiWeights.get();
                int vertex_index = bone_weight.mVertexId();
                float vertex_weight = bone_weight.mWeight();

                bone_weights[vertex_index].add(vertex_weight);
                bone_indices[vertex_index].add(bone_index);
            }
        }

        // Pad each list to the desired length (currently 4 weights per vertex)
        for (int i = 0; i < num_vertices; ++i) {
            ArrayList<Float> weights = bone_weights[i];
            while (weights.size() < 4) {
                weights.add(0.0f);
                bone_indices[i].add(0);
            }
        }

        // flatten the lists
        ArrayList<Float> flattened_weights = new ArrayList<>();
        ArrayList<Integer> flattened_indices = new ArrayList<>();
        for (int i = 0; i < num_vertices; ++i) {
            flattened_weights.addAll(bone_weights[i]);
            flattened_indices.addAll(bone_indices[i]);

            // System.out.printf("%4d: %f, %f, %f, %f = %f   %d, %d, %d, %d\n", i, bone_weights[i].get(0), bone_weights[i].get(1), bone_weights[i].get(2), bone_weights[i].get(3),
            //     bone_weights[i].get(0) + bone_weights[i].get(1) + bone_weights[i].get(2) + bone_weights[i].get(3),
            //         bone_indices[i].get(0), bone_indices[i].get(1), bone_indices[i].get(2), bone_indices[i].get(3));
        }
        meshBuilder.addAllWeights(flattened_weights);
        meshBuilder.addAllBoneIndices(flattened_indices);
    }

    private static String getMaterialProperty(AIMaterial aiMaterial, String name) {
        AIString buffer = AIString.calloc();
        Assimp.aiGetMaterialString(aiMaterial, name, 0, 0, buffer);
        return buffer.dataString();
    }

    public static ArrayList<String> loadMaterials(AIScene scene) {
        ArrayList<String> materials = new ArrayList<>();

        int num_materials = scene.mNumMaterials();
        PointerBuffer aiMaterials = scene.mMaterials();
        for (int i = 0; i < num_materials; i++) {
            AIMaterial aiMaterial = AIMaterial.create(aiMaterials.get(i));

            String id = getMaterialProperty(aiMaterial, Assimp.AI_MATKEY_NAME);
            if (id == null)
            {
                id = String.format("null_%d", i);
            }
            materials.add(id);
        }
        return materials;
    }

    private class OptimizedMesh
    {
        Rig.IndexBufferFormat indices_format;
        int                   indices_count;
        ByteBuffer            indices_bytes;
        List<Rig.MeshVertexIndices> mesh_vertex_indices;
    };

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
        // We can then use this to quickly build am optimized indexed vertex buffer of any selected vertex elements in run-time without any sorting.
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


        System.out.printf("opt: mesh_vertex_indices: %d\n", mesh_vertex_indices.size());
        System.out.printf("opt: num_indices:         %d\n", mesh_index_list.size());
    }

    public static Rig.Mesh loadMesh(AIMesh mesh, ArrayList<ModelUtil.Bone> skeleton) {
        ArrayList<Float> positions = getVertexData(mesh.mVertices());

        String name = mesh.mName().dataString();
// if (name.equals("Cube-mesh"))
// {
// System.out.printf("Mesh positions: %s\n", mesh.mName().dataString());
//             for (int i = 0; i < positions.size()/3; ++i)
//             {
// System.out.printf("  position %d: %f %f %f \n", i, positions.get(i*3+0), positions.get(i*3+1), positions.get(i*3+2));
//             }
// }

        ArrayList<Float> normals = getVertexData(mesh.mNormals());
        ArrayList<Float> tangents = getVertexData(mesh.mTangents());
        //ArrayList<Float> bitangents = getVertexData(mesh.mBitangents());

        ArrayList<Float> texcoord0 = getVertexData(mesh.mTextureCoords(0));
        ArrayList<Float> texcoord1 = getVertexData(mesh.mTextureCoords(1));

        Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();

        if (positions != null)
            meshBuilder.addAllPositions(positions);

        if (normals != null)
            meshBuilder.addAllNormals(normals);

        if (tangents != null)
            meshBuilder.addAllTangents(tangents);

        if (texcoord0 != null) {
            meshBuilder.addAllTexcoord0(texcoord0);
            meshBuilder.setNumTexcoord0Components(mesh.mNumUVComponents(0));
        }
        if (texcoord1 != null) {
            meshBuilder.addAllTexcoord1(texcoord1);
            meshBuilder.setNumTexcoord0Components(mesh.mNumUVComponents(1));
        }

        int triangle_count = mesh.mNumFaces();

        List<Integer> position_indices_list = new ArrayList<Integer>(triangle_count*3);
        List<Integer> normal_indices_list = new ArrayList<Integer>(triangle_count*3);
        List<Integer> texcoord0_indices_list = new ArrayList<Integer>(triangle_count*3);

        AIFace.Buffer faces = mesh.mFaces();
        for (int f = 0; f < triangle_count; ++f) {
            AIFace face = faces.get(f);
            assert(face.mNumIndices() == 3); // We only allow triangles

            IntBuffer indices = face.mIndices();
            for (int i = 0; i < 3; ++i) {

                // Sometimes the <p> values can be -1 from Maya exports, we clamp it below to 0 instead.
                // Similar solution as AssImp; https://github.com/assimp/assimp/blob/master/code/ColladaParser.cpp#L2336
                // TODO: Is it really true when using the AssImp library?

                int vertex_index = indices.get(i);
                vertex_index = Math.max(0, vertex_index);

                position_indices_list.add(vertex_index);

                if (normals != null) {
                    normal_indices_list.add(vertex_index);
                }

                if (texcoord0 != null) {
                    texcoord0_indices_list.add(vertex_index);
                } else {
                    texcoord0_indices_list.add(0);
                }
            }
        }

        if(normals != null) {
            meshBuilder.addAllNormalsIndices(normal_indices_list);
        }
        meshBuilder.addAllPositionIndices(position_indices_list);
        meshBuilder.addAllTexcoord0Indices(texcoord0_indices_list);

        System.out.printf("position_indices_list: %d\n", position_indices_list.size());
        System.out.printf("normal_indices_list: %d\n", normal_indices_list.size());
        System.out.printf("texcoord0_indices_list: %d\n", texcoord0_indices_list.size());

        createMeshIndices(meshBuilder, triangle_count, true, position_indices_list, normal_indices_list, texcoord0_indices_list);

        int material_index = mesh.mMaterialIndex();
        meshBuilder.setMaterialIndex(material_index);

        loadWeights(mesh, skeleton, meshBuilder);

        return meshBuilder.build();
    }

    public static Matrix4d getTransform(AINode node) {
        if (node == null) {
            Matrix4d identity = new Matrix4d();
            identity.setIdentity();
            return identity;
        }
        Matrix4d transform = convertMatrix4x4(node.mTransformation());
        Matrix4d parentTransform = getTransform(node.mParent());
        transform.mul(parentTransform, transform);
        return transform;
    }

    public static void loadMeshInstances(AINode node, ArrayList<String> mesh_names, ArrayList<Rig.MeshEntry> mesh_entries) {
        int num_meshes = node.mNumMeshes();

        if (num_meshes > 0)
        {
            IntBuffer meshrefs = node.mMeshes();

            String node_name = node.mName().dataString();

            Rig.MeshEntry.Builder meshEntryBuilder = Rig.MeshEntry.newBuilder();
            meshEntryBuilder.setId(MurmurHash.hash64(node_name));

            Matrix4d transformd = getTransform(node);
            Matrix4 transform = MathUtil.vecmathToDDF(transformd);
            meshEntryBuilder.setTransform(transform);

            for (int i = 0; i < num_meshes; ++i) {
                int meshref = meshrefs.get(i);

                Rig.MeshSlot.Builder meshSlotBuilder = Rig.MeshSlot.newBuilder();

                String mesh_name = mesh_names.get(meshref);
                System.out.printf("MESH SLOT: %s  %d \n", mesh_name, MurmurHash.hash64(mesh_name));
                meshSlotBuilder.setId(MurmurHash.hash64(mesh_name));
                meshSlotBuilder.setActiveIndex(0); // Default to the first mesh in the mesh attachments
                meshSlotBuilder.addMeshAttachments(meshref);

                // // Not really used by models
                // meshSlotBuilder.addSlotColor(1.0f);
                // meshSlotBuilder.addSlotColor(1.0f);
                // meshSlotBuilder.addSlotColor(1.0f);
                // meshSlotBuilder.addSlotColor(1.0f);

                meshEntryBuilder.addMeshSlots(meshSlotBuilder);

                printMatrix4d(node.mName().dataString(), transformd);
            }

            System.out.printf("MESH ENTRY: %s %d\n", node_name, MurmurHash.hash64(node_name));

            mesh_entries.add(meshEntryBuilder.build());
        }

        PointerBuffer children = node.mChildren();
        for (int i = 0; i < node.mNumChildren(); ++i) {
            AINode child = AINode.create(children.get(i));
            loadMeshInstances(child, mesh_names, mesh_entries);
        }
    }

    public static void loadMeshes(AIScene scene, Rig.MeshSet.Builder meshSetBuilder) {
// TODO: Compare with the skeleton that is set as the "skeleton" !
// Report error if
// * this internal skeleton contains nodes that are not part of the external skeleton
// Remap indices if the layout is different?

        ArrayList<ModelUtil.Bone> skeleton = loadSkeleton(scene);

        ArrayList<String> materials = loadMaterials(scene);
        meshSetBuilder.addAllMaterials(materials);

        ArrayList<Rig.Mesh> meshes = new ArrayList<>();
        ArrayList<String> mesh_names = new ArrayList<>();

        PointerBuffer aiMeshes = scene.mMeshes();
        for (int i = 0; i < scene.mNumMeshes(); ++i) {
            AIMesh aiMesh = AIMesh.create(aiMeshes.get(i));

            String mesh_name = aiMesh.mName().dataString();
            mesh_names.add(mesh_name);
            System.out.printf("MESH: %s\n", mesh_name);

            Rig.Mesh mesh = loadMesh(aiMesh, skeleton);
            meshes.add(mesh);
        }

        ArrayList<Rig.MeshEntry> mesh_entries = new ArrayList<>();
        loadMeshInstances(scene.mRootNode(), mesh_names, mesh_entries);

        // The total number of slots in the scene (we can use it to preallocate data)
        int num_slots = 0;
        for (Rig.MeshEntry entry : mesh_entries) {
            num_slots += entry.getMeshSlotsCount();
        }
        meshSetBuilder.setSlotCount(num_slots);

        meshSetBuilder.addAllMeshAttachments(meshes);
        meshSetBuilder.addAllMeshEntries(mesh_entries);
        meshSetBuilder.setMaxBoneCount(skeleton.size());

        for (ModelUtil.Bone bone : skeleton) {
            meshSetBuilder.addBoneList(MurmurHash.hash64(bone.name));
        }
    }


    public static void loadSkeleton(byte[] content, String suffix, Rig.Skeleton.Builder skeletonBuilder) throws IOException, LoaderException {
        AIScene aiScene = loadScene(content, suffix);
        loadSkeleton(aiScene, skeletonBuilder);
    }


    // // Creates a list of bone ids ordered by their index.
    // // Their index is first taken from the order they appear in the skin entry of the collada file,
    // // which correspond to the index where their inv bind pose transform is located in the INV_BIND_MATRIX entry.
    // private static ArrayList<String> createBoneReferenceList(XMLCOLLADA collada) throws LoaderException {

    //     HashSet<String> boneIdLut = new HashSet<String>();
    //     ArrayList<String> boneRefArray = new ArrayList<String>();

    //     // First get the bone index from the skin entry
    //     XMLSkin skin = null;
    //     if (!collada.libraryControllers.isEmpty()) {
    //         skin = findFirstSkin(collada.libraryControllers.get(0));

    //         if (skin != null)
    //         {
    //             List<XMLSource> sources = skin.sources;
    //             HashMap<String, XMLSource> sourcesMap = getSourcesMap(sources);

    //             XMLInput jointInput = findInput(skin.jointsInputs, "JOINT", true);
    //             XMLSource jointSource = sourcesMap.get(jointInput.source);

    //             if(jointSource.nameArray != null) {
    //                 boneRefArray = new ArrayList<String>(Arrays.asList(jointSource.nameArray.names));
    //             } else if(jointSource.idrefArray != null) {
    //                 boneRefArray = new ArrayList<String>(Arrays.asList(jointSource.idrefArray.idrefs));
    //             }

    //             for (String boneId : boneRefArray) {
    //                 boneIdLut.add(boneId);
    //             }
    //         }
    //     }

    //     // Next, add the remaining bones from the visual scene. These are bones that don't have animation,
    //     // their order/indices are not relevant why they are added last.
    //     if (!collada.libraryVisualScenes.isEmpty()) {
    //         Matrix4d t = new Matrix4d();
    //         XMLNode rootNode = findSkeletonNode(collada, t);
    //         if (rootNode != null) {
    //             getBoneReferenceFromSceneNode(rootNode, boneRefArray, boneIdLut);
    //         }
    //     }

    //     return boneRefArray;
    // }

    // // Recurses through JOINT nodes and fills the boneTransform map with their local transforms.
    // // If a joint/bone has an entry in the invBoneTransforms map it will first convert the stored
    // // entry from asset space into local space and then store the result in boneTransform.
    // private static void fillBoneTransformsFromVisualScene(XMLNode node, Matrix4d parentTransform, HashMap<String, Matrix4d> boneTransforms, HashMap<String, Matrix4d> invBoneTransforms) {
    //     Matrix4d sceneNodeTransform = new Matrix4d(MathUtil.vecmath2ToVecmath1(node.matrix.matrix4f));

    //     if (invBoneTransforms.containsKey(node.id)) {
    //         // If this bone exist in the invBoneTransform map
    //         Matrix4d boneTransform = new Matrix4d(invBoneTransforms.get(node.id));
    //         boneTransform.invert();
    //         Matrix4d invParentTransform = new Matrix4d(parentTransform);
    //         invParentTransform.invert();
    //         boneTransform.mul(invParentTransform, boneTransform);
    //         sceneNodeTransform = boneTransform;
    //     }

    //     // Calculate next parentTransform
    //     Matrix4d newParentTransform = new Matrix4d(parentTransform);
    //     newParentTransform.mul(parentTransform, sceneNodeTransform);

    //     boneTransforms.put(node.id, sceneNodeTransform);


    //     for(XMLNode childNode : node.childrenList) {
    //         if(childNode.type == XMLNode.Type.JOINT) {
    //             fillBoneTransformsFromVisualScene(childNode, newParentTransform, boneTransforms, invBoneTransforms);
    //         }
    //     }
    // }

    // // Create a map of bone id to bone transform (local space).
    // // Needed to have correct local bone transforms before creating bone hierarchy in loadSkeleton.
    // // This function reads from both the skin and visual scene entries in the collada file
    // // to get correct transforms for both animated and non-animated bones.
    // private static HashMap<String, Matrix4d> createBoneTransformsMap(XMLCOLLADA collada) throws LoaderException
    // {
    //     // Final map with bone-name -> bone-transform (in local space, relative to its parent)
    //     HashMap<String, Matrix4d> boneTransforms = new HashMap<String, Matrix4d>();

    //     // Map used to keep track of asset space inv pose transforms for bones with animation.
    //     HashMap<String, Matrix4d> invBoneTransforms = new HashMap<String, Matrix4d>();

    //     // First we try to extract the inverse bone bind poses from the skin entry
    //     if (!collada.libraryControllers.isEmpty()) {
    //         XMLSkin skin = findFirstSkin(collada.libraryControllers.get(0));

    //         if (skin != null) {

    //             List<XMLSource> sources = skin.sources;
    //             HashMap<String, XMLSource> sourcesMap = getSourcesMap(sources);

    //             XMLInput invBindMatInput = findInput(skin.jointsInputs, "INV_BIND_MATRIX", true);
    //             XMLSource invBindMatSource = sourcesMap.get(invBindMatInput.source);
    //             if (invBindMatSource != null) {
    //                 float[] invBindMatFloats = invBindMatSource.floatArray.floats;

    //                 // Get the list of bone names and their corresponding index from the order they are stored in the INV_BIND_MATRIX.
    //                 // We need this inside the loop below so we can map them from bone name to bone inv pose matrix.
    //                 List<String> boneIdList = createBoneReferenceList(collada);

    //                 int matricesCount = invBindMatFloats.length / 16;
    //                 for (int i = 0; i < matricesCount; i++)
    //                 {
    //                     int j = i*16;
    //                     String boneId = boneIdList.get(i);
    //                     Matrix4d invBoneTransform = new Matrix4d(new Matrix4f(ArrayUtils.subarray(invBindMatFloats, j, j + 16)));
    //                     invBoneTransforms.put(boneId, invBoneTransform);
    //                 }
    //             }
    //         }
    //     }

    //     XMLNode rootNode = findSkeletonNode(collada, new Matrix4d());

    //     // Now we need to extract any missing bone transforms that wasn't available in the skin entry, such as non-animated bones.
    //     // Bones with animations have their bind pose stored as inverse matrices in asset space (see above loop),
    //     // but Defold stores bone transforms in local space relative to each parent so we need to "undo" this for these bones.
    //     if (rootNode != null) {
    //         Matrix4d parentTransform = new Matrix4d();
    //         parentTransform.setIdentity();
    //         fillBoneTransformsFromVisualScene(rootNode, parentTransform, boneTransforms, invBoneTransforms);
    //     }

    //     return boneTransforms;
    // }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////

    private static void createNodeMap(AINode node, Map<String, AINode> map, int indent) {
        map.put(node.mName().dataString(), node);

        for (int i = 0; i < indent; ++i)
        {
            System.out.printf("  ");
        }
        System.out.printf("Node: %s  numMeshes: %d\n", node.mName().dataString(), node.mNumMeshes());

        PointerBuffer children = node.mChildren();
        for (int i = 0; i < node.mNumChildren(); ++i) {
            AINode child = AINode.create(children.get(i));
            createNodeMap(child, map, indent+1);
        }
    }

    private static void gatherMeshOwners(AIScene aiScene, AINode node, HashMap<String, AINode> meshOwners) {

        if (node.mNumMeshes() > 0)
        {
            String name = node.mName().dataString();
            PointerBuffer meshes = aiScene.mMeshes();
            int numMeshes = node.mNumMeshes();
            IntBuffer meshrefs = node.mMeshes();
            for (int i = 0; i < numMeshes; ++i) {
                int meshref = meshrefs.get(i);
                AIMesh mesh = AIMesh.create(meshes.get(meshref));
                String mesh_name = mesh.mName().dataString();

                meshOwners.put(mesh_name, node);
            }
        }

        PointerBuffer children = node.mChildren();
        for (int i = 0; i < node.mNumChildren(); ++i) {
            AINode child = AINode.create(children.get(i));
            gatherMeshOwners(aiScene, child, meshOwners);
        }
    }

    private static void markNecessary(AINode node, AINode mesh_owner, HashSet<String> boneSet) {
        // The iteration step from https://assimp-docs.readthedocs.io/en/latest/usage/use_the_lib.html#bones
        AINode mesh_owner_parent = mesh_owner.mParent();
        if (node.mName().equals(mesh_owner.mName()) ||
            node.mName().equals(mesh_owner_parent.mName()))
            return;

        String name = node.mName().dataString();
        boneSet.add(name);

        markNecessary(node.mParent(), mesh_owner, boneSet);
    }

    private static void gatherSkeleton(AINode node, int parentIndex, HashSet<String> boneSet, List<ModelUtil.Bone> skeleton) {
        String name = node.mName().dataString();
        if (!boneSet.contains(name))
            return;

        ModelUtil.Bone bone = new ModelUtil.Bone();
        bone.name = name;
        bone.node = node;
        bone.index = skeleton.size();
        bone.parentIndex = parentIndex;

        parentIndex = skeleton.size();
        skeleton.add(bone);

        PointerBuffer children = node.mChildren();
        for (int i = 0; i < node.mNumChildren(); ++i) {
            AINode child = AINode.create(children.get(i));
            gatherSkeleton(child, parentIndex, boneSet, skeleton);
        }
    }

    private static ArrayList<ModelUtil.Bone> findSkeleton(AIScene aiScene) {

        // Possible improvements:
        // * check the bone.mArmature to only gather the first skeleton found
        // * use bone.mNode to find the corresponding node


        AINode root = aiScene.mRootNode();

        HashSet<String> boneSet = new HashSet<>();
        HashMap<String, AIBone> boneMap = new HashMap<>();
        HashMap<String, AINode> nodeMap = new HashMap<>();
        createNodeMap(root, nodeMap, 0);


        HashMap<String, AINode> meshOwners = new HashMap<>();
        gatherMeshOwners(aiScene, root, meshOwners);

        for (String key : meshOwners.keySet()) {
            AINode node = meshOwners.get(key);
            String name = node.mName().dataString();
        }

        int numMeshes = aiScene.mNumMeshes();
        PointerBuffer meshes = aiScene.mMeshes();
        for (int i = 0; i < numMeshes; ++i) {
            AIMesh mesh = AIMesh.create(meshes.get(i));
            String mesh_name = mesh.mName().dataString();

            System.out.printf("MESH: %s\n", mesh_name);

            int numBones = mesh.mNumBones();
            PointerBuffer bones = mesh.mBones();
            for (int b = 0; b < numBones; ++b) {
                AIBone bone = AIBone.create(bones.get(b));
                String bone_name = bone.mName().dataString();
                System.out.printf("  bone: %s 0x%016X\n", bone_name, MurmurHash.hash64(bone_name));

                boneMap.put(bone_name, bone);

                AINode bone_node = nodeMap.get(bone_name);
                AINode mesh_owner = meshOwners.get(mesh_name);
                markNecessary(bone_node, mesh_owner, boneSet);
            }
        }

        if (boneMap.isEmpty()) {
            return new ArrayList<ModelUtil.Bone>();
        }

        // Find the root bone
        String some_bone_name = boneSet.iterator().next();
        AINode root_bone = nodeMap.get(some_bone_name);
        AINode parent = root_bone.mParent();
        String parent_name = parent.mName().dataString();
        while (boneSet.contains(parent_name)) {
            root_bone = parent;
            parent = parent.mParent();
            parent_name = parent.mName().dataString();
        }

        System.out.printf("root bone: %s\n", root_bone.mName().dataString());

        ArrayList<ModelUtil.Bone> skeleton = new ArrayList<>();
        gatherSkeleton(root_bone, BONE_NO_PARENT, boneSet, skeleton);

        int index = 0;
        for (ModelUtil.Bone bone : skeleton) {
            // Note that some bones might not actually influence any vertices, and won't have an AIBone
            bone.bone = boneMap.getOrDefault(bone.name, null);

            System.out.printf("Skeleton %2d: %s       0x%016X\n", index++, bone.name, MurmurHash.hash64(bone.name));

            // TODO: check that the bone.bone.mNumWeights() is <= 4, otherwise output a warning
        }

        return skeleton;
    }

    public static ArrayList<ModelUtil.Bone> loadSkeleton(AIScene aiScene) {
        AssetSpace assetSpace = getAssetSpace(aiScene);

        ArrayList<ModelUtil.Bone> skeleton = findSkeleton(aiScene);

        Matrix4d skeletonTransform = new Matrix4d();
        skeletonTransform.setIdentity();

        for (ModelUtil.Bone bone : skeleton) {
            AIMatrix4x4 offset_matrix = AIMatrix4x4.create();
            if (bone.bone != null) {
                offset_matrix = bone.bone.mOffsetMatrix();
            } else {
                offset_matrix.set(1.0f,0.0f,0.0f,0.0f, 0.0f,1.0f,0.0f,0.0f, 0.0f,0.0f,1.0f,0.0f, 0.0f,0.0f,0.0f,1.0f);
            }

            // "The offset matrix is a 4x4 matrix that transforms from mesh space to bone space in bind pose." - https://github.com/assimp/assimp/blob/master/port/jassimp/jassimp/src/jassimp/AiBone.java
            bone.offsetTransform = convertMatrix4x4(offset_matrix);
            printMatrix4d("offsetTransform", bone.offsetTransform);

            bone.transform = convertMatrix4x4(bone.node.mTransformation());
            printMatrix4d("transform", bone.transform);
        }
        return skeleton;
    }

    public static ArrayList<ModelUtil.Bone> loadSkeleton(byte[] content, String suffix) {
        AIScene aiScene = loadScene(content, suffix);
        return loadSkeleton(aiScene);
    }

    // Generate skeleton DDF data of bones.
    // It will extract the position, rotation and scale from the bone transform as needed by the runtime.
    private static void boneToDDF(ModelUtil.Bone bone, ArrayList<Rig.Bone> ddfBones) {
        Rig.Bone.Builder b = com.dynamo.rig.proto.Rig.Bone.newBuilder();

        b.setParent(bone.parentIndex);
        b.setId(MurmurHash.hash64(bone.name));

        // Decompose pos, rot and scale from bone matrix.
        Vector3d position = new Vector3d();
        Quat4d rotation = new Quat4d();
        Vector3d scale = new Vector3d();
        MathUtil.decompose(bone.transform, position, rotation, scale);

        Point3 ddfpos = MathUtil.vecmathToDDF(new Point3d(position));
        b.setPosition(ddfpos);

        Quat ddfrot = MathUtil.vecmathToDDF(rotation);
        b.setRotation(ddfrot);

        Vector3 ddfscale = MathUtil.vecmathToDDF(scale);
        b.setScale(ddfscale);

        b.setLength(0.0f);
        b.setInheritScale(true);

        ddfBones.add(b.build());
    }

    public static void loadSkeleton(AIScene aiScene, com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder) throws IOException, LoaderException {
        ArrayList<ModelUtil.Bone> boneList = loadSkeleton(aiScene);

        // Generate DDF representation of bones.
        ArrayList<Rig.Bone> ddfBones = new ArrayList<Rig.Bone>();
        for (ModelUtil.Bone bone : boneList) {
            boneToDDF(bone, ddfBones);
        }
        skeletonBuilder.addAllBones(ddfBones);
    }


/*

    // Finds first skin in the controllers entry.
    private static XMLSkin findFirstSkin(XMLLibraryControllers controllers) {
        for(XMLController controller : controllers.controllers.values()) {
            if(controller.skin != null) {
                return controller.skin;
            }
        }
        return null;
    }

    private static int loadVertexWeights(XMLCOLLADA collada, List<Float> boneWeightsList, List<Integer> boneIndicesList) throws IOException, LoaderException {

        XMLSkin skin = null;
        if (!collada.libraryControllers.isEmpty()) {
            skin = findFirstSkin(collada.libraryControllers.get(0));
        }
        if(skin == null) {
            return 0;
        }

        List<XMLSource> sources = skin.sources;
        HashMap<String, XMLSource> sourcesMap = getSourcesMap(sources);

        XMLInput weights_input = findInput(skin.vertexWeights.inputs, "WEIGHT", true);
        XMLSource weightsSource = sourcesMap.get(weights_input.source);
        Vector<Weight> weights = new Vector<Weight>(10);
        int maxBoneCount = 0;

        int vIndex = 0;
        for ( int i = 0; i < skin.vertexWeights.vcount.ints.length; i++ )
        {
            int influenceCount = skin.vertexWeights.vcount.ints[ i ];
            int j = 0;
            weights.setSize(0);
            for (; j < influenceCount; j++ ) {
                float bw = 0f;
                int bi = 0;
                bi = skin.vertexWeights.v.ints[ vIndex + j * 2 + 0 ];
                if (bi != -1) {
                    final int weightIndex = skin.vertexWeights.v.ints[ vIndex + j * 2 + 1 ];
                    bw = weightsSource.floatArray.floats[ weightIndex ];
                    weights.add(new Weight(bi, bw));
                } else {
                    throw new LoaderException("Invalid bone index when loading vertex weights.");
                }
            }

            vIndex += influenceCount * 2;

            // Skinning in engine expect each vertex to have exactly 4 bone influences.
            for (; j < 4; j++ ) {
                weights.add(new Weight(0, 0.0f));
            }

            // Sort and take only the 4 influences with highest weight.
            Collections.sort(weights);
            weights.setSize(Math.min(4, weights.size()));
            influenceCount = weights.size();

            for (Weight w : weights) {
                boneIndicesList.add(w.boneIndex);
                maxBoneCount = Math.max(maxBoneCount, w.boneIndex + 1);
                boneWeightsList.add(w.weight);
            }
        }
        return maxBoneCount;
    }
*/

}
