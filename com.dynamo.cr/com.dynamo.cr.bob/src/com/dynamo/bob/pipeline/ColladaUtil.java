package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map.Entry;

import org.apache.commons.lang.ArrayUtils;
import org.jagatoo.loaders.models.collada.datastructs.animation.Bone;

import javax.vecmath.AxisAngle4d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Matrix4f;
import javax.vecmath.Point3d;
import javax.vecmath.Point3f;
import javax.vecmath.Quat4d;
import javax.vecmath.Quat4f;
import javax.vecmath.Tuple3d;
import javax.vecmath.Tuple4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector3f;
import javax.xml.stream.XMLInputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.loaders.models.collada.stax.XMLAnimation;
import org.jagatoo.loaders.models.collada.stax.XMLAnimationClip;
import org.jagatoo.loaders.models.collada.stax.XMLCOLLADA;
import org.jagatoo.loaders.models.collada.stax.XMLController;
import org.jagatoo.loaders.models.collada.stax.XMLGeometry;
import org.jagatoo.loaders.models.collada.stax.XMLInput;
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
import org.openmali.FastMath;


import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.RigUtil;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.RigUtil.AnimationKey;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.proto.DdfMath.Vector3;

import com.dynamo.rig.proto.Rig;

public class ColladaUtil {

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

    public static boolean load(InputStream is, Rig.Mesh.Builder meshBuilder, Rig.AnimationSet.Builder animationSetBuilder, Rig.Skeleton.Builder skeletonBuilder) throws IOException, XMLStreamException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        loadMesh(collada, meshBuilder);
        ArrayList<Bone> boneList = loadSkeleton(collada, skeletonBuilder, new ArrayList<String>());
        loadAnimations(collada, animationSetBuilder, boneList, 30.0f, new ArrayList<String>()); // TODO pick the sample rate from a parameter
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

