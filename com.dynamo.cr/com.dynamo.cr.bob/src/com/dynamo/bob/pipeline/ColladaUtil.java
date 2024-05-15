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

package com.dynamo.bob.pipeline;

import java.io.File;
import java.io.FileInputStream;
import java.io.ByteArrayInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Vector;
import java.util.Map.Entry;

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.lang3.ArrayUtils;

import javax.vecmath.Matrix4d;
import javax.vecmath.Matrix4f;
import javax.vecmath.Point3f;
import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;
import javax.vecmath.Tuple4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector3f;
import javax.vecmath.Vector4d;
import javax.xml.stream.XMLInputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

// Sources:
// https://github.com/spidasoftware/jagatoo/blob/master/src/main/java/org/jagatoo/loaders/models/collada
import org.jagatoo.loaders.models.collada.stax.XMLAnimation;
import org.jagatoo.loaders.models.collada.stax.XMLAnimationClip;
import org.jagatoo.loaders.models.collada.stax.XMLAsset;
import org.jagatoo.loaders.models.collada.stax.XMLCOLLADA;
import org.jagatoo.loaders.models.collada.stax.XMLController;
import org.jagatoo.loaders.models.collada.stax.XMLGeometry;
import org.jagatoo.loaders.models.collada.stax.XMLInput;
import org.jagatoo.loaders.models.collada.stax.XMLInstanceGeometry;
import org.jagatoo.loaders.models.collada.stax.XMLLibraryAnimationClips;
import org.jagatoo.loaders.models.collada.stax.XMLLibraryAnimations;
import org.jagatoo.loaders.models.collada.stax.XMLLibraryControllers;
import org.jagatoo.loaders.models.collada.stax.XMLMesh;
import org.jagatoo.loaders.models.collada.stax.XMLSampler;
import org.jagatoo.loaders.models.collada.stax.XMLNode;
import org.jagatoo.loaders.models.collada.stax.XMLSkin;
import org.jagatoo.loaders.models.collada.stax.XMLSource;
import org.jagatoo.loaders.models.collada.stax.XMLVisualScene;
import org.jagatoo.loaders.models.collada.stax.XMLAsset.UpAxis;
import org.jagatoo.loaders.models.collada.stax.XMLVisualSceneExtra;
import org.jagatoo.loaders.models.collada.datastructs.animation.Bone;

import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.RigUtil;
import com.dynamo.bob.util.RigUtil.AnimationKey;
import com.dynamo.bob.util.RigUtil.Weight;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.proto.DdfMath.Transform;

import com.dynamo.rig.proto.Rig;
import com.dynamo.rig.proto.Rig.AnimationInstanceDesc;
import com.dynamo.rig.proto.Rig.AnimationSetDesc;
import com.google.protobuf.TextFormat;

public class ColladaUtil {

    static int BONE_NO_PARENT = 0xffffffff;

    private static Logger logger = Logger.getLogger(ColladaUtil.class.getName());

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

    public static AssetSpace getAssetSpace(XMLAsset asset)
    {
        AssetSpace assetSpace = new AssetSpace();
        if (asset != null) {

            if (asset.unit != null) {
                assetSpace.unit = asset.unit.meter;
            }

            UpAxis upAxis = asset.upAxis != null ? asset.upAxis : UpAxis.Y_UP;
            if (upAxis.equals(UpAxis.Z_UP)) {
                assetSpace.rotation.setRow(0, new double[] {1.0, 0.0, 0.0, 0.0});
                assetSpace.rotation.setRow(1, new double[] {0.0, 0.0, 1.0, 0.0});
                assetSpace.rotation.setRow(2, new double[] {0.0, -1.0, 0.0, 0.0});
            } else if (upAxis.equals(UpAxis.X_UP)) {
                assetSpace.rotation.setRow(0, new double[] {0.0, -1.0, 0.0, 0.0});
                assetSpace.rotation.setRow(1, new double[] {1.0, 0.0, 0.0, 0.0});
                assetSpace.rotation.setRow(2, new double[] {0.0, 0.0, 1.0, 0.0});
            } else {
                assetSpace.rotation.setRow(0, new double[] {1.0, 0.0, 0.0, 0.0});
                assetSpace.rotation.setRow(1, new double[] {0.0, 1.0, 0.0, 0.0});
                assetSpace.rotation.setRow(2, new double[] {0.0, 0.0, 1.0, 0.0});
            }
        }

        return assetSpace;
    }

    // private static void printVector3d(Vector3d v) {
    //     System.err.printf("  %f, %f, %f\n", v.getX(), v.getY(), v.getZ());
    // }

    private static void printVector4d(Vector4d v) {
        System.err.printf("  %f, %f, %f, %f\n", v.getX(), v.getY(), v.getZ(), v.getW());
    }

    // private static void printQuat4d(Quat4d v) {
    //     System.err.printf("  %f, %f, %f, %f\n", v.getX(), v.getY(), v.getZ(), v.getW());
    // }

    private static void printMatrix4d(Matrix4d mat) {
        Vector4d v = new Vector4d();
        mat.getColumn(0, v); printVector4d(v);
        mat.getColumn(1, v); printVector4d(v);
        mat.getColumn(2, v); printVector4d(v);
        mat.getColumn(3, v); printVector4d(v);
    }

    private static XMLInput findInput(List<XMLInput> inputs, String semantic, boolean required)
            throws LoaderException {
        for (XMLInput i : inputs) {
            if (i.semantic.equals(semantic))
                return i;
        }
        if (required)
            throw new LoaderException(String.format("Input '%s' not found", semantic));
        return null;
    }

    private static HashMap<String, XMLSource> getSourcesMap(List<XMLSource> sources) {
        HashMap<String, XMLSource> sourcesMap;
        sourcesMap = new HashMap<String, XMLSource>();
        for (int i = 0; i < sources.size(); i++) {
            XMLSource source = sources.get(i);
            sourcesMap.put(source.id, source);
        }
        return sourcesMap;
    }

    public static XMLCOLLADA loadDAE(InputStream is) throws IOException, XMLStreamException, LoaderException {
        XMLInputFactory factory = XMLInputFactory.newInstance();
        factory.setProperty("javax.xml.stream.isCoalescing", true);
        XMLStreamReader stream_reader = factory.createXMLStreamReader(is);
        XMLCOLLADA collada = new XMLCOLLADA();
        collada.parse(stream_reader);
        return collada;
    }

