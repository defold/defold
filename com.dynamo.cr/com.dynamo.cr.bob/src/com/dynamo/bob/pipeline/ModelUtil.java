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

    public static Scene loadScene(byte[] content, String path, Options options) {
        if (options == null)
            options = new Options();
        return ModelImporter.LoadFromBuffer(options, path, content);
    }

    public static Scene loadScene(InputStream stream, String path, Options options) throws IOException {
        byte[] bytes = IOUtils.toByteArray(stream);
        return loadScene(bytes, path, options);
    }

    public static void unloadScene(Scene scene) {
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

    private static float EPSILON = 0.00001f;

    private static boolean AlmostEqual(float v, float limit, float epsilon)
    {
        v = (v - limit);
        if (v < 0)
            v = -v;
        return v < epsilon;
    }

    private static boolean IsIdentityPos(ModelImporter.KeyFrame keyFrame)
    {
        return AlmostEqual(keyFrame.value[0], 0.0f, EPSILON) && AlmostEqual(keyFrame.value[1], 0.0f, EPSILON) && AlmostEqual(keyFrame.value[2], 0.0f, EPSILON);
    }

    private static boolean IsIdentityScale(ModelImporter.KeyFrame keyFrame)
    {
        return AlmostEqual(keyFrame.value[0], 1.0f, EPSILON) && AlmostEqual(keyFrame.value[1], 1.0f, EPSILON) && AlmostEqual(keyFrame.value[2], 1.0f, EPSILON);
    }

    private static boolean IsIdentityRotation(ModelImporter.KeyFrame keyFrame)
    {
        return AlmostEqual(keyFrame.value[0], 0.0f, EPSILON) && AlmostEqual(keyFrame.value[1], 0.0f, EPSILON) &&
                AlmostEqual(keyFrame.value[2], 0.0f, EPSILON) && AlmostEqual(keyFrame.value[3], 1.0f, EPSILON);
    }

    public static void loadAnimationTracks(Rig.RigAnimation.Builder animBuilder, ModelImporter.NodeAnimation nodeAnimation, ModelImporter.Bone bone, double duration, double startTime, double sampleRate) {
        double spf = 1.0 / sampleRate;

        if (nodeAnimation.translationKeys.length > 0) {

            if (nodeAnimation.translationKeys.length == 1 && IsIdentityPos(nodeAnimation.translationKeys[0])) {
                // pass
            } else {
                RigUtil.AnimationTrack sparseTrack = new RigUtil.AnimationTrack();
                sparseTrack.property = RigUtil.AnimationTrack.Property.POSITION;
                copyKeys(nodeAnimation.translationKeys, 3, sparseTrack.keys);

                samplePosTrack(animBuilder, sparseTrack, bone.index, duration, startTime, sampleRate, spf, true);
            }
        }

        if (nodeAnimation.rotationKeys.length > 0) {

            if (nodeAnimation.rotationKeys.length == 1 && IsIdentityRotation(nodeAnimation.rotationKeys[0])) {
                // pass
            } else {
                RigUtil.AnimationTrack sparseTrack = new RigUtil.AnimationTrack();
                sparseTrack.property = RigUtil.AnimationTrack.Property.ROTATION;
                copyKeys(nodeAnimation.rotationKeys, 4, sparseTrack.keys);

                sampleRotTrack(animBuilder, sparseTrack, bone.index, duration, startTime, sampleRate, spf, true);
            }
        }

        if (nodeAnimation.scaleKeys.length > 0) {

            if (nodeAnimation.scaleKeys.length == 1 && IsIdentityScale(nodeAnimation.scaleKeys[0])) {
                // pass
            } else {
                RigUtil.AnimationTrack sparseTrack = new RigUtil.AnimationTrack();
                sparseTrack.property = RigUtil.AnimationTrack.Property.SCALE;
                copyKeys(nodeAnimation.scaleKeys, 3, sparseTrack.keys);

                sampleScaleTrack(animBuilder, sparseTrack, bone.index, duration, startTime, sampleRate, spf, true);
            }
        }
    }

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
            if ( (bone.node != null && bone.node.name.equals(node.name)) || bone.name.equals(node.name)) {
                return bone;
            }
        }
        return null;
    }

    public static void loadAnimations(Scene scene, ArrayList<ModelImporter.Bone> bones, Rig.AnimationSet.Builder animationSetBuilder, String parentAnimationId, ArrayList<String> animationIds) {
        for (ModelImporter.Animation animation : scene.animations) {

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
            for (ModelImporter.NodeAnimation nodeAnimation : animation.nodeAnimations) {

                int count = 0;
                ModelImporter.KeyFrame keys[] = null;

                if (nodeAnimation.translationKeys.length > 0) {
                    keys = nodeAnimation.translationKeys;
                    count = nodeAnimation.translationKeys.length;
                }

                if (nodeAnimation.rotationKeys.length > 0) {
                    keys = nodeAnimation.rotationKeys;
                    count = nodeAnimation.rotationKeys.length;
                }

                if (nodeAnimation.scaleKeys.length > 0) {
                    keys = nodeAnimation.scaleKeys;
                    count = nodeAnimation.scaleKeys.length;
                }

                if (keys == null) {
                    System.err.printf("Animation contains no keys!");
                    return;
                }

                if (nodeAnimation.startTime < startTime)
                    startTime = nodeAnimation.startTime;
            }

            animBuilder.setDuration(animation.duration);

    System.out.printf("ANIMATION: %s  dur: %f  sampleRate: %f  startTime: %f\n", animation.name, animation.duration, sampleRate, startTime);

            for (ModelImporter.NodeAnimation nodeAnimation : animation.nodeAnimations) {

                Bone bone = findBone(bones, nodeAnimation.node);
                assert(bone != null);

                loadAnimationTracks(animBuilder, nodeAnimation, bone, animation.duration, startTime, sampleRate);
            }

            animationSetBuilder.addAnimations(animBuilder.build());
        }

        ModelUtil.setBoneList(animationSetBuilder, bones);
    }

    public static ArrayList<String> loadMaterialNames(Scene scene) {
        ArrayList<String> materials = new ArrayList<>();

        for (Model model : scene.models) {
            for (Mesh mesh : model.meshes) {
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

        float[] positions = mesh.positions;
        float[] normals = mesh.normals;
        float[] tangents = mesh.tangents;
        float[] colors = mesh.colors;
        //float[] weights = mesh.weights;
        //int[] bones = mesh.bones;
        float[] texCoords0 = mesh.getTexCoords(0);
        float[] texCoords1 = mesh.getTexCoords(1);

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

        int[] indices = mesh.indices;
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

        if (mesh.weights != null) {
            List<Float> weights_list = new ArrayList<Float>(mesh.weights.length);
            for (int i = 0; i < mesh.weights.length; ++i) {
                weights_list.add(mesh.weights[i]);
            }
            meshBuilder.addAllWeights(weights_list);
        }
        if (mesh.bones != null) {
            meshBuilder.addAllBoneIndices(()->Arrays.stream(mesh.bones).iterator());
        }

        return meshBuilder.build();
    }

    private static Rig.Model loadModel(Node node, Model model, ArrayList<ModelImporter.Bone> skeleton, ArrayList<String> materials) {

        Rig.Model.Builder modelBuilder = Rig.Model.newBuilder();

        for (Mesh mesh : model.meshes) {
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

        for (Node child : node.children) {
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
        for (Node root : scene.rootNodes) {
            if (root.model == null)
                continue;

            loadModelInstances(root, skeleton, materials, models);
            break; // TODO: Support more than one root node
        }

        meshSetBuilder.addAllModels(models);
        meshSetBuilder.setMaxBoneCount(skeleton.size());

        for (ModelImporter.Bone bone : skeleton) {
            meshSetBuilder.addBoneList(MurmurHash.hash64(bone.name));
        }
    }

    public static int getNumSkins(Scene scene) {
        return scene.skins.length;
    }

    public static ArrayList<ModelImporter.Bone> loadSkeleton(Scene scene) {
        ArrayList<ModelImporter.Bone> skeleton = new ArrayList<>();

        if (scene.skins.length == 0)
        {
            return skeleton;
        }

        // get the first skeleton
        ModelImporter.Skin skin = scene.skins[0];
        for (Bone bone : skin.bones) {
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

        b.setParent((bone.parent != null) ? bone.parent.index : -1);
        b.setId(MurmurHash.hash64(bone.name));
        b.setName(bone.name);

        b.setLength(0.0f);
        b.setInheritScale(true);

        ModelImporter.Transform transform = new ModelImporter.Transform();
        if (bone.node != null) {
            transform = bone.node.transform;
        }
        else {
            transform = new ModelImporter.Transform();
            transform.setIdentity();
        }
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
            InputStream is = new FileInputStream(file);
            scene = loadScene(is, file.getPath(), new ModelImporter.Options());
        } catch (Exception e) {
            System.out.printf("Failed reading '%s':\n%s\n", file, e.getMessage());
            return;
        }

        if (scene == null){
            System.out.printf("Failed to load '%s'\n", file);
            return;
        }


        // System.out.printf("Scene Root Transform:\n");
        // printMatrix4x4(scene.mRootNode().mName().dataString(), scene.mRootNode().mTransformation());
        // System.out.printf("\n");

        System.out.printf("--------------------------------------------\n");

        System.out.printf("Bones:\n");

        ArrayList<ModelImporter.Bone> bones = loadSkeleton(scene);
        for (Bone bone : bones) {
            System.out.printf("  Bone: %s  index: %d  parent: %s\n", bone.name, bone.index, bone.parent != null ? bone.parent.name : "");
        }
        System.out.printf("--------------------------------------------\n");

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

        Rig.AnimationSet.Builder animationSetBuilder = Rig.AnimationSet.newBuilder();
        ArrayList<String> animationIds = new ArrayList<>();
        loadAnimations(scene, bones, animationSetBuilder, file.getName(), animationIds);

    }

}