    private static void ExtractRotateKeys(Bone bone, Matrix4d parentToLocal, XMLAnimation animation, RigUtil.AnimationTrack rotTrack) {
        int keyCount = animation.getInput().length;
        Vector3d axis = new Vector3d(0.0, 0.0, 0.0);
        switch (animation.getRotationAxis()) {
        case X: axis.setX(1.0); break;
        case Y: axis.setY(1.0); break;
        case Z: axis.setZ(1.0); break;
        }
        parentToLocal.transform(axis);
        AxisAngle4d axisAngle = new AxisAngle4d(axis, 0.0);
        float[] times = animation.getInput();
        float[] values = animation.getOutput();
        Quat4f q = new Quat4f();
        for (int key = 0; key < keyCount; ++key) {
            axisAngle.setAngle(FastMath.toRad(values[key]));
            q.set(axisAngle);
            AnimationKey rotKey = createKey(times[key], false, 4);
            q.get(rotKey.value);
            rotTrack.keys.add(rotKey);
        }
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

    private static void ExtractMatrixKeys(Bone bone, Matrix4d parentToLocal, XMLAnimation animation, RigUtil.AnimationTrack posTrack, RigUtil.AnimationTrack rotTrack, RigUtil.AnimationTrack scaleTrack) {
        int keyCount = animation.getInput().length;
        float[] time = animation.getInput();
        float[] values = animation.getOutput();
        for (int key = 0; key < keyCount; ++key) {
            int index = key * 16;
            Matrix4d m = MathUtil.floatToDouble(new Matrix4f(ArrayUtils.subarray(values, index, index + 16)));
            m.mul(parentToLocal, m);
            Vector3d p = new Vector3d();
            Quat4d r = new Quat4d();
            Vector3d s = new Vector3d();
            MathUtil.decompose(m, p, r, s);
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

    private static void ExtractKeys(Bone bone, Matrix4d parentToLocal, XMLAnimation animation, RigUtil.AnimationTrack posTrack, RigUtil.AnimationTrack rotTrack, RigUtil.AnimationTrack scaleTrack) throws LoaderException {
        switch (animation.getType()) {
        case TRANSLATE:
        case SCALE:
        case ROTATE:
            throw new LoaderException("Currently only collada files with matrix animations are supported.");
        case TRANSFORM:
        case MATRIX:
            ExtractMatrixKeys(bone, parentToLocal, animation, posTrack, rotTrack, scaleTrack);
            break;
        default:
            throw new LoaderException(String.format("Animations of type %s are not supported.", animation.getType().name()));
        }
    }

    private static Matrix4d getBoneParentToLocal(Bone bone) {
        return MathUtil.floatToDouble(MathUtil.vecmath2ToVecmath1(bone.invBindMatrix));
    }

    private static void samplePosTrack(Rig.RigAnimation.Builder animBuilder, int boneIndex, RigUtil.AnimationTrack track, double duration, double sampleRate, double spf, boolean interpolate, boolean slerp) {
        if (!track.keys.isEmpty()) {
            Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
            animTrackBuilder.setBoneIndex(boneIndex);
            RigUtil.PositionBuilder positionBuilder = new RigUtil.PositionBuilder(animTrackBuilder);
            RigUtil.sampleTrack(track, positionBuilder, new Point3d(0.0, 0.0, 0.0), duration, sampleRate, spf, true, slerp);
            animBuilder.addTracks(animTrackBuilder.build());
        }
    }

    private static void sampleRotTrack(Rig.RigAnimation.Builder animBuilder, int boneIndex, RigUtil.AnimationTrack track, double duration, double sampleRate, double spf, boolean interpolate, boolean slerp) {
        if (!track.keys.isEmpty()) {
            Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
            animTrackBuilder.setBoneIndex(boneIndex);
            RigUtil.QuatRotationBuilder rotationBuilder = new RigUtil.QuatRotationBuilder(animTrackBuilder);
            RigUtil.sampleTrack(track, rotationBuilder, new Quat4d(0.0, 0.0, 0.0, 1.0), duration, sampleRate, spf, true, slerp);
            animBuilder.addTracks(animTrackBuilder.build());
        }
    }

    private static void sampleScaleTrack(Rig.RigAnimation.Builder animBuilder, int boneIndex, RigUtil.AnimationTrack track, double duration, double sampleRate, double spf, boolean interpolate, boolean slerp) {
        if (!track.keys.isEmpty()) {
            Rig.AnimationTrack.Builder animTrackBuilder = Rig.AnimationTrack.newBuilder();
            animTrackBuilder.setBoneIndex(boneIndex);
            RigUtil.ScaleBuilder scaleBuilder = new RigUtil.ScaleBuilder(animTrackBuilder);
            RigUtil.sampleTrack(track, scaleBuilder, new Vector3d(1.0, 1.0, 1.0), duration, sampleRate, spf, true, slerp);
            animBuilder.addTracks(animTrackBuilder.build());
        }
    }

    private static void boneAnimToDDF(XMLCOLLADA collada, Rig.RigAnimation.Builder animBuilder, ArrayList<Bone> boneList, HashMap<String, ArrayList<XMLAnimation>> boneToAnimations, float duration, float sampleRate) throws LoaderException {
        animBuilder.setDuration(duration);
        animBuilder.setSampleRate(sampleRate);

        if (boneList == null) {
            return;
        }

        // loop through each bone
        double spf = 1.0 / sampleRate;
        for (int bi = 0; bi < boneList.size(); ++bi)
        {
            Bone bone = boneList.get(bi);
            if (boneToAnimations.containsKey(bone.getName()))
            {
                Matrix4d parentToLocal = getBoneParentToLocal(bone);

                // search the animations for each bone
                ArrayList<XMLAnimation> anims = boneToAnimations.get(bone.getName());
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

                    ExtractKeys(bone, parentToLocal, animation, posTrack, rotTrack, scaleTrack);

                    samplePosTrack(animBuilder, bi, posTrack, duration, sampleRate, spf, true, false);
                    sampleRotTrack(animBuilder, bi, rotTrack, duration, sampleRate, spf, true, true);
                    sampleScaleTrack(animBuilder, bi, scaleTrack, duration, sampleRate, spf, true, false);
                }
            }
        }
    }

    public static void loadAnimationIds(InputStream is, ArrayList<String> animationIds) throws IOException, XMLStreamException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        ArrayList<XMLLibraryAnimationClips> animClips = collada.libraryAnimationClips;
        if(animClips.isEmpty()) {
            animationIds.clear();
            if(!collada.libraryAnimations.isEmpty()) {
                animationIds.add("Default");
            }
            return;
        }
        for (XMLAnimationClip clip : animClips.get(0).animationClips.values()) {
            if (clip.name != null) {
                animationIds.add(clip.name);
            } else if (clip.id != null) {
                animationIds.add(clip.id);
            } else {
                throw new LoaderException("Animation clip lacks name and id.");
            }
        }
    }

    public static void loadAnimations(InputStream is, Rig.AnimationSet.Builder animationSetBuilder, ArrayList<Bone> boneList, float sampleRate, ArrayList<String> animationIds) throws IOException, XMLStreamException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        loadAnimations(collada, animationSetBuilder, boneList, sampleRate, animationIds);
    }

