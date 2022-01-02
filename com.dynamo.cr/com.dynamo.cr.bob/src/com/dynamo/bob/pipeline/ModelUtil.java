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
        int         parentIndex;
        Matrix4d    transform;
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
            // {
            //     AINode scene_root = scene.mRootNode();
            //     AIMatrix4x4 mat = scene_root.mTransformation();

            //     printMatrix4x4(scene_root.mName().dataString(), mat);

            //     assetSpace.rotation.setRow(0, new double[] {mat.a1(), mat.a2(), mat.a3(), 0.0});
            //     assetSpace.rotation.setRow(1, new double[] {mat.b1(), mat.b2(), mat.b3(), 0.0});
            //     assetSpace.rotation.setRow(2, new double[] {mat.c1(), mat.c2(), mat.c3(), 0.0});
            //     assetSpace.rotation.transpose();
            // }


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


    // private static void ExtractMatrixKeys(Bone bone, Matrix4d localToParent, AssetSpace assetSpace, XMLAnimation animation, RigUtil.AnimationTrack posTrack, RigUtil.AnimationTrack rotTrack, RigUtil.AnimationTrack scaleTrack) {

    //     Vector3d bindP = new Vector3d();
    //     Quat4d bindR = new Quat4d();
    //     Vector3d bindS = new Vector3d();
    //     MathUtil.decompose(localToParent, bindP, bindR, bindS);
    //     bindR.inverse();
    //     Vector4d lastR = new Vector4d(0.0, 0.0, 0.0, 0.0);

    //     int keyCount = animation.getInput().length;
    //     float[] time = animation.getInput();
    //     float[] values = animation.getOutput();
    //     for (int key = 0; key < keyCount; ++key) {
    //         int index = key * 16;
    //         Matrix4d m = new Matrix4d(new Matrix4f(ArrayUtils.subarray(values, index, index + 16)));
    //         if (assetSpace != null) {
    //             m.m03 *= assetSpace.unit;
    //             m.m13 *= assetSpace.unit;
    //             m.m23 *= assetSpace.unit;
    //             m.mul(assetSpace.rotation, m);
    //         }

    //         Vector3d p = new Vector3d();
    //         Quat4d r = new Quat4d();
    //         Vector3d s = new Vector3d();
    //         MathUtil.decompose(m, p, r, s);

    //         // Check if dot product of decomposed rotation and previous frame is < 0,
    //         // if that is the case; flip rotation.
    //         // This is to avoid a problem that can occur when we decompose the matrix and
    //         // we get a quaternion representing the same rotation but in the opposite direction.
    //         // See this answer on SO: http://stackoverflow.com/a/2887128
    //         Vector4d rv = new Vector4d(r.x, r.y, r.z, r.w);
    //         if (lastR.dot(rv) < 0.0) {
    //             r.scale(-1.0);
    //             rv.scale(-1.0);
    //         }
    //         lastR = rv;

    //         // Get pose relative transform
    //         p.set(p.getX() - bindP.getX(), p.getY() - bindP.getY(), p.getZ() - bindP.getZ());
    //         r.mul(bindR, r);
    //         s.set(s.getX() / bindS.getX(), s.getY() / bindS.getY(), s.getZ() / bindS.getZ());

    //         float t = time[key];
    //         AnimationKey posKey = createKey(t, false, 3);
    //         toFloats(p, posKey.value);
    //         posTrack.keys.add(posKey);
    //         AnimationKey rotKey = createKey(t, false, 4);
    //         toFloats(r, rotKey.value);
    //         rotTrack.keys.add(rotKey);
    //         AnimationKey scaleKey = createKey(t, false, 3);
    //         toFloats(s, scaleKey.value);
    //         scaleTrack.keys.add(scaleKey);
    //     }
    // }

    // private static void ExtractKeys(Bone bone, Matrix4d localToParent, AssetSpace assetSpace, XMLAnimation animation, RigUtil.AnimationTrack posTrack, RigUtil.AnimationTrack rotTrack, RigUtil.AnimationTrack scaleTrack) throws LoaderException {
    //     switch (animation.getType()) {
    //     case TRANSLATE:
    //     case SCALE:
    //     case ROTATE:
    //         throw new LoaderException("Currently only collada files with matrix animations are supported.");
    //     case TRANSFORM:
    //     case MATRIX:
    //         ExtractMatrixKeys(bone, localToParent, assetSpace, animation, posTrack, rotTrack, scaleTrack);
    //         break;
    //     default:
    //         throw new LoaderException(String.format("Animations of type %s are not supported.", animation.getType().name()));
    //     }
    // }

    // private static Matrix4d getBoneLocalToParent(Bone bone) {
    //     return new Matrix4d(MathUtil.vecmath2ToVecmath1(bone.bindMatrix));
    // }

    private static void samplePosTrack(Rig.RigAnimation.Builder animBuilder, int boneIndex, RigUtil.AnimationTrack track, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
        if (!track.keys.isEmpty()) {
            Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
            animTrackBuilder.setBoneIndex(boneIndex);
            RigUtil.PositionBuilder positionBuilder = new RigUtil.PositionBuilder(animTrackBuilder);
            RigUtil.sampleTrack(track, positionBuilder, new Point3d(0.0, 0.0, 0.0), startTime, duration, sampleRate, spf, true);
            animBuilder.addTracks(animTrackBuilder.build());
        }
    }

    private static void sampleRotTrack(Rig.RigAnimation.Builder animBuilder, int boneIndex, RigUtil.AnimationTrack track, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
        if (!track.keys.isEmpty()) {
            Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
            animTrackBuilder.setBoneIndex(boneIndex);
            RigUtil.QuatRotationBuilder rotationBuilder = new RigUtil.QuatRotationBuilder(animTrackBuilder);
            RigUtil.sampleTrack(track, rotationBuilder, new Quat4d(0.0, 0.0, 0.0, 1.0), startTime, duration, sampleRate, spf, true);
            animBuilder.addTracks(animTrackBuilder.build());
        }
    }

    private static void sampleScaleTrack(Rig.RigAnimation.Builder animBuilder, int boneIndex, RigUtil.AnimationTrack track, double duration, double startTime, double sampleRate, double spf, boolean interpolate) {
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

    public interface ColladaResourceResolver {
        public InputStream getResource(String resourceName) throws FileNotFoundException;
    }
    private static void loadAnimationClipIds(InputStream is, String parentId, ArrayList<String> animationIds) throws IOException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        ArrayList<XMLLibraryAnimationClips> animClips = collada.libraryAnimationClips;
        if(animClips.isEmpty()) {
            if(!collada.libraryAnimations.isEmpty()) {
                animationIds.add(parentId);
            }
            return;
        }
        for (XMLAnimationClip clip : animClips.get(0).animationClips.values()) {
            if (clip.name != null) {
                animationIds.add(parentId + "/" + clip.name);
            } else if (clip.id != null) {
                animationIds.add(parentId + "/" + clip.id);
            } else {
                throw new LoaderException("Animation clip must contain name or id.");
            }
        }
    }

    public static void loadAnimationIds(String resourceName, String parentId, ArrayList<String> animationIds, final ColladaResourceResolver resourceResolver) throws IOException, LoaderException {
        InputStream is;
        try {
            is = resourceResolver.getResource(resourceName);
        } catch (FileNotFoundException e) {
            throw new IOException("Could not extract animation id from resource: " + resourceName);
        }
        String animId  = FilenameUtils.getBaseName(resourceName);
        if(resourceName.toLowerCase().endsWith(".dae")) {
            loadAnimationClipIds(is, animId, animationIds);
            return;
        }
        animId  = (parentId.isEmpty() ? "" : parentId + "/") + animId;
        InputStreamReader animset_isr = new InputStreamReader(is);
        AnimationSetDesc.Builder animationDescBuilder = AnimationSetDesc.newBuilder();
        TextFormat.merge(animset_isr, animationDescBuilder);

        for(AnimationInstanceDesc animationSet : animationDescBuilder.getAnimationsList()) {
            loadAnimationIds(animationSet.getAnimation(), animId, animationIds, resourceResolver);
        }
    }

