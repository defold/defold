package com.defold.editor.pipeline;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Vector;
import java.util.Map.Entry;

import org.apache.commons.io.FilenameUtils;
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
import org.openmali.FastMath;

import com.defold.editor.pipeline.MathUtil;
import com.defold.editor.pipeline.RigUtil;

import com.defold.editor.pipeline.MurmurHash;
import com.defold.editor.pipeline.RigUtil.AnimationKey;
import com.defold.editor.pipeline.RigUtil.Weight;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.proto.DdfMath.Vector3;

import com.dynamo.rig.proto.Rig;
import com.dynamo.rig.proto.Rig.AnimationInstanceDesc;
import com.dynamo.rig.proto.Rig.AnimationSetDesc;
import com.dynamo.rig.proto.Rig.MeshEntry;
import com.google.protobuf.TextFormat;

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

    public static boolean load(InputStream is, Rig.MeshSet.Builder meshSetBuilder, Rig.AnimationSet.Builder animationSetBuilder, Rig.Skeleton.Builder skeletonBuilder) throws IOException, XMLStreamException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        loadMesh(collada, meshSetBuilder);
        loadSkeleton(collada, skeletonBuilder, new ArrayList<String>());
        loadAnimations(collada, animationSetBuilder, 30.0f, "", new ArrayList<String>()); // TODO pick the sample rate from a parameter
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

    private static void ExtractMatrixKeys(Bone bone, Matrix4d parentToLocal, Matrix4d upAxisMatrix, XMLAnimation animation, RigUtil.AnimationTrack posTrack, RigUtil.AnimationTrack rotTrack, RigUtil.AnimationTrack scaleTrack) {

        Vector3d bindP = new Vector3d();
        Quat4d bindR = new Quat4d();
        Vector3d bindS = new Vector3d();
        MathUtil.decompose(parentToLocal, bindP, bindR, bindS);
        bindR.inverse();

        int keyCount = animation.getInput().length;
        float[] time = animation.getInput();
        float[] values = animation.getOutput();
        for (int key = 0; key < keyCount; ++key) {
            int index = key * 16;
            Matrix4d m = MathUtil.floatToDouble(new Matrix4f(ArrayUtils.subarray(values, index, index + 16)));
            if (upAxisMatrix != null) {
                m.mul(upAxisMatrix, m);
            }

            Vector3d p = new Vector3d();
            Quat4d r = new Quat4d();
            Vector3d s = new Vector3d();
            MathUtil.decompose(m, p, r, s);

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
    }

    private static void ExtractKeys(Bone bone, Matrix4d parentToLocal, Matrix4d upAxisMatrix, XMLAnimation animation, RigUtil.AnimationTrack posTrack, RigUtil.AnimationTrack rotTrack, RigUtil.AnimationTrack scaleTrack) throws LoaderException {
        switch (animation.getType()) {
        case TRANSLATE:
        case SCALE:
        case ROTATE:
            throw new LoaderException("Currently only collada files with matrix animations are supported.");
        case TRANSFORM:
        case MATRIX:
            ExtractMatrixKeys(bone, parentToLocal, upAxisMatrix, animation, posTrack, rotTrack, scaleTrack);
            break;
        default:
            throw new LoaderException(String.format("Animations of type %s are not supported.", animation.getType().name()));
        }
    }

    private static Matrix4d getBoneParentToLocal(Bone bone) {
        return MathUtil.floatToDouble(MathUtil.vecmath2ToVecmath1(bone.bindMatrix));
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

    private static void boneAnimToDDF(XMLCOLLADA collada, Rig.RigAnimation.Builder animBuilder, ArrayList<Bone> boneList, HashMap<Long, Integer> boneRefMap, HashMap<String, ArrayList<XMLAnimation>> boneToAnimations, float duration, float sampleRate) throws LoaderException {
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
            String boneId = bone.node.id;

            // Lookup the index for the bone in the reference map.
            // This is the bone index to use since we can't guarantee that that
            // the bone hierarchy is stored as depth first, we need to depend on the
            // supplied "JOINT" reference list from the skin entry.
            Long boneHash = MurmurHash.hash64(bone.getSourceId());
            Integer refIndex = boneRefMap.get(boneHash);

            if (boneToAnimations.containsKey(boneId) && refIndex != null)
            {
                Matrix4d parentToLocal = getBoneParentToLocal(bone);
                Matrix4d upAxisMatrix = null;
                if (bi == 0) {
                    upAxisMatrix = MathUtil.floatToDouble(createUpAxisMatrix(collada.asset.upAxis));
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

                    ExtractKeys(bone, parentToLocal, upAxisMatrix, animation, posTrack, rotTrack, scaleTrack);

                    samplePosTrack(animBuilder, refIndex, posTrack, duration, sampleRate, spf, true, false);
                    sampleRotTrack(animBuilder, refIndex, rotTrack, duration, sampleRate, spf, true, true);
                    sampleScaleTrack(animBuilder, refIndex, scaleTrack, duration, sampleRate, spf, true, false);
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

    public static void loadAnimations(InputStream is, Rig.AnimationSet.Builder animationSetBuilder, float sampleRate, String parentAnimationId, ArrayList<String> animationIds) throws IOException, XMLStreamException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        loadAnimations(collada, animationSetBuilder, sampleRate, parentAnimationId, animationIds);
    }

    public static void loadAnimations(XMLCOLLADA collada, Rig.AnimationSet.Builder animationSetBuilder, float sampleRate, String parentAnimationId, ArrayList<String> animationIds) throws IOException, XMLStreamException, LoaderException {
        if (collada.libraryAnimations.size() != 1) {
            return;
        }

        String[] boneRefArray = getBoneReferenceList(collada);
        if (boneRefArray == null) {
            return;
        }
        HashMap<Long, Integer> boneRefMap = new HashMap<Long, Integer>();
        for (int i = 0; i < boneRefArray.length; i++) {
            Long boneRef = MurmurHash.hash64(boneRefArray[i]);
            boneRefMap.put(boneRef, i);
            animationSetBuilder.addBoneList(boneRef);
        }

        // Load skeleton to get bone bind poses
        ArrayList<String> boneIds = new ArrayList<String>();
        ArrayList<Bone> boneList = loadSkeleton(collada, boneIds);

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
            boneAnimToDDF(collada, animBuilder, boneList, boneRefMap, boneToAnimations, totalAnimationLength, sampleRate);
            animBuilder.setId(MurmurHash.hash64(parentAnimationId));
            animationIds.add(parentAnimationId);
            animationSetBuilder.addAnimations(animBuilder.build());
        }
    }

    public static void loadMesh(InputStream is, Rig.MeshSet.Builder meshSetBuilder) throws IOException, XMLStreamException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        loadMesh(collada, meshSetBuilder);
    }

    public static Matrix4f createUpAxisMatrix(UpAxis upAxis) {
        Matrix4f axisMatrix = new Matrix4f();
        axisMatrix.setIdentity();
        if (upAxis.equals(UpAxis.Z_UP)) {
            axisMatrix.setRow(1, new float[]{0.0f, 0.0f, 1.0f, 0.0f});
            axisMatrix.setRow(2, new float[]{0.0f, -1.0f, 0.0f, 0.0f});
        } else if (upAxis.equals(UpAxis.X_UP)) {
            axisMatrix.setRow(0, new float[]{0.0f, -1.0f, 0.0f, 0.0f});
            axisMatrix.setRow(1, new float[]{1.0f, 0.0f, 0.0f, 0.0f});
        }
        return axisMatrix;
    }

    private static XMLNode getFirstNodeWithGeoemtry(Collection<XMLVisualScene> scenes) {
        for (XMLVisualScene scene : scenes) {
            for (XMLNode node : scene.nodes.values()) {
                if (node.instanceGeometries.size() > 0) {
                    return node;
                }
            }
        }

        return null;
    }

    public static void loadMesh(XMLCOLLADA collada, Rig.MeshSet.Builder meshSetBuilder) throws IOException, XMLStreamException, LoaderException {
        if (collada.libraryGeometries.size() != 1) {
            if (collada.libraryGeometries.isEmpty()) {
                return;
            }
            throw new LoaderException("Only a single geometry is supported");
        }

        // Use first geometry entry as default
        XMLGeometry geom = collada.libraryGeometries.get(0).geometries.values()
                .iterator().next();

        // Find first node in visual scene tree that has a instance geometry
        XMLNode sceneNode = null;
        Matrix4f sceneNodeMatrix = new Matrix4f();
        sceneNodeMatrix.setIdentity();
        if (collada.libraryVisualScenes.size() > 0) {
            sceneNode = getFirstNodeWithGeoemtry(collada.libraryVisualScenes.get(0).scenes.values());
            if (sceneNode != null) {
                XMLInstanceGeometry instanceGeo = sceneNode.instanceGeometries.get(0);
                String geometryId = instanceGeo.url;

                // Get node transform if available
                sceneNodeMatrix = MathUtil.vecmath2ToVecmath1(sceneNode.matrix.matrix4f);

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
        Matrix4f bindShapeMatrix = new Matrix4f();
        bindShapeMatrix.setIdentity();
        XMLSkin skin = null;
        if (!collada.libraryControllers.isEmpty()) {
            skin = findFirstSkin(collada.libraryControllers.get(0));
            if (skin != null && skin.bindShapeMatrix != null) {
                bindShapeMatrix.set(MathUtil.vecmath2ToVecmath1(skin.bindShapeMatrix.matrix4f));
            }
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
        Matrix4f axisMatrix = createUpAxisMatrix(collada.asset.upAxis);
        bindShapeMatrix.mul(axisMatrix, bindShapeMatrix);

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

        // Create a normal matrix which is the transposed inverse of
        // the bind shape matrix (which already include the up axis matrix).
        Matrix4f normalMatrix = new Matrix4f(bindShapeMatrix);
        normalMatrix.invert();
        normalMatrix.transpose();

        List<Float> normal_list = null;
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
        for (int i = 0; i < mesh.triangles.count; ++i) {

            for (int j = 0; j < 3; ++j) {
                int idx = i * stride * 3 + vertex_input.offset;
                int vert_idx = mesh.triangles.p[idx + stride * j];
                position_indices_list.add(vert_idx);

                if (normals != null) {
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
        int max_bone_count = loadVertexWeights(collada, bone_weights_list, bone_indices_list);

        Rig.Mesh.Builder meshBuilder = Rig.Mesh.newBuilder();
        if(normals != null) {
            meshBuilder.addAllNormals(normal_list);
            meshBuilder.addAllNormalsIndices(normal_indices_list);
        }
        meshBuilder.addAllPositions(position_list);
        meshBuilder.addAllTexcoord0(texcoord_list);
        meshBuilder.addAllIndices(position_indices_list);
        meshBuilder.addAllTexcoord0Indices(texcoord_indices_list);
        meshBuilder.addAllWeights(bone_weights_list);
        meshBuilder.addAllBoneIndices(bone_indices_list);

        // We currently only support one mesh per collada file
        MeshEntry.Builder meshEntryBuilder = MeshEntry.newBuilder();
        meshEntryBuilder.addMeshes(meshBuilder);
        meshEntryBuilder.setId(0);
        meshSetBuilder.addMeshEntries(meshEntryBuilder);
        meshSetBuilder.setMaxBoneCount(max_bone_count);

        String[] boneRefArray = getBoneReferenceList(collada);
        if (boneRefArray != null) {
            for (int i = 0; i < boneRefArray.length; i++) {
                meshSetBuilder.addBoneList(MurmurHash.hash64(boneRefArray[i]));
            }
        }
    }


    public static void loadSkeleton(InputStream is, com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder, ArrayList<String> boneIds) throws IOException, XMLStreamException, LoaderException {
        loadSkeleton(loadDAE(is), skeletonBuilder, boneIds);
    }

    private static Bone loadBone(XMLNode node, ArrayList<Bone> boneList, ArrayList<String> boneIds, Matrix4f upAxisMatrix) {

        Matrix4f boneBindMatrix = MathUtil.vecmath2ToVecmath1(node.matrix.matrix4f);
        if (upAxisMatrix != null) {
            boneBindMatrix.mul(upAxisMatrix, boneBindMatrix);
        }

        Bone newBone = new Bone(node, node.sid, node.name, MathUtil.vecmath1ToVecmath2(boneBindMatrix), new org.openmali.vecmath2.Quaternion4f(0f, 0f, 0f, 1f));

        boneList.add(newBone);
        boneIds.add(newBone.getSourceId());

        for(XMLNode childNode : node.childrenList) {
            if(childNode.type == XMLNode.Type.JOINT) {
                Bone childBone = loadBone(childNode, boneList, boneIds, null);
                newBone.addChild(childBone);
            }
        }

        return newBone;
    }

    private static String[] getBoneReferenceList(XMLCOLLADA collada) throws LoaderException {
        /*
         * Get the bone reference array.
         * This array specify the order the bone indices are defined (bone indices for vertex data are based on this order).
         */
        XMLSkin skin = null;
        if (!collada.libraryControllers.isEmpty()) {
            skin = findFirstSkin(collada.libraryControllers.get(0));
        }
        if(skin == null) {
            return null;
        }
        List<XMLSource> sources = skin.sources;
        HashMap<String, XMLSource> sourcesMap = getSourcesMap(sources);

        XMLInput jointInput = findInput(skin.jointsInputs, "JOINT", true);
        XMLSource jointSource = sourcesMap.get(jointInput.source);

        String boneRefArray[];
        if(jointSource.nameArray != null) {
            boneRefArray = jointSource.nameArray.names;
        } else if(jointSource.idrefArray != null) {
            boneRefArray = jointSource.idrefArray.idrefs;
        } else {
            return null;
        }

        return boneRefArray;
    }

    public static ArrayList<Bone> loadSkeleton(XMLCOLLADA collada, ArrayList<String> boneIds) throws IOException, XMLStreamException, LoaderException {
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

        // We don't use the supplied inverse bind matrices. This is calculated automatically in the engine, see res_rig_scene.cpp.
        // XMLInput invBindMatInput = findInput(skin.jointsInputs, "INV_BIND_MATRIX", true);
        // XMLSource invBindMatSource = sourcesMap.get(invBindMatInput.source);

        /*
         * Create the Bone hierarchy based on depth first recursion of the skeleton tree.
         */
        ArrayList<Bone> boneList = new ArrayList<Bone>();
        loadBone(rootNode, boneList, boneIds, createUpAxisMatrix(collada.asset.upAxis));
        return boneList;
    }

    public static void loadSkeleton(XMLCOLLADA collada, com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder, ArrayList<String> boneIds) throws IOException, XMLStreamException, LoaderException {
        ArrayList<Bone> boneList = loadSkeleton(collada, boneIds);

        // No bones found
        if (boneList == null) {
            return;
        }

        /*
         * Generate DDF representation of bones.
         */
        ArrayList<com.dynamo.rig.proto.Rig.Bone> ddfBones = new ArrayList<com.dynamo.rig.proto.Rig.Bone>();
        toDDF(ddfBones, boneList.get(0), 0xffff, createUpAxisMatrix(collada.asset.upAxis));
        skeletonBuilder.addAllBones(ddfBones);
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

    private static void toDDF(ArrayList<com.dynamo.rig.proto.Rig.Bone> ddfBones, Bone bone, int parentIndex, Matrix4f upAxisMatrix) {
        com.dynamo.rig.proto.Rig.Bone.Builder b = com.dynamo.rig.proto.Rig.Bone.newBuilder();

        b.setParent(parentIndex);
        b.setId(MurmurHash.hash64(bone.getSourceId()));

        Matrix4d matrix = MathUtil.floatToDouble(MathUtil.vecmath2ToVecmath1(bone.bindMatrix));

        /*
         * Decompose pos, rot and scale from bone matrix.
         */
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

        /*
         * Collada "bones" are just joints and don't have any length.
         */
        b.setLength(0.0f);
        b.setInheritScale(true);

        /*
         * Add bone to bone list and keep track of bone id (we use "sid"/"source id").
         */
        parentIndex = ddfBones.size();
        ddfBones.add(b.build());

        for(int i = 0; i < bone.numChildren(); i++) {
            Bone childBone = bone.getChild(i);
            toDDF(ddfBones, childBone, parentIndex, null);
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
        return maxBoneCount;
    }


}