    public static void loadAnimations(XMLCOLLADA collada, Rig.AnimationSet.Builder animationSetBuilder, ArrayList<Bone> boneList, float sampleRate, ArrayList<String> animationIds) throws IOException, XMLStreamException, LoaderException {
        if (collada.libraryAnimations.size() != 1) {
            return;
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
                String boneTarget = animation.getTargetBone();
                if (!boneToAnimations.containsKey(animation.getTargetBone())) {
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
            boneAnimToDDF(collada, animBuilder, boneList, boneToAnimations, totalAnimationLength, sampleRate);
            animBuilder.setId(MurmurHash.hash64("Default"));
            animationIds.add("Default");
            animationSetBuilder.addAnimations(animBuilder.build());
        }
    }

    public static void loadMesh(InputStream is, Rig.Mesh.Builder meshBuilder) throws IOException, XMLStreamException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        loadMesh(collada, meshBuilder);
    }


    public static void loadMesh(XMLCOLLADA collada, Rig.Mesh.Builder meshBuilder) throws IOException, XMLStreamException, LoaderException {
        if (collada.libraryGeometries.size() != 1) {
            if (collada.libraryGeometries.isEmpty()) {
                return;
            }
            throw new LoaderException("Only a single geometry is supported");
        }
        XMLGeometry geom = collada.libraryGeometries.get(0).geometries.values()
                .iterator().next();
        XMLMesh mesh = geom.mesh;
        List<XMLSource> sources = mesh.sources;
        HashMap<String, XMLSource> sourcesMap = getSourcesMap(sources);

        XMLInput vpos_input = findInput(mesh.vertices.inputs, "POSITION", true);
        XMLInput vertex_input = findInput(mesh.triangles.inputs, "VERTEX", true);
        XMLInput normal_input = findInput(mesh.triangles.inputs, "NORMAL", false);
        XMLInput texcoord_input = findInput(mesh.triangles.inputs, "TEXCOORD", false);

        // Get bind shape from skin, if it exist
        Matrix4f bindShapeMatrix = new Matrix4f();
        bindShapeMatrix.setIdentity();
        XMLSkin skin = null;
        if (!collada.libraryControllers.isEmpty()) {
            skin = findFirstSkin(collada.libraryControllers.get(0));
            if (skin != null && skin.bindShapeMatrix != null) {
                bindShapeMatrix.set(MathUtil.vecmath2ToVecmath1(skin.bindShapeMatrix.matrix4f));
            }
        }

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

        float meter = collada.asset.unit.meter;
        List<Float> position_list = new ArrayList<Float>(positions.floatArray.count);
        for (int i = 0; i < positions.floatArray.count / 3; ++i) {
            Point3f p = new Point3f(positions.floatArray.floats[i*3], positions.floatArray.floats[i*3+1], positions.floatArray.floats[i*3+2]);
            p.scale(meter);
            bindShapeMatrix.transform(p);
            position_list.add(p.getX());
            position_list.add(p.getY());
            position_list.add(p.getZ());
        }

        List<Float> normal_list;
        if(normals == null) {
            normal_list = new ArrayList<Float>(Arrays.asList(0f, 0f, 1f));
        } else {
            normal_list = new ArrayList<Float>(normals.floatArray.count);
            for (int i = 0; i < normals.floatArray.count; ++i) {
                normal_list.add(normals.floatArray.floats[i]);
            }
        }

        List<Float> texcoord_list;
        if(texcoords == null) {
            texcoord_list = new ArrayList<Float>(Arrays.asList(0f, 0f));
        } else {
            texcoord_list = new ArrayList<Float>(texcoords.floatArray.count);
            if(collada.asset.upAxis.equals(UpAxis.Y_UP)) {
                for (int i = 0; i < texcoords.floatArray.count; i += 2 ) {
                    texcoord_list.add(texcoords.floatArray.floats[i]);
                    texcoord_list.add(1-texcoords.floatArray.floats[i+1]);
                }
            } else {
                for (int i = 0; i < texcoords.floatArray.count; i += 2 ) {
                    texcoord_list.add(texcoords.floatArray.floats[i]);
                    texcoord_list.add(texcoords.floatArray.floats[i+1]);
                }
            }
        }

        List<Integer> position_indices_list = new ArrayList<Integer>(mesh.triangles.count*3);
        List<Integer> normal_indices_list = new ArrayList<Integer>(mesh.triangles.count*3);
        List<Integer> texcoord_indices_list = new ArrayList<Integer>(mesh.triangles.count*3);
        for (int i = 0; i < mesh.triangles.count; ++i) {

            for (int j = 0; j < 3; ++j) {
                int idx = i * stride * 3 + vertex_input.offset;
                int vert_idx = mesh.triangles.p[idx + stride * j];
                position_indices_list.add(vert_idx);

                if (normals == null) {
                    normal_indices_list.add(0);
                } else {
                    idx = i * stride * 3 + normalOffset;
                    vert_idx = mesh.triangles.p[idx + stride * j];
                    normal_indices_list.add(vert_idx);
                }

                if (texcoords == null) {
                    texcoord_indices_list.add(0);
                } else {
                    idx = i * stride * 3 + texcoord_input.offset;
                    vert_idx = mesh.triangles.p[idx + stride * j];
                    texcoord_indices_list.add(vert_idx);
                }

            }

        }

        List<Integer> bone_indices_list = new ArrayList<Integer>(position_list.size()*4);
        List<Float> bone_weights_list = new ArrayList<Float>(position_list.size()*4);
        loadVertexWeights(collada, position_indices_list, bone_weights_list, bone_indices_list);

        meshBuilder.addAllPositions(position_list);
        meshBuilder.addAllNormals(normal_list);
        meshBuilder.addAllTexcoord0(texcoord_list);
        meshBuilder.addAllIndices(position_indices_list);
        meshBuilder.addAllNormalsIndices(normal_indices_list);
        meshBuilder.addAllTexcoord0Indices(texcoord_indices_list);
        meshBuilder.addAllWeights(bone_weights_list);
        meshBuilder.addAllBoneIndices(bone_indices_list);
    }