    public static boolean load(InputStream is, Rig.MeshSet.Builder meshSetBuilder, Rig.AnimationSet.Builder animationSetBuilder, Rig.Skeleton.Builder skeletonBuilder) throws IOException, XMLStreamException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        loadMesh(collada, meshSetBuilder, true, false);
        loadSkeleton(collada, skeletonBuilder, new ArrayList<String>());
        loadAnimations(collada, animationSetBuilder, "", new ArrayList<String>());
        return true;
    }

    private static HashMap<String, XMLSource> getSamplersLUT(XMLAnimation animation) {
        XMLSampler sampler = animation.samplers.get(0);
        HashMap<String, XMLSource> samplersLUT = new HashMap<String, XMLSource>();
        for (int i = 0; i < sampler.inputs.size(); i++) {
            XMLInput input = sampler.inputs.get(i);

            // Find source for sampler
            XMLSource source = null;
            for (int j = 0; j < animation.sources.size(); j++) {
                if (animation.sources.get(j).id.equals(input.source)) {
                    source = animation.sources.get(j);
                    break;
                }
            }
            samplersLUT.put(input.semantic, source);
        }
        return samplersLUT;
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

    private static void ExtractMatrixKeys(Bone bone, Matrix4d localToParent, AssetSpace assetSpace, XMLAnimation animation, RigUtil.AnimationTrack posTrack, RigUtil.AnimationTrack rotTrack, RigUtil.AnimationTrack scaleTrack) {


        Vector4d lastR = new Vector4d(0.0, 0.0, 0.0, 0.0);

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
    }

    private static void ExtractKeys(Bone bone, Matrix4d localToParent, AssetSpace assetSpace, XMLAnimation animation, RigUtil.AnimationTrack posTrack, RigUtil.AnimationTrack rotTrack, RigUtil.AnimationTrack scaleTrack) throws LoaderException {
        switch (animation.getType()) {
        case TRANSLATE:
        case SCALE:
        case ROTATE:
            throw new LoaderException("Currently only collada files with matrix animations are supported.");
        case TRANSFORM:
        case MATRIX:
            ExtractMatrixKeys(bone, localToParent, assetSpace, animation, posTrack, rotTrack, scaleTrack);
            break;
        default:
            throw new LoaderException(String.format("Animations of type %s are not supported.", animation.getType().name()));
        }
    }

    private static Matrix4d getBoneLocalToParent(Bone bone) {
        return new Matrix4d(MathUtil.vecmath2ToVecmath1(bone.bindMatrix));
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
        RigUtil.sampleTrack(track, scaleBuilder, startTime, duration, sampleRate, spf, true);
    }

    public static void createAnimationTracks(Rig.RigAnimation.Builder animBuilder,
                                             String boneName,
                                             Bone bone,
                                             RigUtil.AnimationTrack posTrack,
                                             RigUtil.AnimationTrack rotTrack,
                                             RigUtil.AnimationTrack sclTrack,
                                            int boneIndex, double duration, double startTime, double sampleRate) {
        double spf = 1.0 / sampleRate;

        Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
        animTrackBuilder.setBoneId(MurmurHash.hash64(boneName));

        samplePosTrack(animBuilder, animTrackBuilder, posTrack, duration, startTime, sampleRate, spf, true);
        sampleRotTrack(animBuilder, animTrackBuilder, rotTrack, duration, startTime, sampleRate, spf, true);
        sampleScaleTrack(animBuilder, animTrackBuilder, sclTrack, duration, startTime, sampleRate, spf, true);

        animBuilder.addTracks(animTrackBuilder.build());
    }

    private static class SortOnBoneIndex implements Comparator<Bone> {
        public SortOnBoneIndex(HashMap<Long, Integer> boneRefMap) {
            this.boneRefMap = boneRefMap;
        }
        public int compare(Bone a, Bone b) {
            Long boneHashA = MurmurHash.hash64(a.getSourceId());
            Long boneHashB = MurmurHash.hash64(b.getSourceId());
            Integer refIndexA = this.boneRefMap.getOrDefault(boneHashA, 100000);
            Integer refIndexB = this.boneRefMap.getOrDefault(boneHashB, 100000);
            return refIndexA - refIndexB;
        }
        private HashMap<Long, Integer> boneRefMap;
    }

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

        boneList.sort(new SortOnBoneIndex(boneRefMap));

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

            if (refIndex == null)
                continue;

            String boneName = bone.getSourceId();
            if (bi == 0 || refIndex == 0)
            {
                boneName = "root";
            }

            Matrix4d localToParent = getBoneLocalToParent(bone);
            AssetSpace assetSpace = getAssetSpace(collada.asset);
            if (bi != 0) {
                // Only apply up axis rotation for first bone.
                assetSpace.rotation.setIdentity();
            }

            if (boneToAnimations.containsKey(boneId))
            {
                // search the animations for each bone
                ArrayList<XMLAnimation> anims = boneToAnimations.get(boneId);

                boolean anim_found = false;
                for (XMLAnimation animation : anims) {
                    if (animation.getType() == null) {
                        continue;
                    }

                    RigUtil.AnimationTrack posTrack = new RigUtil.AnimationTrack();
                    posTrack.property = RigUtil.AnimationTrack.Property.POSITION;
                    RigUtil.AnimationTrack rotTrack = new RigUtil.AnimationTrack();
                    rotTrack.property = RigUtil.AnimationTrack.Property.ROTATION;
                    RigUtil.AnimationTrack sclTrack = new RigUtil.AnimationTrack();
                    sclTrack.property = RigUtil.AnimationTrack.Property.SCALE;

                    ExtractKeys(bone, localToParent, assetSpace, animation, posTrack, rotTrack, sclTrack);

                    createAnimationTracks(animBuilder, boneName, bone, posTrack, rotTrack, sclTrack, refIndex, (float)duration, sceneStartTime, sceneFrameRate);

                    break; // we only support one animation per file/bone
                }
            }
        }
    }

    public interface ColladaResourceResolver {
        public InputStream getResource(String resourceName) throws FileNotFoundException;
    }

    private static void loadAnimationClipIds(InputStream is, String parentId, ArrayList<String> animationIds) throws IOException, XMLStreamException, LoaderException {
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

    public static void loadAnimationIds(String resourceName, String parentId, ArrayList<String> animationIds, final ColladaResourceResolver resourceResolver) throws IOException, XMLStreamException, LoaderException {
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

    public static void loadAnimations(InputStream is, Rig.AnimationSet.Builder animationSetBuilder, String parentAnimationId, ArrayList<String> animationIds) throws IOException, XMLStreamException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        loadAnimations(collada, animationSetBuilder, parentAnimationId, animationIds);
    }

    public static void loadAnimations(XMLCOLLADA collada, Rig.AnimationSet.Builder animationSetBuilder, String animationId, ArrayList<String> animationIds) throws IOException, XMLStreamException, LoaderException {

        if (collada.libraryAnimations.size() != 1) {
            return;
        }

        List<String> boneRefArray = createBoneReferenceList(collada);
        if (boneRefArray == null || boneRefArray.isEmpty()) {
            return;
        }

        // Load skeleton to get bone bind poses
        ArrayList<String> boneIds = new ArrayList<String>();
        ArrayList<Bone> boneList = loadSkeleton(collada, boneIds);

        HashMap<Long, Integer> boneRefMap = new HashMap<Long, Integer>();
        int boneIndex = 0;
        for (String boneId : boneIds) {
            Long boneRef = MurmurHash.hash64(boneId);
            boneRefMap.put(boneRef, boneIndex);
            ++boneIndex;
        }

        // Animation clips
        ArrayList<XMLLibraryAnimationClips> animClips = collada.libraryAnimationClips;
        XMLLibraryAnimations libraryAnimation = collada.libraryAnimations.get(0);

        if(!animClips.isEmpty()) {

            throw new LoaderException("Anmation clips are currently not supported.");

        } else {

            float totalAnimationLength = 0.0f;
            HashMap<String, ArrayList<XMLAnimation>> boneToAnimations = new HashMap<String, ArrayList<XMLAnimation>>();

            // Loop through all animations and build a bone-to-animations LUT
            Iterator<Entry<String, XMLAnimation>> it = libraryAnimation.animations.entrySet().iterator();
            while (it.hasNext()) {
                XMLAnimation animation = (XMLAnimation)it.next().getValue();
                if (animation.channels.size() == 0) {
                    continue;
                }

                String boneTarget = animation.getTargetBone();
                if (!boneToAnimations.containsKey(boneTarget)) {
                    boneToAnimations.put(boneTarget, new ArrayList<XMLAnimation>());
                }
                boneToAnimations.get(boneTarget).add(animation);

                // Figure out the total duration of the animation.
                HashMap<String, XMLSource> samplersLUT = getSamplersLUT(animation);
                XMLSource inputSampler = samplersLUT.get("INPUT");
                float animLength = inputSampler.floatArray.floats[inputSampler.floatArray.count-1];
                totalAnimationLength = Math.max(totalAnimationLength, animLength);
            }

            // If no clips are provided, add a "Default" clip that is the whole animation as one clip
            Rig.RigAnimation.Builder animBuilder = Rig.RigAnimation.newBuilder();
            boneAnimToDDF(collada, animBuilder, boneList, boneRefMap, boneToAnimations, totalAnimationLength);
            animBuilder.setId(MurmurHash.hash64(animationId));

            animationIds.add(animationId);
            animationSetBuilder.addAnimations(animBuilder.build());
        }
    }

    public static void loadMesh(InputStream is, Rig.MeshSet.Builder meshSetBuilder) throws IOException, XMLStreamException, LoaderException {
        loadMesh(is, meshSetBuilder, false, false);
    }

    public static void loadMesh(InputStream is, Rig.MeshSet.Builder meshSetBuilder, boolean optimize, boolean splitMeshes) throws IOException, XMLStreamException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        loadMesh(collada, meshSetBuilder, optimize, splitMeshes);
    }

    private static XMLNode getFirstNodeWithGeometry(Collection<XMLVisualScene> scenes) {
        for (XMLVisualScene scene : scenes) {
            for (XMLNode node : scene.nodes.values()) {
                if (node.instanceGeometries.size() > 0) {
                    return node;
                }
            }
        }

        return null;
    }

    private static float[] toFloatArray(List<Float> a) {
        float[] floatArray = new float[a.size()];
        for (int i = 0 ; i < a.size(); i++) {
            floatArray[i] = (float) a.get(i);
        }
        return floatArray;
    }

    private static int[] toIntArray(List<Integer> a) {
        return a.stream().mapToInt(x -> x).toArray();
    }

    private static ModelImporter.Aabb calcAabb(float[] positions) {
        ModelImporter.Aabb aabb = new ModelImporter.Aabb();
        for (int i = 0; i < positions.length; i += 3) {
            aabb.expand(positions[i+0], positions[i+1], positions[i+2]);
        }
        return aabb;
    }

    private static ModelImporter.Mesh createModelImporterMesh(List<Float> position_list,
                                                              List<Float> normal_list,
                                                              List<Float> texcoord_list,
                                                              List<Float> bone_weights_list,
                                                              List<Integer> bone_indices_list,
                                                              List<Integer> mesh_index_list,
                                                              ModelImporter.Material material) {
        ModelImporter.Mesh mesh = new ModelImporter.Mesh();
        mesh.name = "";
        mesh.material = material;

        mesh.positions = toFloatArray(position_list);
        if (normal_list.size() > 0)
            mesh.normals = toFloatArray(normal_list);

        mesh.aabb = calcAabb(mesh.positions);

        mesh.tangents = null;
        mesh.colors = null;

        if (bone_weights_list.size() > 0)
            mesh.weights = toFloatArray(bone_weights_list);
        if (bone_indices_list.size() > 0)
            mesh.bones = toIntArray(bone_indices_list);

        mesh.texCoords0NumComponents = 2;
        if (texcoord_list.size() > 0)
            mesh.texCoords0 = toFloatArray(texcoord_list);
        mesh.texCoords1NumComponents = 0; // 2 or 3
        mesh.texCoords1 = null;

        if (mesh_index_list.size() > 0)
            mesh.indices = toIntArray(mesh_index_list);

        mesh.vertexCount = position_list.size() / 3;
        mesh.indexCount = mesh_index_list.size();

        return mesh;
    }

    public static void loadMesh(XMLCOLLADA collada, Rig.MeshSet.Builder meshSetBuilder, boolean optimize, boolean splitMeshes) throws IOException, XMLStreamException, LoaderException {
        if (collada.libraryGeometries.size() != 1) {
            if (collada.libraryGeometries.isEmpty()) {
                return;
            }
            throw new LoaderException("Only a single geometry is supported");
        }

        // Use first geometry entry as default
        XMLGeometry geom = collada.libraryGeometries.get(0).geometries.values().iterator().next();

        // Find first node in visual scene tree that has a instance geometry
        XMLNode sceneNode = null;
        Matrix4d sceneNodeMatrix = null;
        if (collada.libraryVisualScenes.size() > 0) {
            sceneNode = getFirstNodeWithGeometry(collada.libraryVisualScenes.get(0).scenes.values());
            if (sceneNode != null) {
                XMLInstanceGeometry instanceGeo = sceneNode.instanceGeometries.get(0);
                String geometryId = instanceGeo.url;

                // Get node transform if available
                sceneNodeMatrix = new Matrix4d(MathUtil.vecmath2ToVecmath1(sceneNode.matrix.matrix4f));

                // Find geometry entry
                for (XMLGeometry geomEntry : collada.libraryGeometries.get(0).geometries.values())
                {
                    if (geomEntry.equals(geometryId)) {
                        geom = geomEntry;
                        break;
                    }
                }
            }
        }
        if (sceneNodeMatrix == null) {
            sceneNodeMatrix = new Matrix4d();
            sceneNodeMatrix.setIdentity();
        }

        XMLMesh mesh = geom.mesh;
        if(mesh == null || mesh.triangles == null) {
            return;
        }
        List<XMLSource> sources = mesh.sources;
        HashMap<String, XMLSource> sourcesMap = getSourcesMap(sources);

        XMLInput vpos_input = findInput(mesh.vertices.inputs, "POSITION", true);
        XMLInput vertex_input = findInput(mesh.triangles.inputs, "VERTEX", true);
        XMLInput normal_input = findInput(mesh.triangles.inputs, "NORMAL", false);
        XMLInput texcoord_input = findInput(mesh.triangles.inputs, "TEXCOORD", false);

        // Get bind shape from skin, if it exist
        Matrix4d bindShapeMatrix = null;
        XMLSkin skin = null;
        if (!collada.libraryControllers.isEmpty()) {
            skin = findFirstSkin(collada.libraryControllers.get(0));
            if (skin != null && skin.bindShapeMatrix != null) {
                bindShapeMatrix = new Matrix4d(MathUtil.vecmath2ToVecmath1(skin.bindShapeMatrix.matrix4f));
            }
        }
        if (bindShapeMatrix == null) {
            bindShapeMatrix = new Matrix4d();
            bindShapeMatrix.setIdentity();
        }

        // Apply any matrix found for scene node
        bindShapeMatrix.mul(sceneNodeMatrix);

        // NOTE: Normals could be part of the vertex of specified directly in triangles...
        int normalOffset;
        if (normal_input  != null) {
            normalOffset = normal_input.offset;
        } else {
            normalOffset = 0;
            normal_input = findInput(mesh.vertices.inputs, "NORMAL", false);
        }

        if (mesh.triangles.inputs.size() == 0)
            throw new LoaderException("No inputs in triangles");

        int stride = 0;
        for (XMLInput i : mesh.triangles.inputs) {
            stride = Math.max(stride, i.offset);
        }
        stride += 1;

        XMLSource positions = sourcesMap.get(vpos_input.source);
        XMLSource normals = null;
        if (normal_input != null) {
            normals = sourcesMap.get(normal_input.source);
        }

        XMLSource texcoords = null;
        if (texcoord_input != null) {
            texcoords = sourcesMap.get(texcoord_input.source);
        }

        // Apply the up-axis matrix on the bind shape matrix.
        AssetSpace assetSpace = getAssetSpace(collada.asset);
        Matrix4d assetSpaceMtx = new Matrix4d();
        Matrix4d assetScaleMtx = new Matrix4d();
        assetScaleMtx.setIdentity();
        assetScaleMtx.setScale(assetSpace.unit);
        assetSpaceMtx.mul(assetSpace.rotation, assetScaleMtx);
        bindShapeMatrix.mul(assetSpaceMtx, bindShapeMatrix);

        List<Float> position_list = new ArrayList<Float>(positions.floatArray.count);
        for (int i = 0; i < positions.floatArray.count / 3; ++i) {
            Point3f p = new Point3f(positions.floatArray.floats[i*3], positions.floatArray.floats[i*3+1], positions.floatArray.floats[i*3+2]);
            bindShapeMatrix.transform(p);
            position_list.add(p.getX());
            position_list.add(p.getY());
            position_list.add(p.getZ());
        }

        // Create a normal matrix which is the transposed inverse of
        // the bind shape matrix (which already include the up axis matrix).
        Matrix4f normalMatrix = new Matrix4f(bindShapeMatrix);
        normalMatrix.invert();
        normalMatrix.transpose();

        List<Float> normal_list = new ArrayList<Float>();
        if(normals != null) {
            normal_list = new ArrayList<Float>(normals.floatArray.count);
            for (int i = 0; i < normals.floatArray.count / 3; ++i) {
                Vector3f n = new Vector3f(normals.floatArray.floats[i*3], normals.floatArray.floats[i*3+1], normals.floatArray.floats[i*3+2]);
                normalMatrix.transform(n);
                if (n.lengthSquared() > 0.0) {
                    n.normalize();
                }
                normal_list.add(n.getX());
                normal_list.add(n.getY());
                normal_list.add(n.getZ());
            }
        }

        List<Float> texcoord_list;
        if(texcoords == null) {
            texcoord_list = new ArrayList<Float>(Arrays.asList(0f, 0f));
        } else {
            texcoord_list = new ArrayList<Float>(texcoords.floatArray.count);
            for (int i = 0; i < texcoords.floatArray.count; i += 2 ) {
                texcoord_list.add(texcoords.floatArray.floats[i]);
                texcoord_list.add(texcoords.floatArray.floats[i+1]);
            }
        }

        List<Integer> position_indices_list = new ArrayList<Integer>(mesh.triangles.count*3);
        List<Integer> normal_indices_list = new ArrayList<Integer>(mesh.triangles.count*3);
        List<Integer> texcoord_indices_list = new ArrayList<Integer>(mesh.triangles.count*3);

        // Sometimes the <p> values can be -1 from Maya exports, we clamp it below to 0 instead.
        // Similar solution as AssImp; https://github.com/assimp/assimp/blob/master/code/ColladaParser.cpp#L2336
        for (int i = 0; i < mesh.triangles.count; ++i) {

            for (int j = 0; j < 3; ++j) {
                int idx = i * stride * 3 + vertex_input.offset;
                int vert_idx = Math.max(0, mesh.triangles.p[idx + stride * j]);
                position_indices_list.add(vert_idx);

                if (normals != null) {
                    idx = i * stride * 3 + normalOffset;
                    vert_idx = Math.max(0, mesh.triangles.p[idx + stride * j]);
                    normal_indices_list.add(vert_idx);
                }

                if (texcoords == null) {
                    texcoord_indices_list.add(0);
                } else {
                    idx = i * stride * 3 + texcoord_input.offset;
                    vert_idx = Math.max(0, mesh.triangles.p[idx + stride * j]);
                    texcoord_indices_list.add(vert_idx);
                }

            }

        }

        class MeshVertexIndex {
            public int position, texcoord0, normal;

            @Override
            public final int hashCode() { // fnv 32 bit hash
                int result = 0x811c9dc5;
                result ^= position;
                result *= 0x01000193;
                result ^= texcoord0;
                result *= 0x01000193;
                result ^= normal;
                result *= 0x01000193;
                return result;
            }
        }

        long tstart = System.currentTimeMillis();
        TimeProfiler.start();
        TimeProfiler.addData("optimizeVertices", "Colladautil");

        // Build an optimized list of triangles from indices and instance (make unique) any vertices common attributes (position, normal etc.).
        // We can then use this to quickly build am optimized indexed vertex buffer of any selected vertex elements in run-time without any sorting.
        boolean mesh_has_normals = normal_indices_list.size() > 0;
        List<MeshVertexIndex> shared_vertex_indices = new ArrayList<MeshVertexIndex>(mesh.triangles.count*3);
        Map<Integer, Integer> shared_vertex_index_map = new HashMap<>();

        List<Integer> mesh_index_list = new ArrayList<Integer>(mesh.triangles.count*3);
        for (int i = 0; i < mesh.triangles.count*3; ++i) {
            MeshVertexIndex ci = new MeshVertexIndex();
            ci.position = position_indices_list.get(i);
            ci.texcoord0 = texcoord_indices_list.get(i);
            ci.normal = mesh_has_normals ? normal_indices_list.get(i) : 0;
            int index = optimize ? (int)shared_vertex_index_map.getOrDefault(ci.hashCode(), -1) : -1;
            if(index == -1) {
                // create new vertex as this is not equal to any existing in generated list
                index = shared_vertex_indices.size();
                mesh_index_list.add(index);
                shared_vertex_indices.add(ci);
                shared_vertex_index_map.put(ci.hashCode(), index);
            } else {
                // shared vertex, add index to existing vertex in generating list instead of adding new
                mesh_index_list.add(index);
            }
        }

        TimeProfiler.stop();
        long tend = System.currentTimeMillis();
        logger.fine("ColladaUtil: Creating %d vertices (optimize: %s) took %f s", shared_vertex_indices.size(), optimize?"on":"off", (tend-tstart)/1000.0);

        int vertex_count = shared_vertex_indices.size();

        ArrayList<Integer> bone_indices_list = new ArrayList<Integer>(vertex_count*4);
        ArrayList<Float> bone_weights_list = new ArrayList<Float>(vertex_count*4);
        int max_bone_count = loadVertexWeights(collada, bone_weights_list, bone_indices_list);

        // Bake the values again into our format
        Float baked_position_list[] = new Float[vertex_count*3];
        Float baked_normal_list[] = new Float[0];
        Float baked_texcoord_list[] = new Float[0];
        Integer baked_bone_indices_list[] = new Integer[0];
        Float baked_bone_weights_list[] = new Float[0];

        if (normal_list.size() > 0) {
            baked_normal_list = new Float[vertex_count*3];
        }
        if (texcoord_list.size() > 0) {
            baked_texcoord_list = new Float[vertex_count*2];
        }
        if (bone_indices_list.size() > 0) {
            baked_bone_indices_list = new Integer[vertex_count*4];
            baked_bone_weights_list = new Float[vertex_count*4];
        }

        for (int index : mesh_index_list) {
            MeshVertexIndex ci = shared_vertex_indices.get(index);

            for (int c = 0; c < 3; ++c)
            {
                baked_position_list[index*3+c] = position_list.get(ci.position*3+c);
                if (normal_list.size() > 0)
                    baked_normal_list[index*3+c] = normal_list.get(ci.normal*3+c);
            }

            if (texcoord_list.size() > 0)
            {
                for (int c = 0; c < 2; ++c)
                {
                    baked_texcoord_list[index*2+c] = texcoord_list.get(ci.texcoord0*2+c);
                }
            }

            if (bone_indices_list.size() > 0)
            {
                // For the bones we use the index of the position
                for (int c = 0; c < 4; ++c)
                {
                    baked_bone_indices_list[index*4+c] = bone_indices_list.get(ci.position*4+c);
                    baked_bone_weights_list[index*4+c] = bone_weights_list.get(ci.position*4+c);
                }
            }
        }

        Rig.Model.Builder modelBuilder = Rig.Model.newBuilder();

        ModelImporter.Material material = new ModelImporter.Material();

        List<ModelImporter.Mesh> allMeshes = new ArrayList<>();
        ModelImporter.Mesh miMesh = createModelImporterMesh(new ArrayList<>(Arrays.asList(baked_position_list)),
                                                            new ArrayList<>(Arrays.asList(baked_normal_list)),
                                                            new ArrayList<>(Arrays.asList(baked_texcoord_list)),
                                                            new ArrayList<>(Arrays.asList(baked_bone_weights_list)),
                                                            new ArrayList<>(Arrays.asList(baked_bone_indices_list)),
                                                            mesh_index_list,
                                                            material);

        if (splitMeshes && vertex_count >= 65536) {
            ModelUtil.splitMesh(miMesh, allMeshes);
        } else {
            allMeshes.add(miMesh);
        }

        for (ModelImporter.Mesh newMesh : allMeshes) {
            modelBuilder.addMeshes(ModelUtil.loadMesh(newMesh));
        }

        String name = sceneNode != null ? sceneNode.name : null;
        if (name == null) {
            name = geom.name;
        }
        if (name == null) {
            name = "unnamed";
        }

        modelBuilder.setId(MurmurHash.hash64(name));

        Matrix4d transformd = new Matrix4d();
        transformd.setIdentity();

        Vector3d tPos = new Vector3d();
        Quat4d tRot = new Quat4d();
        Vector3d tScale = new Vector3d();
        MathUtil.decompose(transformd, tPos, tRot, tScale);
        Transform transform = MathUtil.vecmathToDDF(tPos, tRot, tScale);

        modelBuilder.setLocal(transform);

        meshSetBuilder.addModels(modelBuilder);
        meshSetBuilder.setMaxBoneCount(max_bone_count);

        ArrayList<ModelImporter.Bone> bones = loadSkeleton(collada);
        if (bones != null) {
            for (ModelImporter.Bone bone : bones) {
                meshSetBuilder.addBoneList(MurmurHash.hash64(bone.name));
            }
        }
    }

    public static void loadSkeleton(InputStream is, com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder, ArrayList<String> boneIds) throws IOException, XMLStreamException, LoaderException {
        loadSkeleton(loadDAE(is), skeletonBuilder, boneIds);
    }

    private static Bone loadBone(XMLNode node, ArrayList<Bone> boneList, ArrayList<String> boneIds, AssetSpace assetSpace, HashMap<String, Matrix4d> boneTransforms) {

        // Get the bone transform for this bone from the transform lookup map.
        Matrix4d boneBindMatrix = new Matrix4d();
        if (boneTransforms.containsKey(node.id)) {
            boneBindMatrix = new Matrix4d(boneTransforms.get(node.id));
        }

        // Apply the asset space scale to all bone positions,
        // only apply the asset space rotation to the root bone.
        if (assetSpace != null) {
            boneBindMatrix.m03 *= assetSpace.unit;
            boneBindMatrix.m13 *= assetSpace.unit;
            boneBindMatrix.m23 *= assetSpace.unit;

            boneBindMatrix.mul(assetSpace.rotation, boneBindMatrix);

            // Reset asset space rotation to identity,
            // so it's only used for root.
            assetSpace.rotation.setIdentity();
        }

        Bone newBone = new Bone(node, node.sid, node.name, MathUtil.vecmath1ToVecmath2(new Matrix4f(boneBindMatrix)), new org.openmali.vecmath2.Quaternion4f(0f, 0f, 0f, 1f));

        // Add bone and bone id to the bone list and bone id array.
        boneList.add(newBone);
        boneIds.add(newBone.getSourceId());

        // Iterate over children and look for other JOINTs.
        for(XMLNode childNode : node.childrenList) {
            if(childNode.type == XMLNode.Type.JOINT) {
                Bone childBone = loadBone(childNode, boneList, boneIds, assetSpace, boneTransforms);
                newBone.addChild(childBone);
            }
        }

        return newBone;
    }

    // Recursive helper function for the below function (getBoneReferenceList()).
    // Goes through the JOINT nodes in the scene and add their ids (bone id).
    private static void getBoneReferenceFromSceneNode(XMLNode node, ArrayList<String> boneList, HashSet<String> boneIdLut) {
        if (!boneIdLut.contains(node.id)) {
            boneList.add(node.id);
            boneIdLut.add(node.id);
        }

        for(XMLNode childNode : node.childrenList) {
            if(childNode.type == XMLNode.Type.JOINT) {
                getBoneReferenceFromSceneNode(childNode, boneList, boneIdLut);
            }
        }
    }

    // Creates a list of bone ids ordered by their index.
    // Their index is first taken from the order they appear in the skin entry of the collada file,
    // which correspond to the index where their inv bind pose transform is located in the INV_BIND_MATRIX entry.
    private static ArrayList<String> createBoneReferenceList(XMLCOLLADA collada) throws LoaderException {

        HashSet<String> boneIdLut = new HashSet<String>();
        ArrayList<String> boneRefArray = new ArrayList<String>();

        // First get the bone index from the skin entry
        XMLSkin skin = null;
        if (!collada.libraryControllers.isEmpty()) {
            skin = findFirstSkin(collada.libraryControllers.get(0));

            if (skin != null)
            {
                List<XMLSource> sources = skin.sources;
                HashMap<String, XMLSource> sourcesMap = getSourcesMap(sources);

                XMLInput jointInput = findInput(skin.jointsInputs, "JOINT", true);
                XMLSource jointSource = sourcesMap.get(jointInput.source);

                if(jointSource.nameArray != null) {
                    boneRefArray = new ArrayList<String>(Arrays.asList(jointSource.nameArray.names));
                } else if(jointSource.idrefArray != null) {
                    boneRefArray = new ArrayList<String>(Arrays.asList(jointSource.idrefArray.idrefs));
                }

                for (String boneId : boneRefArray) {
                    boneIdLut.add(boneId);
                }
            }
        }

        // Next, add the remaining bones from the visual scene. These are bones that don't have animation,
        // their order/indices are not relevant why they are added last.
        if (!collada.libraryVisualScenes.isEmpty()) {
            Matrix4d t = new Matrix4d();
            XMLNode rootNode = findSkeletonNode(collada, t);
            if (rootNode != null) {
                getBoneReferenceFromSceneNode(rootNode, boneRefArray, boneIdLut);
            }
        }

        return boneRefArray;
    }

    // Recurses through JOINT nodes and fills the boneTransform map with their local transforms.
    // If a joint/bone has an entry in the invBoneTransforms map it will first convert the stored
    // entry from asset space into local space and then store the result in boneTransform.
    private static void fillBoneTransformsFromVisualScene(XMLNode node, Matrix4d parentTransform, HashMap<String, Matrix4d> boneTransforms, HashMap<String, Matrix4d> invBoneTransforms) {
        Matrix4d sceneNodeTransform = new Matrix4d(MathUtil.vecmath2ToVecmath1(node.matrix.matrix4f));

        if (invBoneTransforms.containsKey(node.id)) {
            // If this bone exist in the invBoneTransform map
            Matrix4d boneTransform = new Matrix4d(invBoneTransforms.get(node.id));
            boneTransform.invert();
            Matrix4d invParentTransform = new Matrix4d(parentTransform);
            invParentTransform.invert();
            boneTransform.mul(invParentTransform, boneTransform);
            sceneNodeTransform = boneTransform;
        }

        // Calculate next parentTransform
        Matrix4d newParentTransform = new Matrix4d(parentTransform);
        newParentTransform.mul(parentTransform, sceneNodeTransform);

        boneTransforms.put(node.id, sceneNodeTransform);


        for(XMLNode childNode : node.childrenList) {
            if(childNode.type == XMLNode.Type.JOINT) {
                fillBoneTransformsFromVisualScene(childNode, newParentTransform, boneTransforms, invBoneTransforms);
            }
        }
    }

    // Create a map of bone id to bone transform (local space).
    // Needed to have correct local bone transforms before creating bone hierarchy in loadSkeleton.
    // This function reads from both the skin and visual scene entries in the collada file
    // to get correct transforms for both animated and non-animated bones.
    private static HashMap<String, Matrix4d> createBoneTransformsMap(XMLCOLLADA collada) throws LoaderException
    {
        // Final map with bone-name -> bone-transform (in local space, relative to its parent)
        HashMap<String, Matrix4d> boneTransforms = new HashMap<String, Matrix4d>();

        // Map used to keep track of asset space inv pose transforms for bones with animation.
        HashMap<String, Matrix4d> invBoneTransforms = new HashMap<String, Matrix4d>();

        // First we try to extract the inverse bone bind poses from the skin entry
        if (!collada.libraryControllers.isEmpty()) {
            XMLSkin skin = findFirstSkin(collada.libraryControllers.get(0));

            if (skin != null) {

                List<XMLSource> sources = skin.sources;
                HashMap<String, XMLSource> sourcesMap = getSourcesMap(sources);

                XMLInput invBindMatInput = findInput(skin.jointsInputs, "INV_BIND_MATRIX", true);
                XMLSource invBindMatSource = sourcesMap.get(invBindMatInput.source);
                if (invBindMatSource != null) {
                    float[] invBindMatFloats = invBindMatSource.floatArray.floats;

                    // Get the list of bone names and their corresponding index from the order they are stored in the INV_BIND_MATRIX.
                    // We need this inside the loop below so we can map them from bone name to bone inv pose matrix.
                    List<String> boneIdList = createBoneReferenceList(collada);

                    int matricesCount = invBindMatFloats.length / 16;
                    for (int i = 0; i < matricesCount; i++)
                    {
                        int j = i*16;
                        String boneId = boneIdList.get(i);
                        Matrix4d invBoneTransform = new Matrix4d(new Matrix4f(ArrayUtils.subarray(invBindMatFloats, j, j + 16)));
                        invBoneTransforms.put(boneId, invBoneTransform);
                    }
                }
            }
        }

        XMLNode rootNode = findSkeletonNode(collada, new Matrix4d());

        // Now we need to extract any missing bone transforms that wasn't available in the skin entry, such as non-animated bones.
        // Bones with animations have their bind pose stored as inverse matrices in asset space (see above loop),
        // but Defold stores bone transforms in local space relative to each parent so we need to "undo" this for these bones.
        if (rootNode != null) {
            Matrix4d parentTransform = new Matrix4d();
            parentTransform.setIdentity();
            fillBoneTransformsFromVisualScene(rootNode, parentTransform, boneTransforms, invBoneTransforms);
        }

        return boneTransforms;
    }

    public static ArrayList<Bone> loadSkeleton(XMLCOLLADA collada, ArrayList<String> boneIds) throws IOException, XMLStreamException, LoaderException {
        if (collada.libraryVisualScenes.size() != 1) {
            return null;
        }

        Matrix4d skeletonTransform = new Matrix4d();
        skeletonTransform.setIdentity();
        XMLNode rootNode = findSkeletonNode(collada, skeletonTransform);

        // We cannot continue if there is no "root" joint node,
        // since it's needed to build up the skeleton hierarchy.
        if (rootNode == null) {
            return null;
        }

        // Get bind pose matrices for all bones
        HashMap<String, Matrix4d> boneTransforms = createBoneTransformsMap(collada);

        // Get the asset space information. Unit/scale will be applied to each bone,
        // up axis rotation will be applied to the root bone (inside loadBone).
        AssetSpace assetSpace = getAssetSpace(collada.asset);

        // Recurse from the root bone/joint and build skeleton hierarchy of bones.
        ArrayList<Bone> boneList = new ArrayList<Bone>();
        loadBone(rootNode, boneList, boneIds, assetSpace, boneTransforms);
        return boneList;
    }

    public static void loadSkeleton(XMLCOLLADA collada, com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder, ArrayList<String> boneIds) throws IOException, XMLStreamException, LoaderException {
        ArrayList<Bone> boneList = loadSkeleton(collada, boneIds);

        // No bones found, cannot create a skeleton with zero bones.
        if (boneList == null) {
            return;
        }

        Matrix4d identity = new Matrix4d();
        identity.setIdentity();

        // Generate DDF representation of bones.
        ArrayList<com.dynamo.rig.proto.Rig.Bone> ddfBones = new ArrayList<com.dynamo.rig.proto.Rig.Bone>();
        toDDF(ddfBones, boneList.get(0), BONE_NO_PARENT, identity);
        skeletonBuilder.addAllBones(ddfBones);
    }

    // Finds first occurrence of JOINT in a subtree of the visual scene nodes.
    private static XMLNode findFirstSkeleton(XMLNode node, Matrix4d transform) throws LoaderException {
        if(node.type == XMLNode.Type.JOINT) {
            return node;
        }
        Matrix4d nodeTrans = new Matrix4d(MathUtil.vecmath2ToVecmath1(node.matrix.matrix4f));
        Matrix4d t = new Matrix4d(transform);
        t.mul(nodeTrans);
        XMLNode rootNode = null;
        if(!node.childrenList.isEmpty()) {
            for(XMLNode childNode : node.childrenList) {
                XMLNode skeletonNode = findFirstSkeleton(childNode, t);
                if(skeletonNode != null) {
                    if (rootNode != null) {
                        throw new LoaderException("Found multiple root bones! Only one root bone for skeletons is supported.");
                    }

                    rootNode = skeletonNode;
                    transform.set(t);
                }
            }
        }
        return rootNode;
    }

    // Find first occurrence of JOINT in the visual scene.
    // It will throw an error if there are more than one "root" JOINT since we only support one root currently.
    // This function also calculates the asset space transform in the scene up until the JOINT node.
    private static XMLNode findSkeletonNode(XMLCOLLADA collada, Matrix4d transform) throws LoaderException {
        XMLNode rootSkeletonNode = null;
        for ( XMLVisualScene scene : collada.libraryVisualScenes.get(0).scenes.values() ) {
            for ( XMLNode node : scene.nodes.values() ) {
                XMLNode skeletonNode = findFirstSkeleton(node, transform);
                if (skeletonNode != null) {
                    if (rootSkeletonNode != null) {
                        throw new LoaderException("Found multiple root bones! Only one root bone for skeletons is supported.");
                    }
                    rootSkeletonNode = skeletonNode;
                }
            }
        }
        return rootSkeletonNode;
    }

    private static boolean validateMatrix4d(Matrix4d m) {
        try {
            Matrix4d inv = new Matrix4d(m);
            inv.invert();
            return true;
        } catch(javax.vecmath.SingularMatrixException e) {
            return false;
        }
    }

    // Generate skeleton DDF data of bones.
    // It will extract the position, rotation and scale from the bone transform as needed by the runtime.
    private static void toDDF(ArrayList<com.dynamo.rig.proto.Rig.Bone> ddfBones, Bone bone, int parentIndex, Matrix4d parentWorldTransform) {
        com.dynamo.rig.proto.Rig.Bone.Builder b = com.dynamo.rig.proto.Rig.Bone.newBuilder();

        // We'd like to do it outside, but since the Bone instances are read-only, we cannot do it.
        if (parentIndex == BONE_NO_PARENT) {
            b.setName("root");
            b.setId(MurmurHash.hash64("root"));
        } else {
            b.setName(bone.getName());
            b.setId(MurmurHash.hash64(bone.getSourceId()));
        }

        b.setParent(parentIndex);

        Matrix4d localMatrix = getBoneLocalToParent(bone);
        if (!validateMatrix4d(localMatrix)) {
            logger.severe(String.format("Found invalid local matrix in bone '%s', replacing with identity matrix", bone.getName()));
            printMatrix4d(localMatrix);
            localMatrix.setIdentity();
        }

        Matrix4d worldMatrix = new Matrix4d();
        worldMatrix.mul(parentWorldTransform, localMatrix);

        if (!validateMatrix4d(worldMatrix)) {
            logger.severe(String.format("Found invalid world matrix in bone '%s', replacing with identity matrix", bone.getName()));
            printMatrix4d(worldMatrix);
            worldMatrix.setIdentity();
        }

        Matrix4d invBindMatrix = new Matrix4d(worldMatrix);
        invBindMatrix.invert();

        /*
         * Decompose pos, rot and scale from bone matrix.
         */
        Vector3d position = new Vector3d();
        Quat4d rotation = new Quat4d();
        Vector3d scale = new Vector3d();
        MathUtil.decompose(localMatrix, position, rotation, scale);

        Transform local = MathUtil.vecmathToDDF(position, rotation, scale);
        b.setLocal(local);

        MathUtil.decompose(worldMatrix, position, rotation, scale);
        Transform world = MathUtil.vecmathToDDF(position, rotation, scale);
        b.setWorld(world);

        MathUtil.decompose(invBindMatrix, position, rotation, scale);
        Transform inv_bind_pose = MathUtil.vecmathToDDF(position, rotation, scale);
        b.setInverseBindPose(inv_bind_pose);

        /*
         * Collada "bones" are just joints and don't have any length.
         */
        b.setLength(0.0f);

        /*
         * Add bone to bone list and keep track of bone id (we use "sid"/"source id").
         */
        parentIndex = ddfBones.size();
        ddfBones.add(b.build());

        for(int i = 0; i < bone.numChildren(); i++) {
            Bone childBone = bone.getChild(i);
            toDDF(ddfBones, childBone, parentIndex, worldMatrix);
        }
    }

    // Finds first skin in the controllers entry.
    private static XMLSkin findFirstSkin(XMLLibraryControllers controllers) {
        for(XMLController controller : controllers.controllers.values()) {
            if(controller.skin != null) {
                return controller.skin;
            }
        }
        return null;
    }

    private static int loadVertexWeights(XMLCOLLADA collada, List<Float> boneWeightsList, List<Integer> boneIndicesList) throws IOException, XMLStreamException, LoaderException {

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

        XMLInput jointInput = findInput(skin.jointsInputs, "JOINT", true);
        XMLSource jointSource = sourcesMap.get(jointInput.source);

        ArrayList<String> jointsArray = new ArrayList<>();
        if(jointSource.nameArray != null)
            jointsArray = new ArrayList<>(Arrays.asList(jointSource.nameArray.names));

        HashMap<String, Integer> toJointIndex = new HashMap<String, Integer>();
        for (int i = 0; i < jointsArray.size(); i++) {
            toJointIndex.put(jointsArray.get(i), i);
        }

        // Load skeleton to get correct bone indices
        ArrayList<String> boneIds = new ArrayList<String>();
        ArrayList<Bone> boneList = loadSkeleton(collada, boneIds);

        HashMap<Integer, Integer> toBoneIndex = new HashMap<Integer, Integer>();

        int boneIndex = 0;
        for (String boneId : boneIds) {
            // The list of joints may be shorter than the actual skeleton
            // It's ok, it just means that the bone influences won't reference the bone
            int jointIndex = toJointIndex.getOrDefault(boneId, 10000);
            toBoneIndex.put(jointIndex, boneIndex);
            ++boneIndex;
        }

        // Convert to bone indices
        for (int i = 0; i < boneIndicesList.size(); ++i)
        {
            int oldIndex = boneIndicesList.get(i);
            int newIndex = toBoneIndex.get(oldIndex);
            boneIndicesList.set(i, newIndex);
        }

        return maxBoneCount;
    }

    // ************************************************************
    // Editor API to mimic the ModelUtil importer

    public static XMLCOLLADA loadScene(InputStream is) throws IOException, XMLStreamException, LoaderException {
        return loadDAE(is);
    }

    private static void flattenBoneTree(Bone colladaBone, ModelImporter.Bone parent, ArrayList<ModelImporter.Bone> out) {
        String originalName = colladaBone.getSourceId();
        String newName = originalName;
        if (parent == null)
            newName = "root";

        ModelImporter.Bone bone = new ModelImporter.Bone();
        bone.name           = newName;
        bone.parent         = parent;
        bone.node           = null;
        bone.index          = out.size();
        bone.invBindPose    = null;

        out.add(bone);

        for (int i = 0 ; i < colladaBone.numChildren(); ++i) {
            Bone child = colladaBone.getChild(i);
            flattenBoneTree(child, bone, out);
        }
    }

    public static ArrayList<ModelImporter.Bone> loadSkeleton(XMLCOLLADA scene) throws IOException, XMLStreamException, LoaderException  {
        ArrayList<String> boneIds = new ArrayList<>();
        ArrayList<Bone> colladaBones = loadSkeleton(scene, boneIds);

        if (colladaBones == null)
            return null;

        ArrayList<ModelImporter.Bone> bones = new ArrayList<>();
        flattenBoneTree(colladaBones.get(0), null, bones);
        return bones;
    }

    public static ArrayList<ModelImporter.Bone> loadSkeleton(byte[] content) throws IOException, IOException {
        try {
            XMLCOLLADA collada = loadDAE(new ByteArrayInputStream(content));
            return loadSkeleton(collada);
        }
        catch (XMLStreamException|LoaderException e) {
            throw new IOException("Failed to load skeleton", e);
        }
    }

    public static ArrayList<String> loadMaterialNames(XMLCOLLADA scene) {
        return new ArrayList<String>();
    }

    public static void loadSkeleton(XMLCOLLADA collada, com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder) throws IOException {
        ArrayList<Bone> boneList = null;
        try {
            boneList = loadSkeleton(collada, new ArrayList<String>());
        }
        catch (XMLStreamException|LoaderException e) {
            throw new IOException("Failed to load skeleton", e);
        }

        // No bones found, cannot create a skeleton with zero bones.
        if (boneList == null) {
            return;
        }

        Matrix4d identity = new Matrix4d();
        identity.setIdentity();

        // Generate DDF representation of bones.
        ArrayList<com.dynamo.rig.proto.Rig.Bone> ddfBones = new ArrayList<>();
        toDDF(ddfBones, boneList.get(0), BONE_NO_PARENT, identity);
        skeletonBuilder.addAllBones(ddfBones);
    }

    public static void loadModels(XMLCOLLADA scene, Rig.MeshSet.Builder meshSetBuilder) throws IOException, XMLStreamException, LoaderException {
        try {
            loadMesh(scene, meshSetBuilder, true, false);
        } catch(IOException e) {
            throw e;
        } catch(Exception e) {
            throw new IOException("Failed to load Collada scene: " + e.getMessage(), e);
        }
    }


