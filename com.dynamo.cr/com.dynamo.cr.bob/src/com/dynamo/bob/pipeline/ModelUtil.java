// Copyright 2020-2024 The Defold Foundation
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


// Reference material:
// https://github.com/OGRECave/ogre/blob/ce7e29fedaa37f3380af4660b3629653c2963cd9/OgreMain/src/OgreSkeleton.cpp
// Retargeting: https://github.com/smix8/GodotAnimationRetargeting/blob/godot_3.x_gdscript/addons/animation_retargeting/AnimationRetargeting.gd

package com.dynamo.bob.pipeline;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.FileInputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;

import org.apache.commons.io.IOUtils;
import org.apache.commons.lang3.ArrayUtils;

import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;
import javax.vecmath.Tuple4d;
import javax.vecmath.Vector3d;

import com.dynamo.bob.util.MathUtil;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.RigUtil;
import com.dynamo.bob.util.RigUtil.AnimationKey;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.proto.DdfMath.Transform;

import com.dynamo.bob.pipeline.ModelImporter.Bone;
import com.dynamo.bob.pipeline.ModelImporter.Material;
import com.dynamo.bob.pipeline.ModelImporter.Mesh;
import com.dynamo.bob.pipeline.ModelImporter.Aabb;
import com.dynamo.bob.pipeline.ModelImporter.Model;
import com.dynamo.bob.pipeline.ModelImporter.Node;
import com.dynamo.bob.pipeline.ModelImporter.Options;
import com.dynamo.bob.pipeline.ModelImporter.Scene;


import com.dynamo.rig.proto.Rig;
import com.google.protobuf.ByteString;

public class ModelUtil {

    private static final int MAX_SPLIT_VCOUNT = 65535;

    public static Scene loadScene(byte[] content, String path, Options options, ModelImporter.DataResolver dataResolver) throws IOException {
        if (options == null)
            options = new Options();

        Scene scene = ModelImporter.LoadFromBuffer(options, path, content, dataResolver);

        for (ModelImporter.Buffer buffer : scene.buffers)
        {
            if (buffer.buffer == null)
                throw new IOException(String.format("Failed to load buffer '%s' for file '%s", buffer.uri, path));
        }

        if (scene != null)
            return loadInternal(scene, options);

        return scene;
    }

    public static Scene loadScene(InputStream stream, String path, Options options, ModelImporter.DataResolver dataResolver) throws IOException {
        byte[] bytes = IOUtils.toByteArray(stream);
        return loadScene(bytes, path, options, dataResolver);
    }

    public static void unloadScene(Scene scene) {
    }

    private static Vector3 toDDFVector3(ModelImporter.Vec4 v) {
        return MathUtil.vecmathToDDF(new Vector3d(v.x, v.y, v.z));
    }