    public static ArrayList<Bone> loadSkeleton(InputStream is, com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder, ArrayList<String> boneIds) throws IOException, XMLStreamException, LoaderException {
        return loadSkeleton(loadDAE(is), skeletonBuilder, boneIds);
    }

    private static XMLNode findJoint(XMLNode node, String sid) {
        if(node.sid.equals(sid)) {
            return node;
        }
        for(XMLNode childNode : node.childrenList) {
            XMLNode n = findJoint(childNode, sid);
            if(n != null) {
                return n;
            }
        }
        assert(false);
        return null;
    }

    public static ArrayList<Bone> loadSkeleton(XMLCOLLADA collada, com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder, ArrayList<String> boneIds) throws IOException, XMLStreamException, LoaderException {
        if (collada.libraryVisualScenes.size() != 1) {
            return null;
        }

        XMLNode rootNode = null;
        for ( XMLVisualScene scene : collada.libraryVisualScenes.get(0).scenes.values() ) {
            for ( XMLNode node : scene.nodes.values() ) {
                rootNode = findFirstSkeleton(node);
                if(rootNode != null) {
                    break;
                }
            }
        }

        if (rootNode == null) {
            return null;
        }

        ArrayList<Bone> boneList = new ArrayList<Bone>();

        HashMap<String, com.dynamo.rig.proto.Rig.Bone> bones = new HashMap<String, com.dynamo.rig.proto.Rig.Bone>();

        XMLSkin skin = null;
        if (!collada.libraryControllers.isEmpty()) {
            skin = findFirstSkin(collada.libraryControllers.get(0));
        }
        if(skin == null) {
            return null;
        }
        List<XMLSource> sources = skin.sources;
        HashMap<String, XMLSource> sourcesMap = getSourcesMap(sources);
        XMLInput invBindMatInput = findInput(skin.jointsInputs, "INV_BIND_MATRIX", true);
        XMLSource invBindMatSource = sourcesMap.get(invBindMatInput.source);


        XMLInput joint_input = findInput(skin.jointsInputs, "JOINT", true);
        XMLSource jointSource = sourcesMap.get(joint_input.source);

        HashMap<String, Integer> boneIndexMap = new HashMap<String, Integer>();
        HashMap<String, Bone> boneNameMap = new HashMap<String, Bone>();

        String boneRefArray[];
        int boneRefArrayCount;
        if(jointSource.nameArray != null) {
            boneRefArray = jointSource.nameArray.names;
            boneRefArrayCount = jointSource.nameArray.count;
        } else if(jointSource.idrefArray != null) {
            boneRefArray = jointSource.idrefArray.idrefs;
            boneRefArrayCount = jointSource.idrefArray.count;
        } else {
            return null;
        }

        for(int i = 0; i < boneRefArrayCount; i++) {
            boneIndexMap.put(boneRefArray[i], i);
            XMLNode n = findJoint(rootNode, boneRefArray[i]);
            Bone newBone = new Bone(n, n.sid, n.name, n.matrix.matrix4f, new org.openmali.vecmath2.Quaternion4f(0f, 0f, 0f, 1f));
            boneList.add(newBone);
            boneNameMap.put(newBone.getSourceId(), newBone);
            int offset = i*16;
            newBone.invBindMatrix2 = new org.openmali.vecmath2.Matrix4f(Arrays.copyOfRange(invBindMatSource.floatArray.floats, offset, offset+16));
        }

        for(Bone bone : boneList) {
            for(XMLNode childNode : bone.node.childrenList) {

                Bone b = boneNameMap.get(childNode.sid);
                if(b == null) {
                    continue;
                }
                bone.addChild(boneNameMap.get(childNode.sid));
            }
        }

        toDDF(bones, boneList.get(0), 0xffff, boneIndexMap, boneIds);

        // need to explicitly add all bones in the correct order before we create the DDF structure
        ArrayList<com.dynamo.rig.proto.Rig.Bone> reorderedBones = new ArrayList<com.dynamo.rig.proto.Rig.Bone>();
        for(int i = 0; i < boneRefArrayCount; i++) {
            reorderedBones.add(bones.get(boneRefArray[i]));
        }
        skeletonBuilder.addAllBones(reorderedBones);

        return boneList;
    }