*/

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

    public static void loadAnimations(byte[] content, String suffix, Rig.AnimationSet.Builder animationSetBuilder, String parentAnimationId, ArrayList<String> animationIds) throws IOException, LoaderException {
        AIScene aiScene = loadScene(content, suffix);
        loadAnimations(aiScene, animationSetBuilder, parentAnimationId, animationIds);
    }

    public static void loadAnimations(AIScene aiScene, Rig.AnimationSet.Builder animationSetBuilder, String parentAnimationId, ArrayList<String> animationIds) throws IOException, LoaderException {
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
        System.out.printf("ModelUtil: loadAnimations\n");

        // Load skeleton to get bone bind poses
        ArrayList<ModelUtil.Bone> boneList = loadSkeleton(aiScene);

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

    // public static void loadMesh(InputStream is, Rig.MeshSet.Builder meshSetBuilder) throws IOException, LoaderException {
    //     loadMesh(is, meshSetBuilder, false);
    // }

    // public static void loadMesh(InputStream is, Rig.MeshSet.Builder meshSetBuilder, boolean optimize) throws IOException, LoaderException {
    //     XMLCOLLADA collada = loadDAE(is);
    //     loadMesh(collada, meshSetBuilder, optimize);
    // }

    // private static XMLNode getFirstNodeWithGeoemtry(Collection<XMLVisualScene> scenes) {
    //     for (XMLVisualScene scene : scenes) {
    //         for (XMLNode node : scene.nodes.values()) {
    //             if (node.instanceGeometries.size() > 0) {
    //                 return node;
    //             }
    //         }
    //     }

    //     return null;
    // }

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

    public static Rig.Mesh loadMesh(AIMesh mesh, ArrayList<ModelUtil.Bone> skeleton) throws IOException, LoaderException {
        ArrayList<Float> positions = getVertexData(mesh.mVertices());
        ArrayList<Float> normals = getVertexData(mesh.mNormals());
        //ArrayList<Float> tangents = getVertexData(mesh.mTangents());
        //ArrayList<Float> bitangents = getVertexData(mesh.mBitangents());

        ArrayList<Float> texcoord0 = getVertexData(mesh.mTextureCoords(0));
        ArrayList<Float> texcoord1 = getVertexData(mesh.mTextureCoords(1));

        Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();

        if (positions != null)
            meshBuilder.addAllPositions(positions);

        if (normals != null)
            meshBuilder.addAllNormals(normals);

        if (texcoord0 != null) {
            meshBuilder.addAllTexcoord0(texcoord0);
            meshBuilder.setNumTexcoord0Components(mesh.mNumUVComponents(0));
        }
        if (texcoord1 != null) {
            meshBuilder.addAllTexcoord1(texcoord1);
            meshBuilder.setNumTexcoord0Components(mesh.mNumUVComponents(1));
        }

        loadWeights(mesh, skeleton, meshBuilder);

        return meshBuilder.build();
    }

    public static void loadMeshes(AIScene scene, Rig.MeshSet.Builder meshSetBuilder) throws IOException, LoaderException {

        // // Use first geometry entry as default
        // XMLGeometry geom = collada.libraryGeometries.get(0).geometries.values().iterator().next();

        // // Find first node in visual scene tree that has a instance geometry
        // XMLNode sceneNode = null;
        // Matrix4d sceneNodeMatrix = null;
        // if (collada.libraryVisualScenes.size() > 0) {
        //     sceneNode = getFirstNodeWithGeoemtry(collada.libraryVisualScenes.get(0).scenes.values());
        //     if (sceneNode != null) {
        //         XMLInstanceGeometry instanceGeo = sceneNode.instanceGeometries.get(0);
        //         String geometryId = instanceGeo.url;

        //         // Get node transform if available
        //         sceneNodeMatrix = new Matrix4d(MathUtil.vecmath2ToVecmath1(sceneNode.matrix.matrix4f));

        //         // Find geometry entry
        //         for (XMLGeometry geomEntry : collada.libraryGeometries.get(0).geometries.values())
        //         {
        //             if (geomEntry.equals(geometryId)) {
        //                 geom = geomEntry;
        //                 break;
        //             }
        //         }
        //     }
        // }
        // if (sceneNodeMatrix == null) {
        //     sceneNodeMatrix = new Matrix4d();
        //     sceneNodeMatrix.setIdentity();
        // }

        // XMLMesh mesh = geom.mesh;
        // if(mesh == null || mesh.triangles == null) {
        //     return;
        // }
        // List<XMLSource> sources = mesh.sources;
        // HashMap<String, XMLSource> sourcesMap = getSourcesMap(sources);

        ArrayList<ModelUtil.Bone> skeleton = loadSkeleton(scene);

        // XMLInput vpos_input = findInput(mesh.vertices.inputs, "POSITION", true);
        // XMLInput vertex_input = findInput(mesh.triangles.inputs, "VERTEX", true);
        // XMLInput normal_input = findInput(mesh.triangles.inputs, "NORMAL", false);
        // XMLInput texcoord_input = findInput(mesh.triangles.inputs, "TEXCOORD", false);

        // Get bind shape from skin, if it exist
        // Matrix4d bindShapeMatrix = null;
        // XMLSkin skin = null;
        // if (!collada.libraryControllers.isEmpty()) {
        //     skin = findFirstSkin(collada.libraryControllers.get(0));
        //     if (skin != null && skin.bindShapeMatrix != null) {
        //         bindShapeMatrix = new Matrix4d(MathUtil.vecmath2ToVecmath1(skin.bindShapeMatrix.matrix4f));
        //     }
        // }
        // if (bindShapeMatrix == null) {
        //     bindShapeMatrix = new Matrix4d();
        //     bindShapeMatrix.setIdentity();
        // }

        // // Apply any matrix found for scene node
        // bindShapeMatrix.mul(sceneNodeMatrix);

        // // NOTE: Normals could be part of the vertex of specified directly in triangles...
        // int normalOffset;
        // if (normal_input  != null) {
        //     normalOffset = normal_input.offset;
        // } else {
        //     normalOffset = 0;
        //     normal_input = findInput(mesh.vertices.inputs, "NORMAL", false);
        // }

        // if (mesh.triangles.inputs.size() == 0)
        //     throw new LoaderException("No inputs in triangles");

        // int stride = 0;
        // for (XMLInput i : mesh.triangles.inputs) {
        //     stride = Math.max(stride, i.offset);
        // }
        // stride += 1;

        // XMLSource positions = sourcesMap.get(vpos_input.source);
        // XMLSource normals = null;
        // if (normal_input != null) {
        //     normals = sourcesMap.get(normal_input.source);
        // }

        // XMLSource texcoords = null;
        // if (texcoord_input != null) {
        //     texcoords = sourcesMap.get(texcoord_input.source);
        // }

        // // Apply the up-axis matrix on the bind shape matrix.
        // AssetSpace assetSpace = getAssetSpace(collada.asset);
        // Matrix4d assetSpaceMtx = new Matrix4d();
        // Matrix4d assetScaleMtx = new Matrix4d();
        // assetScaleMtx.setIdentity();
        // assetScaleMtx.setScale(assetSpace.unit);
        // assetSpaceMtx.mul(assetSpace.rotation, assetScaleMtx);
        // bindShapeMatrix.mul(assetSpaceMtx, bindShapeMatrix);

        // List<Float> position_list = new ArrayList<Float>(positions.floatArray.count);
        // for (int i = 0; i < positions.floatArray.count / 3; ++i) {
        //     Point3f p = new Point3f(positions.floatArray.floats[i*3], positions.floatArray.floats[i*3+1], positions.floatArray.floats[i*3+2]);
        //     bindShapeMatrix.transform(p);
        //     position_list.add(p.getX());
        //     position_list.add(p.getY());
        //     position_list.add(p.getZ());
        // }

        // // Create a normal matrix which is the transposed inverse of
        // // the bind shape matrix (which already include the up axis matrix).
        // Matrix4f normalMatrix = new Matrix4f(bindShapeMatrix);
        // normalMatrix.invert();
        // normalMatrix.transpose();

        // List<Float> normal_list = null;
        // if(normals != null) {
        //     normal_list = new ArrayList<Float>(normals.floatArray.count);
        //     for (int i = 0; i < normals.floatArray.count / 3; ++i) {
        //         Vector3f n = new Vector3f(normals.floatArray.floats[i*3], normals.floatArray.floats[i*3+1], normals.floatArray.floats[i*3+2]);
        //         normalMatrix.transform(n);
        //         if (n.lengthSquared() > 0.0) {
        //             n.normalize();
        //         }
        //         normal_list.add(n.getX());
        //         normal_list.add(n.getY());
        //         normal_list.add(n.getZ());
        //     }
        // }

        // List<Float> texcoord_list;
        // if(texcoords == null) {
        //     texcoord_list = new ArrayList<Float>(Arrays.asList(0f, 0f));
        // } else {
        //     texcoord_list = new ArrayList<Float>(texcoords.floatArray.count);
        //     for (int i = 0; i < texcoords.floatArray.count; i += 2 ) {
        //         texcoord_list.add(texcoords.floatArray.floats[i]);
        //         texcoord_list.add(texcoords.floatArray.floats[i+1]);
        //     }
        // }

        // List<Integer> position_indices_list = new ArrayList<Integer>(mesh.triangles.count*3);
        // List<Integer> normal_indices_list = new ArrayList<Integer>(mesh.triangles.count*3);
        // List<Integer> texcoord_indices_list = new ArrayList<Integer>(mesh.triangles.count*3);

        // // Sometimes the <p> values can be -1 from Maya exports, we clamp it below to 0 instead.
        // // Similar solution as AssImp; https://github.com/assimp/assimp/blob/master/code/ColladaParser.cpp#L2336
        // for (int i = 0; i < mesh.triangles.count; ++i) {

        //     for (int j = 0; j < 3; ++j) {
        //         int idx = i * stride * 3 + vertex_input.offset;
        //         int vert_idx = Math.max(0, mesh.triangles.p[idx + stride * j]);
        //         position_indices_list.add(vert_idx);

        //         if (normals != null) {
        //             idx = i * stride * 3 + normalOffset;
        //             vert_idx = Math.max(0, mesh.triangles.p[idx + stride * j]);
        //             normal_indices_list.add(vert_idx);
        //         }

        //         if (texcoords == null) {
        //             texcoord_indices_list.add(0);
        //         } else {
        //             idx = i * stride * 3 + texcoord_input.offset;
        //             vert_idx = Math.max(0, mesh.triangles.p[idx + stride * j]);
        //             texcoord_indices_list.add(vert_idx);
        //         }

        //     }

        // }

        // class MeshVertexIndex {
        //     public int position, texcoord0, normal;
        //     public boolean equals(Object o) {
        //         MeshVertexIndex m = (MeshVertexIndex) o;
        //         return (this.position == m.position && this.texcoord0 == m.texcoord0 && this.normal == m.normal);
        //     }
        // }

        // // Build an optimized list of triangles from indices and instance (make unique) any vertices common attributes (position, normal etc.).
        // // We can then use this to quickly build am optimized indexed vertex buffer of any selected vertex elements in run-time without any sorting.
        // boolean mesh_has_normals = normal_indices_list.size() > 0;
        // List<MeshVertexIndex> shared_vertex_indices = new ArrayList<MeshVertexIndex>(mesh.triangles.count*3);
        // List<Integer> mesh_index_list = new ArrayList<Integer>(mesh.triangles.count*3);
        // for (int i = 0; i < mesh.triangles.count*3; ++i) {
        //     MeshVertexIndex ci = new MeshVertexIndex();
        //     ci.position = position_indices_list.get(i);
        //     ci.texcoord0 = texcoord_indices_list.get(i);
        //     ci.normal = mesh_has_normals ? normal_indices_list.get(i) : 0;
        //     int index = optimize ? shared_vertex_indices.indexOf(ci) : -1;
        //     if(index == -1) {
        //         // create new vertex as this is not equal to any existing in generated list
        //         mesh_index_list.add(shared_vertex_indices.size());
        //         shared_vertex_indices.add(ci);
        //     } else {
        //         // shared vertex, add index to existing vertex in generating list instead of adding new
        //         mesh_index_list.add(index);
        //     }
        // }
        // List<Rig.MeshVertexIndices> mesh_vertex_indices = new ArrayList<Rig.MeshVertexIndices>(mesh.triangles.count*3);
        // for (int i = 0; i < shared_vertex_indices.size() ; ++i) {
        //     Rig.MeshVertexIndices.Builder b = Rig.MeshVertexIndices.newBuilder();
        //     MeshVertexIndex ci = shared_vertex_indices.get(i);
        //     b.setPosition(ci.position);
        //     b.setTexcoord0(ci.texcoord0);
        //     b.setNormal(ci.normal);
        //     mesh_vertex_indices.add(b.build());
        // }

        // Rig.IndexBufferFormat indices_format;
        // ByteBuffer indices_bytes;
        // if(shared_vertex_indices.size() <= 65536)
        // {
        //     // if we only need 16-bit indices, use this primarily. Less data to upload to GPU and ES2.0 core functionality.
        //     indices_format = Rig.IndexBufferFormat.INDEXBUFFER_FORMAT_16;
        //     indices_bytes = ByteBuffer.allocateDirect(mesh_index_list.size() * 2);
        //     indices_bytes.order(ByteOrder.LITTLE_ENDIAN).asShortBuffer();
        //     for (int i = 0; i < mesh_index_list.size();) {
        //         indices_bytes.putShort(mesh_index_list.get(i++).shortValue());
        //     }
        // }
        // else
        // {
        //     indices_format = Rig.IndexBufferFormat.INDEXBUFFER_FORMAT_32;
        //     indices_bytes = ByteBuffer.allocateDirect(mesh_index_list.size() * 4);
        //     indices_bytes.order(ByteOrder.LITTLE_ENDIAN).asIntBuffer();
        //     for (int i = 0; i < mesh_index_list.size();) {
        //         indices_bytes.putInt(mesh_index_list.get(i++));
        //     }
        // }
        // indices_bytes.rewind();

        // List<Integer> bone_indices_list = new ArrayList<Integer>(position_list.size()*4);
        // List<Float> bone_weights_list = new ArrayList<Float>(position_list.size()*4);
        // int max_bone_count = loadVertexWeights(scene, skeleton, bone_weights_list, bone_indices_list);

        // We currently only support one mesh per collada file
        // This result in one dmRigDDF::Mesh, one dmRigDDF::MeshEntry with only one MeshSlot.
        // The MeshSlot will only contain one "mesh attachment" pointing to the Mesh (index: 0),
        // the active index should be 0 to indicate the one and only attachment.

        // Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();
        // meshBuilder.addAllVertices(mesh_vertex_indices);
        // meshBuilder.setIndices(ByteString.copyFrom(indices_bytes));
        // meshBuilder.setIndicesFormat(indices_format);
        // if(normals != null) {
        //     meshBuilder.addAllNormals(normal_list);
        //     meshBuilder.addAllNormalsIndices(normal_indices_list);
        // }
        // meshBuilder.addAllPositions(position_list);
        // meshBuilder.addAllTexcoord0(texcoord_list);
        // meshBuilder.addAllPositionIndices(position_indices_list);
        // meshBuilder.addAllTexcoord0Indices(texcoord_indices_list);
        // meshBuilder.addAllWeights(bone_weights_list);
        // meshBuilder.addAllBoneIndices(bone_indices_list);

        // MeshSlot.Builder meshSlotBuilder = MeshSlot.newBuilder();
        // meshSlotBuilder.addMeshAttachments(0);
        // meshSlotBuilder.setActiveIndex(0);
        // meshSlotBuilder.addSlotColor(1.0f);
        // meshSlotBuilder.addSlotColor(1.0f);
        // meshSlotBuilder.addSlotColor(1.0f);
        // meshSlotBuilder.addSlotColor(1.0f);

        // MeshEntry.Builder meshEntryBuilder = MeshEntry.newBuilder();
        // //meshEntryBuilder.addMeshSlots(meshSlotBuilder);
        // meshEntryBuilder.setId(0);

        //meshSetBuilder.addMeshAttachments(meshBuilder);
        //meshSetBuilder.addMeshEntries(meshEntryBuilder);

        ArrayList<Rig.Mesh> meshes = new ArrayList<>();

        PointerBuffer aiMeshes = scene.mMeshes();
        for (int i = 0; i < scene.mNumMeshes(); ++i) {
            AIMesh aiMesh = AIMesh.create(aiMeshes.get(i));
            Rig.Mesh mesh = loadMesh(aiMesh, skeleton);
            meshes.add(mesh);
        }

        meshSetBuilder.addAllMeshes(meshes);
        meshSetBuilder.setMaxBoneCount(skeleton.size());
        meshSetBuilder.setSlotCount(1);

        // List<String> boneRefArray = createBoneReferenceList(collada);
        // if (boneRefArray != null && !boneRefArray.isEmpty()) {
        //     for (int i = 0; i < boneRefArray.size(); i++) {
        //         meshSetBuilder.addBoneList(MurmurHash.hash64(boneRefArray.get(i)));
        //     }
        // }
    }


    public static void loadSkeleton(byte[] content, String suffix, Rig.Skeleton.Builder skeletonBuilder) throws IOException, LoaderException {
        AIScene aiScene = loadScene(content, suffix);
        loadSkeleton(aiScene, skeletonBuilder);
    }

    // private static Rig.Bone loadBone(AIBone bone, AssetSpace assetSpace, HashMap<String, Matrix4d> boneTransforms) {
    // }


    // private static Bone loadBone(XMLNode node, ArrayList<Bone> boneList, ArrayList<String> boneIds, AssetSpace assetSpace, HashMap<String, Matrix4d> boneTransforms) {

    //     // Get the bone transform for this bone from the transform lookup map.
    //     Matrix4d boneBindMatrix = new Matrix4d();
    //     if (boneTransforms.containsKey(node.id)) {
    //         boneBindMatrix = new Matrix4d(boneTransforms.get(node.id));
    //     }

    //     // Apply the asset space scale to all bone positions,
    //     // only apply the asset space rotation to the root bone.
    //     if (assetSpace != null) {
    //         boneBindMatrix.m03 *= assetSpace.unit;
    //         boneBindMatrix.m13 *= assetSpace.unit;
    //         boneBindMatrix.m23 *= assetSpace.unit;

    //         boneBindMatrix.mul(assetSpace.rotation, boneBindMatrix);

    //         // Reset asset space rotation to identity,
    //         // so it's only used for root.
    //         assetSpace.rotation.setIdentity();
    //     }

    //     Bone newBone = new Bone(node, node.sid, node.name, MathUtil.vecmath1ToVecmath2(new Matrix4f(boneBindMatrix)), new org.openmali.vecmath2.Quaternion4f(0f, 0f, 0f, 1f));

    //     // Add bone and bone id to the bone list and bone id array.
    //     boneList.add(newBone);
    //     boneIds.add(newBone.getSourceId());

    //     // Iterate over children and look for other JOINTs.
    //     for(XMLNode childNode : node.childrenList) {
    //         if(childNode.type == XMLNode.Type.JOINT) {
    //             Bone childBone = loadBone(childNode, boneList, boneIds, assetSpace, boneTransforms);
    //             newBone.addChild(childBone);
    //         }
    //     }

    //     return newBone;
    // }

    // // Recursive helper function for the below function (getBoneReferenceList()).
    // // Goes through the JOINT nodes in the scene and add their ids (bone id).
    // private static void getBoneReferenceFromSceneNode(XMLNode node, ArrayList<String> boneList, HashSet<String> boneIdLut) {
    //     if (!boneIdLut.contains(node.id)) {
    //         boneList.add(node.id);
    //         boneIdLut.add(node.id);
    //     }

    //     for(XMLNode childNode : node.childrenList) {
    //         if(childNode.type == XMLNode.Type.JOINT) {
    //             getBoneReferenceFromSceneNode(childNode, boneList, boneIdLut);
    //         }
    //     }
    // }

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
                System.out.printf("  bone: %s\n", bone_name);
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

        //findNessaryNodes(root, necessityMap);
        ArrayList<ModelUtil.Bone> skeleton = new ArrayList<>();
        gatherSkeleton(root_bone, BONE_NO_PARENT, boneSet, skeleton);

        for (ModelUtil.Bone bone : skeleton) {
            // Note that some bones might not actually influence any vertices, and won't have an AIBone
            bone.bone = boneMap.getOrDefault(bone.name, null);

            System.out.printf("Skeleton: %s\n", bone.name);

            // TODO: check that the bone.bone.mNumWeights() is <= 4, otherwise output a warning
        }

        return skeleton;
    }

    public static ArrayList<ModelUtil.Bone> loadSkeleton(AIScene aiScene) throws IOException, LoaderException {
        // if (collada.libraryVisualScenes.size() != 1) {
        //     return null;
        // }

        AssetSpace assetSpace = getAssetSpace(aiScene);

        ArrayList<ModelUtil.Bone> skeleton = findSkeleton(aiScene);

        Matrix4d skeletonTransform = new Matrix4d();
        skeletonTransform.setIdentity();

        int index = 0;
        for (ModelUtil.Bone bone : skeleton) {
            AIMatrix4x4 offset_matrix = AIMatrix4x4.create();
            if (bone.bone != null) {
                offset_matrix = bone.bone.mOffsetMatrix();
            } else {
                offset_matrix.set(1.0f,0.0f,0.0f,0.0f, 0.0f,1.0f,0.0f,0.0f, 0.0f,0.0f,1.0f,0.0f, 0.0f,0.0f,0.0f,1.0f);
            }
            //bone.transform = bone.bone.

            printMatrix4x4(bone.name, offset_matrix);

            // Matrix4d bone_matrix = convertMatrix4x4(offset_matrix);
            // printMatrix4d("converted", bone_matrix);

            // bone_matrix.mul(assetSpace.rotation, bone_matrix);
            // printMatrix4d("transformed", bone_matrix);

            bone.transform = convertMatrix4x4(offset_matrix);
        }
        return skeleton;
    }

    // Generate skeleton DDF data of bones.
    // It will extract the position, rotation and scale from the bone transform as needed by the runtime.
    private static void boneToDDF(ModelUtil.Bone bone, ArrayList<Rig.Bone> ddfBones) {
        com.dynamo.rig.proto.Rig.Bone.Builder b = com.dynamo.rig.proto.Rig.Bone.newBuilder();

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
        // for (ModelUtil.Bone bone : boneList) {
        //     boneIds.add(bone.name);
        // }

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
