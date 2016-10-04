package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map.Entry;
import org.jagatoo.loaders.models.collada.datastructs.animation.Bone;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.xml.stream.XMLInputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.loaders.models.collada.stax.XMLAnimation;
import org.jagatoo.loaders.models.collada.stax.XMLAnimationClip;
import org.jagatoo.loaders.models.collada.stax.XMLCOLLADA;
import org.jagatoo.loaders.models.collada.stax.XMLChannel.ChannelType;
import org.jagatoo.loaders.models.collada.stax.XMLNode.OperationType;
import org.jagatoo.loaders.models.collada.stax.XMLNode.TransformOperation;
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
import org.openmali.vecmath2.Matrix4f;
import org.openmali.vecmath2.Point3f;
import org.openmali.vecmath2.Quaternion4f;
import org.openmali.vecmath2.Tuple3f;
import org.openmali.vecmath2.Vector3f;
import org.openmali.vecmath2.Vector4f;


import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.RigUtil;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.RigUtil.AnimationCurve;
import com.dynamo.bob.util.RigUtil.AnimationCurve.CurveIntepolation;
import com.dynamo.bob.util.RigUtil.MatrixAnimationKey;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.proto.DdfMath.Vector3;

import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.AnimationTrack;
import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.rig.proto.Rig.RigAnimation;
import com.dynamo.rig.proto.Rig.Skeleton;

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
        XMLStreamReader stream_reader = factory.createXMLStreamReader(is);
        XMLCOLLADA collada = new XMLCOLLADA();
        collada.parse(stream_reader);
        return collada;
    }

    public static boolean load(InputStream is, Mesh.Builder meshBuilder, AnimationSet.Builder animationSetBuilder, Skeleton.Builder skeletonBuilder) throws IOException, XMLStreamException, LoaderException {
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

    private static void boneAnimToDDF(XMLCOLLADA collada, RigAnimation.Builder animBuilder, ArrayList<Bone> boneList, HashMap<String, ArrayList<XMLAnimation>> boneToAnimations, float duration, float sampleRate) throws LoaderException {
        if(boneList == null)
            return;

        animBuilder.setDuration(duration);
        animBuilder.setSampleRate(sampleRate);

        // loop through each bone
        double spf = 1.0 / sampleRate;
        for(int bi = 0; bi < boneList.size(); ++bi)
        {
            Bone bone = boneList.get(bi);
            if (boneToAnimations.containsKey(bone.getName()))
            {
                // search the animations for each bone
                ArrayList<XMLAnimation> anims = boneToAnimations.get(bone.getName());
                for (XMLAnimation animation : anims)
                {
                    if ( animation.getType() == null ) {
                        continue;
                    }
                    System.out.println("animation id: " + animation.channels.get(0).target);

                    AnimationTrack.Builder animTrackBuilder = AnimationTrack.newBuilder();
                    animTrackBuilder.setBoneIndex(bi);
                    switch ( animation.getType() )
                    {
                        case ROTATE :
                        {
                            RigUtil.MatrixAnimationTrack track = new RigUtil.MatrixAnimationTrack();

                            for ( int j = 0; j < animation.getInput().length; j++ )
                            {
                                String operationId = "rotate" + animation.getRotationAxis();

                                float[] data = new float[]{0.0f,0.0f,0.0f,animation.getOutput()[j]};
                                switch ( animation.getRotationAxis() ) {
                                    case X:
                                        data[0] = 1.0f;
                                        break;
                                    case Y:
                                        data[1] = 1.0f;
                                        break;
                                    case Z:
                                        data[2] = 1.0f;
                                        break;
                                }
//                                System.out.println("key t: " + animation.getInput()[j]);
//                                System.out.println("key v: " + animation.getOutput()[j]);
                                TransformOperation t = new TransformOperation(OperationType.ROTATE, operationId, data);
                                HashMap<String, TransformOperation> animationOverrides = new HashMap<String, TransformOperation>();
                                animationOverrides.put(operationId, t);
                                Matrix4f mat = new Matrix4f(Matrix4f.IDENTITY);
                                bone.node.applyTransformOperations(mat, animationOverrides);

                                // Create a animation key from resulting matrix
                                MatrixAnimationKey key = new MatrixAnimationKey();
                                key.t = animation.getInput()[j];
                                key.matrix = mat;
                                AnimationCurve curve = new AnimationCurve();
                                curve.interpolation = CurveIntepolation.LINEAR;
                                key.curve = curve;
                                track.keys.add(key);
                            }
                            ArrayList<Matrix4f> matrixTrack = new ArrayList<Matrix4f>();
                            RigUtil.MatrixPropertyBuilder matrixBuilder = new RigUtil.MatrixPropertyBuilder(matrixTrack);
                            RigUtil.sampleMatrixTrack(track, matrixBuilder, Matrix4f.IDENTITY, (double)duration, (double)sampleRate, (double)spf, true);

                            Matrix4f invPose = new Matrix4f(bone.bindMatrix).invert();

                            for (int i = 0; i < matrixTrack.size(); i++) {
                                Matrix4f matrix = matrixTrack.get(i);
                                matrix.mul(invPose);
                                Quaternion4f quat = new Quaternion4f();
                                quat.set(matrix);
                                animTrackBuilder.addRotations(quat.getA());
                                animTrackBuilder.addRotations(quat.getB());
                                animTrackBuilder.addRotations(quat.getC());
                                animTrackBuilder.addRotations(quat.getD());
                            }

                            animBuilder.addTracks(animTrackBuilder.build());
                        }
                        break;

                        case MATRIX:
                        case TRANSFORM:
                        {
                            RigUtil.MatrixAnimationTrack track = new RigUtil.MatrixAnimationTrack();
                            for ( int j = 0; j < animation.getInput().length; j++ )
                            {
                                String operationId = "transform";
                                if (animation.getType() == ChannelType.MATRIX) {
                                    operationId = "matrix";
                                }

                                float[] data = new float[16];
                                for(int yx = 0; yx < 16; yx++) {
                                        data[yx] = animation.getOutput()[j*16 + yx];
                                }
//                                System.out.println("key t: " + animation.getInput()[j]);
//                                System.out.println("key v: " + animation.getOutput()[j]);
                                TransformOperation t = new TransformOperation(OperationType.MATRIX, operationId, data);
                                HashMap<String, TransformOperation> animationOverrides = new HashMap<String, TransformOperation>();
                                animationOverrides.put(operationId, t);
                                Matrix4f mat = new Matrix4f(Matrix4f.IDENTITY);
                                bone.node.applyTransformOperations(mat, animationOverrides);

                                // Create a animation key from resulting matrix
                                MatrixAnimationKey key = new MatrixAnimationKey();
                                key.t = animation.getInput()[j];
                                key.matrix = mat;
                                AnimationCurve curve = new AnimationCurve();
                                curve.interpolation = CurveIntepolation.LINEAR;
                                key.curve = curve;
                                track.keys.add(key);
                            }

                            ArrayList<Matrix4f> matrixTrack = new ArrayList<Matrix4f>();
                            RigUtil.MatrixPropertyBuilder matrixBuilder = new RigUtil.MatrixPropertyBuilder(matrixTrack);
                            RigUtil.sampleMatrixTrack(track, matrixBuilder, Matrix4f.IDENTITY, (double)duration, (double)sampleRate, (double)spf, true);

                            Matrix4f invPose = new Matrix4f(bone.bindMatrix).invert();

                            for (int i = 0; i < matrixTrack.size(); i++) {
                                Matrix4f matrix = matrixTrack.get(i);
                                matrix = matrix.mul(invPose);
                                Quaternion4f quat = new Quaternion4f();
                                quat.set(matrix);
                                animTrackBuilder.addRotations(quat.getA());
                                animTrackBuilder.addRotations(quat.getB());
                                animTrackBuilder.addRotations(quat.getC());
                                animTrackBuilder.addRotations(quat.getD());
                            }
                            animBuilder.addTracks(animTrackBuilder.build());
                        }
                            break;
                        default:
                            throw new LoaderException("unsuported animation currently!" + animation.getType());
                    }
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

    public static void loadAnimations(InputStream is, AnimationSet.Builder animationSetBuilder, ArrayList<Bone> boneList, float sampleRate, ArrayList<String> animationIds) throws IOException, XMLStreamException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        loadAnimations(collada, animationSetBuilder, boneList, sampleRate, animationIds);
    }

    public static void loadAnimations(XMLCOLLADA collada, AnimationSet.Builder animationSetBuilder, ArrayList<Bone> boneList, float sampleRate, ArrayList<String> animationIds) throws IOException, XMLStreamException, LoaderException {
        if (collada.libraryAnimations.size() != 1) {
            return;
        }

        // Animation clips
        ArrayList<XMLLibraryAnimationClips> animClips = collada.libraryAnimationClips;
        XMLLibraryAnimations libraryAnimation = collada.libraryAnimations.get(0);

        if(!animClips.isEmpty()) {
            /*
            Collection<XMLAnimation> anims = collada.libraryAnimations.get(0).animations.values();
            Iterator<Entry<String, XMLAnimationClip>> clipIt = animClips.get(0).animationClips.entrySet().iterator();
            while (clipIt.hasNext()) {

                XMLAnimationClip clip = (XMLAnimationClip)clipIt.next().getValue();
                String clipName = null;
                if (clip.name != null) {
                    clipName = clip.name;
                } else if (clip.id != null) {
                    clipName = clip.id;
                } else {
                    throw new LoaderException("Animation clip lacks name and id.");
                }

                // Create a boneToAnimation lookup for the current clip
                HashMap<Long, HashMap<String, XMLAnimation>> boneToAnimations = new HashMap<Long, HashMap<String, XMLAnimation>>();
                for (int ci = 0; ci < clip.animations.size(); ci++) {
                    XMLInstanceAnimation inst = clip.animations.get(ci);
                    XMLAnimation animation = libraryAnimation.animations.get(inst.url);

                    String targetPath = animation.channels.get(0).target;
                    String[] targetParts = targetPath.split("/");
                    String boneTarget = targetParts[0];
                    String propertyTarget = targetParts[1];

                    if (!boneToAnimations.containsKey(MurmurHash.hash64(boneTarget))) {
                        HashMap<String, XMLAnimation> animationsEntry = new HashMap<String, XMLAnimation>();
                        boneToAnimations.put(MurmurHash.hash64(boneTarget), animationsEntry);
                    }

                    boneToAnimations.get(MurmurHash.hash64(boneTarget)).put(propertyTarget, animation);
                }

                RigAnimation.Builder animBuilder = RigAnimation.newBuilder();
                boneAnimToDDF(collada, animBuilder, skeleton, boneToAnimations, (float)(clip.end-clip.start), sampleRate);
                animBuilder.setId(MurmurHash.hash64(clipName));
                animationIds.add(clipName);
                animationSetBuilder.addAnimations(animBuilder.build());

            }
            */
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
            RigAnimation.Builder animBuilder = RigAnimation.newBuilder();
            boneAnimToDDF(collada, animBuilder, boneList, boneToAnimations, totalAnimationLength, sampleRate);
            animBuilder.setId(MurmurHash.hash64("Default"));
            animationIds.add("Default");
            animationSetBuilder.addAnimations(animBuilder.build());
        }
    }

    public static void loadMesh(InputStream is, Mesh.Builder meshBuilder) throws IOException, XMLStreamException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        loadMesh(collada, meshBuilder);
    }

    public static void loadMesh(XMLCOLLADA collada, Mesh.Builder meshBuilder) throws IOException, XMLStreamException, LoaderException {
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
            float[] xyz = new float[]{positions.floatArray.floats[i*3], positions.floatArray.floats[i*3+1], positions.floatArray.floats[i*3+2]};
            position_list.add(xyz[0]*meter);
            position_list.add(xyz[1]*meter);
            position_list.add(xyz[2]*meter);
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

        ArrayList<com.dynamo.rig.proto.Rig.Bone> bones = new ArrayList<com.dynamo.rig.proto.Rig.Bone>();
        Vector3f upVector;
        if(collada.asset.upAxis.equals(UpAxis.Y_UP)) {
            upVector = new Vector3f(0.0f, 1.0f, 0.0f);
        } else {
            upVector = new Vector3f(0.0f, 0.0f, 1.0f);
        }

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
            Bone newBone = new Bone( n, n.sid, n.name, n.matrix.matrix4f, new Quaternion4f( 0f, 0f, 0f, 1f ) );
            boneList.add(newBone);
            boneNameMap.put(newBone.getSourceId(), newBone);
            int offset = i*16;
            Matrix4f mat = new Matrix4f(Arrays.copyOfRange(invBindMatSource.floatArray.floats, offset, offset+16));
            newBone.invBindMatrix2 = mat;
        }

        for(Bone bone : boneList) {
            for(XMLNode childNode : bone.node.childrenList) {

                Bone b = boneNameMap.get(childNode.sid);
                if(b == null) {
                    System.out.println("failed map get: " + childNode.name);
                    continue;
                }
                bone.addChild(boneNameMap.get(childNode.sid));
            }
        }

        toDDF(bones, boneList.get(0), 0xffff, boneIndexMap, boneIds);
        skeletonBuilder.addAllBones(bones);

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

    private static void toDDF(ArrayList<com.dynamo.rig.proto.Rig.Bone> builderList, Bone bone, int parentIndex, HashMap<String, Integer> boneIndexMap, ArrayList<String> boneIds) {
        com.dynamo.rig.proto.Rig.Bone.Builder b = com.dynamo.rig.proto.Rig.Bone.newBuilder();

        boneIds.add(bone.getSourceId());

        b.setParent(parentIndex);
        b.setId(MurmurHash.hash64(bone.getName()));

        Matrix4f matrix = bone.bindMatrix;

        // Translation
        float x = matrix.get(0, 3);
        float y = matrix.get(1, 3);
        float z = matrix.get(2, 3);

        Point3 position = MathUtil.vecmathToDDF(new Point3d(x, y, z));
        b.setPosition(position);

        // Rotation
        Quaternion4f q = new Quaternion4f();
        q.set(matrix);
        Quat rotation = MathUtil.vecmathToDDF(new Quat4d(q.getA(), q.getB(), q.getC(), q.getD()));
        b.setRotation(rotation);

        Vector4f s0 = new Vector4f();
        Vector4f s1 = new Vector4f();
        Vector4f s2 = new Vector4f();
        matrix.getColumn(0, s0);
        matrix.getColumn(1, s1);
        matrix.getColumn(2, s2);
        Tuple3f t = new Tuple3f(s0.length(), s1.length(), s2.length());

        Vector3 scale = MathUtil.vecmathToDDF(new Vector3d(t.getX(), t.getY(), t.getZ()));
        b.setScale(scale);

        b.setLength(0.0f);

        builderList.add(b.build());

        parentIndex = boneIndexMap.getOrDefault(bone.getSourceId(), 0xffff);

        System.out.println("bone[" + parentIndex + "].getSourceId(): " + bone.getSourceId());

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
                if( bi != -1 ) {
                    final int weightIndex = skin.vertexWeights.v.ints[ vIndex + j * 2 + 1 ];
                    bw = weightsSource.floatArray.floats[ weightIndex ];
                    bone_indices_list.add(bi);
                    bone_weights_list.add(bw);
                } else {
                    throw new LoaderException("Invalid bone index when loading vertex weights.");
                    // influences[ i ][ j ] = new Influence( bindShapeMatrix.matrix4f, weight );
//                    bone_indices_list.add(0);
//                    bone_weights_list.add(0f);
//                    bi = 0;
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