    private static XMLNode findFirstSkeleton(XMLNode node) {
        if(node.type == XMLNode.Type.JOINT) {
            return node;
        }
        XMLNode rootNode = null;
        if(!node.childrenList.isEmpty()) {
            for(XMLNode childNode : node.childrenList) {
                rootNode = findFirstSkeleton(childNode);
                if(rootNode != null) {
                    break;
                }
            }
        }
        return rootNode;
    }

    private static void toDDF(HashMap<String, com.dynamo.rig.proto.Rig.Bone> builderList, Bone bone, int parentIndex, HashMap<String, Integer> boneIndexMap, ArrayList<String> boneIds) {
        com.dynamo.rig.proto.Rig.Bone.Builder b = com.dynamo.rig.proto.Rig.Bone.newBuilder();

        boneIds.add(bone.getSourceId());

        b.setParent(parentIndex);
        b.setId(MurmurHash.hash64(bone.getName()));

        Matrix4d matrix = MathUtil.floatToDouble(MathUtil.vecmath2ToVecmath1(bone.bindMatrix));

        Vector3d position = new Vector3d();
        Quat4d rotation = new Quat4d();
        Vector3d scale = new Vector3d();
        MathUtil.decompose(matrix, position, rotation, scale);

        Point3 ddfpos = MathUtil.vecmathToDDF(new Point3d(position));
        b.setPosition(ddfpos);

        Quat ddfrot = MathUtil.vecmathToDDF(rotation);
        b.setRotation(ddfrot);

        Vector3 ddfscale = MathUtil.vecmathToDDF(scale);
        b.setScale(ddfscale);

        b.setLength(0.0f);
        b.setInheritScale(true);

        builderList.put(bone.getSourceId(), b.build());


        parentIndex = boneIndexMap.getOrDefault(bone.getSourceId(), 0xffff);

        for(int i = 0; i < bone.numChildren(); i++) {
            Bone childBone = bone.getChild(i);
            toDDF(builderList, childBone, parentIndex, boneIndexMap, boneIds);
        }
    }

