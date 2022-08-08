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


// Reference material:
// https://github.com/OGRECave/ogre/blob/ce7e29fedaa37f3380af4660b3629653c2963cd9/OgreMain/src/OgreSkeleton.cpp
// Retargeting: https://github.com/smix8/GodotAnimationRetargeting/blob/godot_3.x_gdscript/addons/animation_retargeting/AnimationRetargeting.gd

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
import java.util.Comparator;
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
        RigUtil.sampleTrack(track, positionBuilder, new Point3d(0.0, 0.0, 0.0), startTime, duration, sampleRate, spf, true);
    }

    private static void sampleRotTrack(Rig.RigAnimation.Builder animBuilder, Rig.AnimationTrack.Builder animTrackBuilder, RigUtil.AnimationTrack track, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
        if (track.keys.isEmpty())
            return;

        RigUtil.QuatRotationBuilder rotationBuilder = new RigUtil.QuatRotationBuilder(animTrackBuilder);
        RigUtil.sampleTrack(track, rotationBuilder, new Quat4d(0.0, 0.0, 0.0, 1.0), startTime, duration, sampleRate, spf, true);
    }

    private static void sampleScaleTrack(Rig.RigAnimation.Builder animBuilder, Rig.AnimationTrack.Builder animTrackBuilder, RigUtil.AnimationTrack track, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
        if (track.keys.isEmpty())
            return;

        RigUtil.ScaleBuilder scaleBuilder = new RigUtil.ScaleBuilder(animTrackBuilder);
        RigUtil.sampleTrack(track, scaleBuilder, new Vector3d(1.0, 1.0, 1.0), startTime, duration, sampleRate, spf, interpolate);
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
                                                    int boneIndex, double duration, double startTime, double sampleRate) {
        double spf = 1.0 / sampleRate;

        Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
        animTrackBuilder.setBoneIndex(boneIndex);

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

    public static void setBoneList(Rig.AnimationSet.Builder animationSetBuilder, ArrayList<Bone> bones) {
        for (Bone bone : bones) {
            animationSetBuilder.addBoneList(MurmurHash.hash64(bone.name));
        }
    }

    public static void loadAnimations(byte[] content, String suffix, ModelImporter.Options options, ArrayList<ModelImporter.Bone> bones, Rig.AnimationSet.Builder animationSetBuilder, String parentAnimationId, ArrayList<String> animationIds) {
        Scene scene = loadScene(content, suffix, options);
        loadAnimations(scene, bones, animationSetBuilder, parentAnimationId, animationIds);
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

    private static class SortOnNodeIndex implements Comparator<ModelImporter.NodeAnimation> {
        public SortOnNodeIndex(ArrayList<ModelImporter.Bone> bones) {
            this.bones = bones;
        }
        public int compare(ModelImporter.NodeAnimation a, ModelImporter.NodeAnimation b) {
            Bone bonea = findBoneByName(this.bones, a.node.name);
            Bone boneb = findBoneByName(this.bones, b.node.name);
            return bonea.index - boneb.index;
        }
        private ArrayList<ModelImporter.Bone> bones;
    }

    public static void loadAnimations(Scene scene, ArrayList<ModelImporter.Bone> fullSetBones, Rig.AnimationSet.Builder animationSetBuilder,
                                      String parentAnimationId, ArrayList<String> animationIds) {

        Arrays.sort(scene.animations, new SortAnimations());

        if (scene.animations.length > 1) {
            System.out.printf("Scene contains more than one animation. Picking the the longest one ('%s')\n", scene.animations[0].name);
        }

        // We want to use the internal skeleton from this scene to calculate the relative
        // animation keys
        ArrayList<ModelImporter.Bone> bones = ModelUtil.loadSkeleton(scene);

        for (ModelImporter.Animation animation : scene.animations) {

            Rig.RigAnimation.Builder animBuilder = Rig.RigAnimation.newBuilder();

            // TODO: Retrieve this properly from either asset file or Options.
            // TODO: Do we even need this?
            float sampleRate = 30.0f;
            float startTime = 1000000.0f;

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

            Arrays.sort(animation.nodeAnimations, new SortOnNodeIndex(fullSetBones));

            for (ModelImporter.NodeAnimation nodeAnimation : animation.nodeAnimations) {

                Bone skeleton_bone = findBoneByName(fullSetBones, nodeAnimation.node.name);
                Bone anim_bone = findBoneByName(bones, nodeAnimation.node.name);
                if (skeleton_bone == null) {
                    System.out.printf("Warning: Animation uses bone '%s' that isn't present in the skeleton!\n", nodeAnimation.node.name);
                    continue;
                }
                if (anim_bone == null) {
                    System.out.printf("Warning: Animation uses bone '%s' that isn't present in the animation file!\n", nodeAnimation.node.name);
                    continue;
                }

                createAnimationTracks(animBuilder, nodeAnimation, skeleton_bone.index, animation.duration, startTime, sampleRate);
            }

            animationSetBuilder.addAnimations(animBuilder.build());

            break; // we only support one animation per file
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
    }

    public static List<Integer> toList(int[] array) {
        return Arrays.asList(ArrayUtils.toObject(array));
    }

    public static List<Float> toList(float[] array) {
        return Arrays.asList(ArrayUtils.toObject(array));
    }

    public static Rig.Mesh loadMesh(Mesh mesh, ArrayList<ModelImporter.Bone> skeleton, ArrayList<String> materials) {

        String name = mesh.name;

        Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();

        float[] positions = mesh.positions;
        float[] normals = mesh.normals;
        float[] tangents = mesh.tangents;
        float[] colors = mesh.colors;
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
        modelBuilder.setLocal(toDDFTransform(node.local));

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

    private static Node findFirstModelNode(Node node) {
        if (node.model != null) {
            return node;
        }

        for (Node child : node.children) {
            Node modelNode = findFirstModelNode(child);
            if (modelNode != null) {
                return modelNode;
            }
        }

        return null;
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
            Node modelNode = findFirstModelNode(root);
            if (modelNode == null) {
                continue;
            }

            loadModelInstances(modelNode, skeleton, materials, models);
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

            for (Bone bone : scene.skins[0].bones) {
                System.out.printf("  Scene Bone: %s  index: %d  id: %d  nodeid: %d  parent: %s\n", bone.name, bone.index,
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

        System.out.printf("Animations:\n");

        Rig.AnimationSet.Builder animationSetBuilder = Rig.AnimationSet.newBuilder();
        ArrayList<String> animationIds = new ArrayList<>();
        loadAnimations(scene, bones, animationSetBuilder, file.getName(), animationIds);

        for (ModelImporter.Animation animation : scene.animations) {
            System.out.printf("  Animation: %s\n", animation.name);
        }
        System.out.printf("--------------------------------------------\n");

    }

}