// $ java -cp ~/work/defold/tmp/dynamo_home/share/java/bob-light.jar com.dynamo.bob.pipeline.ColladaUtil model_asset.dae
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

        XMLCOLLADA scene;
        try {
            long timeStart = System.currentTimeMillis();

            InputStream is = new FileInputStream(file);
            scene = loadScene(is);

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

        // System.out.printf("--------------------------------------------\n");
        // System.out.printf("Scene Nodes:\n");

        // for (Node node : scene.nodes) {
        //     System.out.printf("  Scene Node: %s  index: %d  id: %d  parent: %s\n", node.name, node.index, ModelImporter.AddressOf(node), node.parent != null ? node.parent.name : "");
        //     System.out.printf("      local: id: %d\n", ModelImporter.AddressOf(node.local));
        //     ModelImporter.DebugPrintTransform(node.local, 3);
        // }


        // if (scene.skins.length > 0)
        // {
        //     System.out.printf("--------------------------------------------\n");
        //     System.out.printf("Scene Bones:\n");

        //     for (Bone bone : scene.skins[0].bones) {
        //         System.out.printf("  Scene Bone: %s  index: %d  id: %d  nodeid: %d  parent: %s\n", bone.name, bone.index,
        //                                     ModelImporter.AddressOf(bone), ModelImporter.AddressOf(bone.node),
        //                                     bone.parent != null ? bone.parent.name : "");
        //         System.out.printf("      local: id: %d\n", ModelImporter.AddressOf(bone.node.local));
        //         ModelImporter.DebugPrintTransform(bone.node.local, 3);
        //         System.out.printf("      inv_bind_poser:\n");
        //         ModelImporter.DebugPrintTransform(bone.invBindPose, 3);
        //     }

        //     System.out.printf("--------------------------------------------\n");
        // }

        try {
            System.out.printf("Bones:\n");
            ArrayList<ModelImporter.Bone> bones = loadSkeleton(scene);
            if (bones != null) {
                for (ModelImporter.Bone bone : bones) {
                    System.out.printf("  Bone: %s  index: %d  parent: %s node: %s\n", bone.name, bone.index, bone.parent != null ? bone.parent.name : "null", bone.node != null ? bone.node.name : "null");
                    if (bone.node != null) {
                        System.out.printf("      local:\n");
                        ModelImporter.DebugPrintTransform(bone.node.local, 3);
                    }
                }
                System.out.printf("--------------------------------------------\n");
            }

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
            loadAnimations(scene, animationSetBuilder, file.getName(), animationIds);

            for (String animation : animationIds) {
                System.out.printf("  Animation: %s\n", animation);
            }
            System.out.printf("--------------------------------------------\n");


            // Something a bit more verbose
            for (int i = 0; i < meshSetBuilder.getModelsCount(); ++i) {
                Rig.Model model = meshSetBuilder.getModels(i);

                for (int m = 0; m < model.getMeshesCount(); ++m) {
                    Rig.Mesh mesh = model.getMeshes(m);

                    int positionCount = mesh.getPositionsCount();
                    if (positionCount > 5)
                        positionCount = 5;
                    for (int v = 0; v < positionCount; ++v) {
                        System.out.printf("pos %d: %f, %f, %f\n", v, mesh.getPositions(v*3+0), mesh.getPositions(v*3+1),mesh.getPositions(v*3+2));
                    }
                }
            }

        } catch (Exception e) {
            System.out.printf("Failed reading '%s':\n%s\n", file, e.getMessage());
            e.printStackTrace();
            return;
        }
    }

}