    private static XMLSkin findFirstSkin(XMLLibraryControllers controllers) {
        for(XMLController controller : controllers.controllers.values()) {
            if(controller.skin != null) {
                return controller.skin;
            }
        }
        return null;
    }


    private static void loadVertexWeights(XMLCOLLADA collada, List<Integer> position_indices_list, List<Float> bone_weights_list, List<Integer> bone_indices_list) throws IOException, XMLStreamException, LoaderException {

        XMLSkin skin = null;
        if (!collada.libraryControllers.isEmpty()) {
            skin = findFirstSkin(collada.libraryControllers.get(0));
        }
        if(skin == null) {
            return;
        }

        List<XMLSource> sources = skin.sources;
        HashMap<String, XMLSource> sourcesMap = getSourcesMap(sources);

        XMLInput weights_input = findInput(skin.vertexWeights.inputs, "WEIGHT", true);
        XMLSource weightsSource = sourcesMap.get(weights_input.source);

        int vIndex = 0;
        for ( int i = 0; i < skin.vertexWeights.vcount.ints.length; i++ )
        {
            final int numBones = skin.vertexWeights.vcount.ints[ i ];
            int j = 0;
            for (; j < Math.min(numBones, 4); j++ ) {
                float bw = 0f;
                int bi = 0;
                bi = skin.vertexWeights.v.ints[ vIndex + j * 2 + 0 ];
                if (bi != -1) {
                    final int weightIndex = skin.vertexWeights.v.ints[ vIndex + j * 2 + 1 ];
                    bw = weightsSource.floatArray.floats[ weightIndex ];
                    bone_indices_list.add(bi);
                    bone_weights_list.add(bw);
                } else {
                    throw new LoaderException("Invalid bone index when loading vertex weights.");
                }
            }
            for (; j < 4; j++ ) {
                bone_indices_list.add(0);
                bone_weights_list.add(0f);
            }

            vIndex += numBones * 2;
        }
    }


}