    private static Transform toDDFTransform(ModelImporter.Transform transform) {
        Vector3d translation = new Vector3d(transform.translation.x, transform.translation.y, transform.translation.z);
        Vector3d scale = new Vector3d(transform.scale.x, transform.scale.y, transform.scale.z);
        Quat4d rotation = new Quat4d(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
        return MathUtil.vecmathToDDF(translation, rotation, scale);
    }

    // private static Point3d toPoint3d(ModelImporter.Vec4 v) {
    //     return new Point3d(v.x, v.y, v.z);
    // }

    // private static Vector3d toVector3d(ModelImporter.Vec4 v) {
    //     return new Vector3d(v.x, v.y, v.z);
    // }

    // private static Quat4d toQuat4d(ModelImporter.Vec4 v) {
    //     return new Quat4d(v.x, v.y, v.z, v.w);
    // }

    // private static Matrix4d toMatrix4d(ModelImporter.Transform transform) {
    //     Matrix4d t = new Matrix4d();
    //     t.setIdentity();
    //     t.setTranslation(new Vector3d(transform.translation.x, transform.translation.y, transform.translation.z));
    //     Matrix4d r = new Matrix4d();
    //     r.setIdentity();
    //     r.setRotation(new Quat4d(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w));
    //     Matrix4d s = new Matrix4d(transform.scale.x, 0, 0, 0,
    //                               0, transform.scale.y, 0, 0,
    //                               0, 0, transform.scale.z, 0,
    //                               0, 0, 0, 1);

    //     Matrix4d out = new Matrix4d();
    //     out.mul(r, s);
    //     out.mul(t);
    //     return out;
    // }

    // private static void printVector4d(Vector4d v) {
    //     System.out.printf("  %f, %f, %f, %f\n", v.getX(), v.getY(), v.getZ(), v.getW());
    // }

    // private static void printMatrix4d(Matrix4d mat) {
    //     Vector4d v = new Vector4d();
    //     mat.getColumn(0, v); printVector4d(v);
    //     mat.getColumn(1, v); printVector4d(v);
    //     mat.getColumn(2, v); printVector4d(v);
    //     mat.getColumn(3, v); printVector4d(v);
    // }

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

    private static void samplePosTrack(Rig.RigAnimation.Builder animBuilder, Rig.AnimationTrack.Builder animTrackBuilder, RigUtil.AnimationTrack track, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
        if (track.keys.isEmpty())
            return;

        RigUtil.PositionBuilder positionBuilder = new RigUtil.PositionBuilder(animTrackBuilder);
        RigUtil.sampleTrack(track, positionBuilder, startTime, duration, sampleRate, spf, true);
    }

    private static void sampleRotTrack(Rig.RigAnimation.Builder animBuilder, Rig.AnimationTrack.Builder animTrackBuilder, RigUtil.AnimationTrack track, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
        if (track.keys.isEmpty())
            return;

        RigUtil.QuatRotationBuilder rotationBuilder = new RigUtil.QuatRotationBuilder(animTrackBuilder);
        RigUtil.sampleTrack(track, rotationBuilder, startTime, duration, sampleRate, spf, true);
    }

    private static void sampleScaleTrack(Rig.RigAnimation.Builder animBuilder, Rig.AnimationTrack.Builder animTrackBuilder, RigUtil.AnimationTrack track, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
        if (track.keys.isEmpty())
            return;

        RigUtil.ScaleBuilder scaleBuilder = new RigUtil.ScaleBuilder(animTrackBuilder);
        RigUtil.sampleTrack(track, scaleBuilder, startTime, duration, sampleRate, spf, interpolate);
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

    public static void createAnimationTracks(Rig.RigAnimation.Builder animBuilder, ModelImporter.NodeAnimation nodeAnimation,
                                                    String bone_name, double duration, double startTime, double sampleRate) {
        double spf = 1.0 / sampleRate;

        Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
        animTrackBuilder.setBoneId(MurmurHash.hash64(bone_name));

        {
            RigUtil.AnimationTrack sparseTrack = new RigUtil.AnimationTrack();
            sparseTrack.property = RigUtil.AnimationTrack.Property.POSITION;
            copyKeys(nodeAnimation.translationKeys, 3, sparseTrack.keys);
            samplePosTrack(animBuilder, animTrackBuilder, sparseTrack, duration, startTime, sampleRate, spf, true);
        }
        {
            RigUtil.AnimationTrack sparseTrack = new RigUtil.AnimationTrack();
            sparseTrack.property = RigUtil.AnimationTrack.Property.ROTATION;
            copyKeys(nodeAnimation.rotationKeys, 4, sparseTrack.keys);

            sampleRotTrack(animBuilder, animTrackBuilder, sparseTrack, duration, startTime, sampleRate, spf, true);
        }
        {
            RigUtil.AnimationTrack sparseTrack = new RigUtil.AnimationTrack();
            sparseTrack.property = RigUtil.AnimationTrack.Property.SCALE;
            copyKeys(nodeAnimation.scaleKeys, 3, sparseTrack.keys);

            sampleScaleTrack(animBuilder, animTrackBuilder, sparseTrack, duration, startTime, sampleRate, spf, true);
        }

        animBuilder.addTracks(animTrackBuilder.build());
    }

    public static void loadAnimations(byte[] content, String suffix, ModelImporter.Options options, ModelImporter.DataResolver dataResolver,
                                        Rig.AnimationSet.Builder animationSetBuilder, String parentAnimationId, boolean selectLongest,
                                        ArrayList<String> animationIds) throws IOException {
        Scene scene = loadScene(content, suffix, options, dataResolver);
        loadAnimations(scene, animationSetBuilder, parentAnimationId, animationIds);
    }

    private static Bone findBoneByName(ArrayList<ModelImporter.Bone> bones, String name) {
        for (ModelImporter.Bone bone : bones) {
            if ( (bone.node != null && bone.node.name.equals(name)) || bone.name.equals(name)) {
                return bone;
            }
        }
        return null;
    }

    private static class SortAnimations implements Comparator<ModelImporter.Animation> {
        public int compare(ModelImporter.Animation a, ModelImporter.Animation b) {
            if (a.duration == b.duration)
                return 0;
            return (a.duration - b.duration) < 0 ? 1 : -1;
        }
    }

    public static void loadAnimations(Scene scene, Rig.AnimationSet.Builder animationSetBuilder,
                                      String parentAnimationId, ArrayList<String> animationIds) {

        ArrayList<ModelImporter.Bone> bones = loadSkeleton(scene);
        String prevRootName = null;
        if (!bones.isEmpty()) {
            prevRootName = bones.get(0).node.name;
        }

        boolean topLevel = parentAnimationId.isEmpty();
        boolean selectLongest = !topLevel;

        for (ModelImporter.Animation animation : scene.animations) {

            Rig.RigAnimation.Builder animBuilder = Rig.RigAnimation.newBuilder();

            // TODO: Retrieve this properly from either asset file or Options.
            // TODO: Do we even need this?
            float sampleRate = 30.0f;
            float startTime = 1000000.0f;

            animBuilder.setSampleRate(sampleRate);

            // If the model file was selected directly
            if (topLevel) {
                animBuilder.setId(MurmurHash.hash64(animation.name));
                animationIds.add(animation.name);
            }
            else {
                // For animation sets, each model file should be named after the model file itself
                // Each file is supposed to only have one animation
                // And the animation name is created from outside of this function, depending on the source
                // E.g. if the animation is from within nested .animationset files
                animBuilder.setId(MurmurHash.hash64(parentAnimationId));
                animationIds.add(parentAnimationId);
                selectLongest = true;
            }

            // TODO: add the start time to the Animation struct!
            for (ModelImporter.NodeAnimation nodeAnimation : animation.nodeAnimations) {

                boolean has_keys =  nodeAnimation.translationKeys.length > 0 ||
                                    nodeAnimation.rotationKeys.length > 0 ||
                                    nodeAnimation.scaleKeys.length > 0;

                if (!has_keys) {
                    System.err.printf("Animation %s contains no keys for node %s\n", animation.name, nodeAnimation.node.name);
                    continue;
                }

                if (nodeAnimation.startTime < startTime)
                    startTime = nodeAnimation.startTime;

                break;
            }

            animBuilder.setDuration(animation.duration);

            for (ModelImporter.NodeAnimation nodeAnimation : animation.nodeAnimations) {
                String nodeName = nodeAnimation.node.name;
                if (prevRootName != null && prevRootName.equals(nodeName)) {
                    nodeName = "root";
                }

                createAnimationTracks(animBuilder, nodeAnimation, nodeName, animation.duration, startTime, sampleRate);
            }

            animationSetBuilder.addAnimations(animBuilder.build());

            if (selectLongest)
            {
                break;
            }
        }
    }

    // For editor
    public static ArrayList<String> getAnimationNames(Scene scene) {
        ArrayList<String> names = new ArrayList<>();
        for (ModelImporter.Animation animation : scene.animations) {
            names.add(animation.name);
        }
        return names;
    }

    public static ArrayList<String> loadMaterialNames(Scene scene) {
        ArrayList<String> materials = new ArrayList<>();
        for (Material material : scene.materials) {
            materials.add(material.name);
        }
        return materials;
    }

    public static ByteBuffer create16BitIndices(int[] indices) {
        ByteBuffer indices_bytes;
        indices_bytes = ByteBuffer.allocateDirect(indices.length * 2);
        indices_bytes.order(ByteOrder.LITTLE_ENDIAN).asShortBuffer();
        for (int i = 0; i < indices.length; ++i) {
            indices_bytes.putShort((short)indices[i]);
        }
        indices_bytes.rewind();
        return indices_bytes;
    }

    public static ByteBuffer create32BitIndices(int[] indices) {
        ByteBuffer indices_bytes;
        indices_bytes = ByteBuffer.allocateDirect(indices.length * 4);
        indices_bytes.order(ByteOrder.LITTLE_ENDIAN).asIntBuffer();
        for (int i = 0; i < indices.length; ++i) {
            indices_bytes.putInt(indices[i]);
        }
        indices_bytes.rewind();
        return indices_bytes;
    }

    private static void copyFloatArray(float[] src, int srcIndex, float[] dst, int dstIndex, int num_components) {
        for (int i = 0; i < num_components; ++i) {
            dst[dstIndex*num_components+i] = src[srcIndex*num_components+i];
        }
    }

    private static void copyIntArray(int[] src, int srcIndex, int[] dst, int dstIndex, int num_components) {
        for (int i = 0; i < num_components; ++i) {
            dst[dstIndex*num_components+i] = src[srcIndex*num_components+i];
        }
    }

    private static void copyVertex(ModelImporter.Mesh inMesh, int inIndex, ModelImporter.Mesh outMesh, int outIndex) {
        if (inMesh.positions != null) {
            copyFloatArray(inMesh.positions, inIndex, outMesh.positions, outIndex, 3);
        }
        if (inMesh.normals != null) {
            copyFloatArray(inMesh.normals, inIndex, outMesh.normals, outIndex, 3);
        }
        if (inMesh.tangents != null) {
            copyFloatArray(inMesh.tangents, inIndex, outMesh.tangents, outIndex, 3);
        }
        if (inMesh.colors != null) {
            copyFloatArray(inMesh.colors, inIndex, outMesh.colors, outIndex, 4);
        }
        if (inMesh.weights != null) {
            copyFloatArray(inMesh.weights, inIndex, outMesh.weights, outIndex, 4);
        }
        if (inMesh.bones != null) {
            copyIntArray(inMesh.bones, inIndex, outMesh.bones, outIndex, 4);
        }
        if (inMesh.texCoords0 != null) {
            copyFloatArray(inMesh.texCoords0, inIndex, outMesh.texCoords0, outIndex, inMesh.texCoords0NumComponents);
        }
        if (inMesh.texCoords1 != null) {
            copyFloatArray(inMesh.texCoords1, inIndex, outMesh.texCoords1, outIndex, inMesh.texCoords1NumComponents);
        }
    }

    public static void splitMesh(ModelImporter.Mesh inMesh, List<ModelImporter.Mesh> outMeshes) {
        int triangleCount = inMesh.indexCount / 3;
        int vertexCount = inMesh.vertexCount;

        int vcount = 0;
        ModelImporter.Mesh newMesh = null;
        HashMap<Integer, Integer> oldToNewIndex = null;
        ArrayList<Integer> newIndices = null;

        for (int i = 0; i < triangleCount; ++i) {

            if (newMesh == null) {
                oldToNewIndex = new HashMap<Integer, Integer>();
                newIndices = new ArrayList<Integer>();

                newMesh = new Mesh();
                newMesh.material = inMesh.material;
                newMesh.name = String.format("%s_%d", inMesh.name, outMeshes.size());
                newMesh.aabb = new Aabb(inMesh.aabb);

                newMesh.texCoords0NumComponents = inMesh.texCoords0NumComponents;
                newMesh.texCoords1NumComponents = inMesh.texCoords1NumComponents;

                if (inMesh.positions != null)
                    newMesh.positions = new float[MAX_SPLIT_VCOUNT*3];
                if (inMesh.normals != null)
                    newMesh.normals = new float[MAX_SPLIT_VCOUNT*3];
                if (inMesh.tangents != null)
                    newMesh.tangents = new float[MAX_SPLIT_VCOUNT*3];
                if (inMesh.colors != null)
                    newMesh.colors = new float[MAX_SPLIT_VCOUNT * 4];
                if (inMesh.weights != null)
                    newMesh.weights = new float[MAX_SPLIT_VCOUNT * 4];
                if (inMesh.bones != null)
                    newMesh.bones = new int[MAX_SPLIT_VCOUNT * 4];
                if (inMesh.texCoords0 != null)
                    newMesh.texCoords0 = new float[MAX_SPLIT_VCOUNT*3];
                if (inMesh.texCoords1 != null)
                    newMesh.texCoords1 = new float[MAX_SPLIT_VCOUNT*3];
            }

            int index0 = inMesh.indices[i*3+0];
            int index1 = inMesh.indices[i*3+1];
            int index2 = inMesh.indices[i*3+2];

            int newIndex0 = oldToNewIndex.getOrDefault(index0, -1);
            int newIndex1 = oldToNewIndex.getOrDefault(index1, -1);
            int newIndex2 = oldToNewIndex.getOrDefault(index2, -1);

            if (newIndex0 == -1) {
                newIndex0 = vcount++;
                oldToNewIndex.put(index0, newIndex0);
                copyVertex(inMesh, index0, newMesh, newIndex0);
            }

            if (newIndex1 == -1) {
                newIndex1 = vcount++;
                oldToNewIndex.put(index1, newIndex1);
                copyVertex(inMesh, index1, newMesh, newIndex1);
            }

            if (newIndex2 == -1) {
                newIndex2 = vcount++;
                oldToNewIndex.put(index2, newIndex2);
                copyVertex(inMesh, index2, newMesh, newIndex2);
            }

            newIndices.add(newIndex0);
            newIndices.add(newIndex1);
            newIndices.add(newIndex2);

            // We need to make sure that we don't split a triangle into two different buffers
            boolean flush = (vcount+3) >= MAX_SPLIT_VCOUNT || ((i+1) == triangleCount);

            if (flush) {

                newMesh.indices = newIndices.stream().mapToInt(idx->idx).toArray();
                newMesh.indexCount = newIndices.size();
                newMesh.vertexCount = vcount;

                // Resize to actual size
                if (newMesh.positions != null)
                    newMesh.positions = Arrays.copyOf(newMesh.positions, vcount * 3);
                if (newMesh.normals != null)
                    newMesh.normals = Arrays.copyOf(newMesh.normals, vcount * 3);
                if (newMesh.tangents != null)
                    newMesh.tangents = Arrays.copyOf(newMesh.tangents, vcount * 3);
                if (newMesh.colors != null)
                    newMesh.colors = Arrays.copyOf(newMesh.colors, vcount * 4);
                if (newMesh.weights != null)
                    newMesh.weights = Arrays.copyOf(newMesh.weights, vcount * 4);
                if (newMesh.bones != null)
                    newMesh.bones = Arrays.copyOf(newMesh.bones, vcount * 4);
                if (newMesh.texCoords0 != null)
                    newMesh.texCoords0 = Arrays.copyOf(newMesh.texCoords0, vcount * newMesh.texCoords0NumComponents);
                if (newMesh.texCoords1 != null)
                    newMesh.texCoords1 = Arrays.copyOf(newMesh.texCoords1, vcount * newMesh.texCoords1NumComponents);

                outMeshes.add(newMesh);
                newMesh = null;
                vcount = 0;
            }
        }
    }

    private static void splitMeshes(Model model) {
        List<Mesh> outMeshes = new ArrayList<>();
        for (Mesh mesh : model.meshes) {
            if ((mesh.positions.length / 3) < MAX_SPLIT_VCOUNT) {
                outMeshes.add(mesh);
                continue;
            }

            List<Mesh> newMeshes = new ArrayList<>();
            splitMesh(mesh, newMeshes);
            outMeshes.addAll(newMeshes);
        }

        if (outMeshes.size() != model.meshes.length) {
            model.meshes = outMeshes.toArray(new ModelImporter.Mesh[0]);
        }
    }

    // Splits meshes that are have more than 65K+ vertices
    public static void splitMeshes(Scene scene) {
        for (Model model : scene.models) {
            splitMeshes(model);
        }
    }

    public static List<Integer> toList(int[] array) {
        return Arrays.asList(ArrayUtils.toObject(array));
    }

    public static List<Float> toList(float[] array) {
        return Arrays.asList(ArrayUtils.toObject(array));
    }

    public static Rig.Mesh loadMesh(Mesh mesh) {

        String name = mesh.name;

        Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();

        float[] positions = mesh.positions;
        float[] normals = mesh.normals;
        float[] texCoords0 = mesh.getTexCoords(0);
        float[] texCoords1 = mesh.getTexCoords(1);

        meshBuilder.setAabbMin(toDDFVector3(mesh.aabb.min));
        meshBuilder.setAabbMax(toDDFVector3(mesh.aabb.max));

        if (mesh.positions != null)
            meshBuilder.addAllPositions(toList(mesh.positions));

        if (mesh.normals != null)
            meshBuilder.addAllNormals(toList(mesh.normals));

        if (mesh.tangents != null)
            meshBuilder.addAllTangents(toList(mesh.tangents));

        if (mesh.colors != null)
            meshBuilder.addAllColors(toList(mesh.colors));

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

        if (mesh.getTexCoords(0) != null) {
            meshBuilder.addAllTexcoord0(toList(mesh.getTexCoords(0)));
            meshBuilder.setNumTexcoord0Components(mesh.texCoords0NumComponents);
        }
        if (mesh.getTexCoords(1) != null) {
            meshBuilder.addAllTexcoord1(toList(mesh.getTexCoords(1)));
            meshBuilder.setNumTexcoord0Components(mesh.texCoords1NumComponents);
        }

        if (mesh.vertexCount >= 65536) {
            meshBuilder.setIndicesFormat(Rig.IndexBufferFormat.INDEXBUFFER_FORMAT_32);
            meshBuilder.setIndices(ByteString.copyFrom(create32BitIndices(mesh.indices)));
        }
        else {
            meshBuilder.setIndicesFormat(Rig.IndexBufferFormat.INDEXBUFFER_FORMAT_16);
            meshBuilder.setIndices(ByteString.copyFrom(create16BitIndices(mesh.indices)));
        }

        if (mesh.material != null)
            meshBuilder.setMaterialIndex(mesh.material.index);
        else
            meshBuilder.setMaterialIndex(0x0); // We still need to assign a material at some point!

        return meshBuilder.build();
    }

    private static Rig.Model loadModel(Node node, Model model, ArrayList<ModelImporter.Bone> skeleton) {

        Rig.Model.Builder modelBuilder = Rig.Model.newBuilder();

        for (Mesh mesh : model.meshes) {
            modelBuilder.addMeshes(loadMesh(mesh));
        }

        modelBuilder.setId(MurmurHash.hash64(node.name)); // the node name is the human readable name (e.g Sword)
        modelBuilder.setLocal(toDDFTransform(node.local));
        modelBuilder.setBoneId(MurmurHash.hash64(model.boneParentName));

        return modelBuilder.build();
    }

    private static void loadModelInstances(Node node, ArrayList<ModelImporter.Bone> skeleton, ArrayList<Rig.Model> models) {

        if (node.model != null)
        {
            models.add(loadModel(node, node.model, skeleton));
        }

        for (Node child : node.children) {
            loadModelInstances(child, skeleton, models);
        }
    }

    private static void findModelNodes(Node node, List<Node> modelNodes) {
        if (node.model != null) {
            modelNodes.add(node);
        }
        for (Node child : node.children) {
            findModelNodes(child, modelNodes);
        }
    }

    private static void calcCenterNode(Node node, Aabb aabb) {
        if (node.model != null) {
            // As a default, we only count nodes with models, as the user
            // cannot currently see/use the lights or cameras etc that are present in the scene.
            aabb.expand(node.world.translation.x, node.world.translation.y, node.world.translation.z);
        }

        for (Node child : node.children) {
            calcCenterNode(child, aabb);
        }
    }

    // Currently finds the center point using the world positions of each node
    private static ModelImporter.Vec4 calcCenter(Scene scene) {
        Aabb aabb = new Aabb();
        for (Node root : scene.rootNodes) {
            calcCenterNode(root, aabb);
        }

        ModelImporter.Vec4 center = new ModelImporter.Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        if (aabb.isValid())
            center = aabb.center();
        return center;
    }

    private static void shiftNodes(Node node, ModelImporter.Vec4 center) {
        node.world.translation.x -= center.x;
        node.world.translation.y -= center.y;
        node.world.translation.z -= center.z;

        for (Node child : node.children) {
            shiftNodes(child, center);
        }
    }

    private static void shiftNodes(Scene scene, ModelImporter.Vec4 center) {
        for (Node node : scene.rootNodes) {
            shiftNodes(node, center);

            node.local.translation.x -= center.x;
            node.local.translation.y -= center.y;
            node.local.translation.z -= center.z;
        }
    }

    private static Scene loadInternal(Scene scene, Options options) {
        ModelImporter.Vec4 center = calcCenter(scene);
        shiftNodes(scene, center); // We might make this optional

        // Sort on duration. This allows us to return a list of sorted animation names
        Arrays.sort(scene.animations, new SortAnimations());
        return scene;
    }


    public static void loadModels(Scene scene, Rig.MeshSet.Builder meshSetBuilder) {
        ArrayList<ModelImporter.Bone> skeleton = loadSkeleton(scene);

        meshSetBuilder.addAllMaterials(loadMaterialNames(scene));

        ArrayList<Rig.Model> models = new ArrayList<>();
        for (Node root : scene.rootNodes) {
            ArrayList<Node> modelNodes = new ArrayList<>();
            findModelNodes(root, modelNodes);

            for (Node modelNode : modelNodes) {
                loadModelInstances(modelNode, skeleton, models);
            }
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

    public static int getNumAnimations(Scene scene) {
        return scene.animations.length;
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
            if (bone.index == 0) {
                bone.name = "root";
            }
            skeleton.add(bone);
        }

        return skeleton;
    }

    public static ArrayList<ModelImporter.Bone> loadSkeleton(byte[] content, String suffix, Options options, ModelImporter.DataResolver dataResolver) throws IOException {
        Scene scene = loadScene(content, suffix, options, dataResolver);
        return loadSkeleton(scene);
    }

    // Generate skeleton DDF data of bones.
    // It will extract the position, rotation and scale from the bone transform as needed by the runtime.
    private static void boneToDDF(ModelImporter.Bone bone, ArrayList<Rig.Bone> ddfBones) {
        Rig.Bone.Builder b = com.dynamo.rig.proto.Rig.Bone.newBuilder();

        int parentIndex = (bone.parent != null) ? bone.parent.index : -1;
        b.setParent(parentIndex);
        b.setId(MurmurHash.hash64(bone.name));
        b.setName(bone.name);

        b.setLength(0.0f);

        if (bone.node == null) {
            Transform identityTransform = MathUtil.vecmathIdentityTransform();
            b.setLocal(identityTransform);
            b.setWorld(identityTransform);
        } else {
            Transform ddfLocal = toDDFTransform(bone.node.local);
            b.setLocal(ddfLocal);

            Transform ddfWorld = toDDFTransform(bone.node.world);
            b.setWorld(ddfWorld);
        }

        Transform ddfBind = toDDFTransform(bone.invBindPose);
        b.setInverseBindPose(ddfBind);

        ddfBones.add(b.build());
    }

    public static boolean loadSkeleton(Scene scene, com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder) {
        ArrayList<Bone> boneList = loadSkeleton(scene);

        // Generate DDF representation of bones.
        ArrayList<Rig.Bone> ddfBones = new ArrayList<Rig.Bone>();
        for (Bone bone : boneList) {
            if (bone.index == 0) {
                bone.name = "root";
            }

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

    // For editor in a migration period
    public static ModelImporter.DataResolver createFileDataResolver(File cwd) {
        return new ModelImporter.FileDataResolver(cwd);
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
            long timeStart = System.currentTimeMillis();

            InputStream is = new FileInputStream(file);
            byte[] bytes = IOUtils.toByteArray(is);

            ModelImporter.FileDataResolver dataResolver = new ModelImporter.FileDataResolver();
            scene = loadScene(bytes, file.getPath(), new ModelImporter.Options(), dataResolver);

            // **********************************
            for (ModelImporter.Buffer buffer : scene.buffers) {
                if (buffer.buffer == null)
                {
                    System.out.printf("Unresolved buffer: %s\n");
                }
            }
            // **********************************


            long timeEnd = System.currentTimeMillis();
            System.out.printf("Loading took %d ms\n", (timeEnd - timeStart));

        } catch (Exception e) {
            System.out.printf("Failed reading '%s':\n%s\n", file, e.getMessage());
            return;
        }

        if (scene == null){
            System.out.printf("Failed to load '%s'\n", file);
            return;
        }

        System.out.printf("--------------------------------------------\n");
        System.out.printf("Scene Nodes:\n");

        for (Node node : scene.nodes) {
            System.out.printf("  Scene Node: %s  index: %d  id: %d  parent: %s\n", node.name, node.index, ModelImporter.AddressOf(node), node.parent != null ? node.parent.name : "");
            System.out.printf("      local: id: %d\n", ModelImporter.AddressOf(node.local));
            ModelImporter.DebugPrintTransform(node.local, 3);
        }


        if (scene.skins.length > 0)
        {
            System.out.printf("--------------------------------------------\n");
            System.out.printf("Scene Bones:\n");

            int bone_count = 0;
            for (Bone bone : scene.skins[0].bones) {
                System.out.printf("  Scene Bone %d: %s  index: %d  id: %d  nodeid: %d  parent: %s\n", bone_count++, bone.name, bone.index,
                                            ModelImporter.AddressOf(bone), ModelImporter.AddressOf(bone.node),
                                            bone.parent != null ? bone.parent.name : "");
                System.out.printf("      local: id: %d\n", ModelImporter.AddressOf(bone.node.local));
                ModelImporter.DebugPrintTransform(bone.node.local, 3);
                System.out.printf("      inv_bind_poser:\n");
                ModelImporter.DebugPrintTransform(bone.invBindPose, 3);
            }

            System.out.printf("--------------------------------------------\n");
        }

        System.out.printf("Bones:\n");

        ArrayList<ModelImporter.Bone> bones = loadSkeleton(scene);
        for (Bone bone : bones) {
            System.out.printf("  Bone: %s  index: %d  parent: %s\n", bone.name, bone.index, bone.parent != null ? bone.parent.name : "");
            System.out.printf("      local:\n");
            ModelImporter.DebugPrintTransform(bone.node.local, 3);
        }
        System.out.printf("--------------------------------------------\n");

        System.out.printf("Root Nodes:\n");

        for (Node node : scene.rootNodes) {
            System.out.printf("  Scene Node: %s  index: %d  id: %d  parent: %s\n", node.name, node.index, ModelImporter.AddressOf(node), node.parent != null ? node.parent.name : "");
            System.out.printf("      local: id: %d\n", ModelImporter.AddressOf(node.local));
            ModelImporter.DebugPrintTransform(node.local, 3);
        }

        System.out.printf("--------------------------------------------\n");

        System.out.printf("Materials:\n");
        for (Material material : scene.materials) {
            System.out.printf("  Material: %s\n", material.name, material.index);
        }
        System.out.printf("--------------------------------------------\n");

        Rig.MeshSet.Builder meshSetBuilder = Rig.MeshSet.newBuilder();
        loadModels(scene, meshSetBuilder); // testing the function

        Rig.Skeleton.Builder skeletonBuilder = Rig.Skeleton.newBuilder();
        loadSkeleton(scene, skeletonBuilder); // testing the function

        System.out.printf("Animations:\n");

        Rig.AnimationSet.Builder animationSetBuilder = Rig.AnimationSet.newBuilder();
        ArrayList<String> animationIds = new ArrayList<>();
        loadAnimations(scene, animationSetBuilder, "", animationIds);

        for (ModelImporter.Animation animation : scene.animations) {
            System.out.printf("  Animation: %s\n", animation.name);
        }
        System.out.printf("--------------------------------------------\n");

    }

}
