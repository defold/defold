package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.StringTokenizer;
import java.util.Map.Entry;

import org.jagatoo.loaders.models.collada.Rotations;
import org.jagatoo.loaders.models.collada.datastructs.animation.Bone;

import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;
import javax.vecmath.Vector3d;
import javax.xml.stream.XMLInputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.loaders.models.collada.stax.XMLAnimation;
import org.jagatoo.loaders.models.collada.stax.XMLCOLLADA;
import org.jagatoo.loaders.models.collada.stax.XMLController;
import org.jagatoo.loaders.models.collada.stax.XMLGeometry;
import org.jagatoo.loaders.models.collada.stax.XMLInput;
import org.jagatoo.loaders.models.collada.stax.XMLLibraryAnimations;
import org.jagatoo.loaders.models.collada.stax.XMLLibraryControllers;
import org.jagatoo.loaders.models.collada.stax.XMLLibraryGeometries;
import org.jagatoo.loaders.models.collada.stax.XMLMatrix4x4;
import org.jagatoo.loaders.models.collada.stax.XMLMesh;
import org.jagatoo.loaders.models.collada.stax.XMLParam;
import org.jagatoo.loaders.models.collada.stax.XMLSampler;
import org.jagatoo.loaders.models.collada.stax.XMLNode;
import org.jagatoo.loaders.models.collada.stax.XMLSkin;
import org.jagatoo.loaders.models.collada.stax.XMLSource;
import org.jagatoo.loaders.models.collada.stax.XMLVisualScene;
import org.jagatoo.loaders.models.collada.stax.XMLAsset.UpAxis;
import org.openmali.FastMath;
import org.openmali.vecmath2.Matrix4f;
import org.openmali.vecmath2.Point3f;
import org.openmali.vecmath2.Quaternion4f;
import org.openmali.vecmath2.Tuple3f;
import org.openmali.vecmath2.Vector3f;


import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.RigUtil;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.RigUtil.AnimationCurve;
import com.dynamo.bob.util.RigUtil.AnimationCurve.CurveIntepolation;
import com.dynamo.bob.util.RigUtil.AnimationKey;
import com.dynamo.bob.util.RigUtil.AnimationTrack.Property;
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
        meshBuilder.mergeFrom(loadMesh(collada));
        Skeleton skeleton = loadSkeleton(collada);
        skeletonBuilder.mergeFrom(skeleton);
        animationSetBuilder.mergeFrom(loadAnimations(collada, skeleton, 30.0f)); // TODO pick the sample rate from a parameter
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

    private static void eulerToQuat(Tuple3d euler, Quat4d quat) {
        // Implementation based on:
        // http://ntrs.nasa.gov/archive/nasa/casi.ntrs.nasa.gov/19770024290.pdf
        // Rotation sequence: 231 (YZX)
        double t1 = euler.y * Math.PI / 180;
        double t2 = euler.z * Math.PI / 180;
        double t3 = euler.x * Math.PI / 180;

        double c1 = Math.cos(t1/2);
        double s1 = Math.sin(t1/2);
        double c2 = Math.cos(t2/2);
        double s2 = Math.sin(t2/2);
        double c3 = Math.cos(t3/2);
        double s3 = Math.sin(t3/2);
        double c1_c2 = c1*c2;
        double s2_s3 = s2*s3;

        quat.w = -s1*s2_s3 + c1_c2*c3;
        quat.x =  s1*s2*c3 + s3*c1_c2;
        quat.y =  s1*c2*c3 + s2_s3*c1;
        quat.z = -s1*s3*c2 + s2*c1*c3;

        quat.normalize();
    }

    private static void loadTransformTrack(XMLAnimation animation, RigUtil.AnimationTrack positionTrack, RigUtil.AnimationTrack rotationTrack, RigUtil.AnimationTrack scaleTrack) {
        float[] t = animation.getInput();
        float[] outputs = animation.getOutput();

        for (int i = 0; i < t.length; i++) {

            int m_i = i*4*4;

            XMLMatrix4x4 matrix = new XMLMatrix4x4();
            int sub_i = 0;
            for(int x = 0; x < 4; x++) {
                for(int y = 0; y < 4; y++) {
                    matrix.matrix4f.set(x, y, outputs[m_i + sub_i++]);
                }
            }

            AnimationCurve curve = new AnimationCurve();
            curve.interpolation = CurveIntepolation.LINEAR;

            AnimationKey positionKey = new AnimationKey();
            positionKey.t = t[i];
            positionKey.curve = curve;
            AnimationKey rotationKey = new AnimationKey();
            rotationKey.t = t[i];
            rotationKey.curve = curve;
            AnimationKey scaleKey = new AnimationKey();
            scaleKey.t = t[i];
            scaleKey.curve = curve;

            // get translation
            float x = matrix.matrix4f.get(0, 3);
            float y = matrix.matrix4f.get(1, 3);
            float z = matrix.matrix4f.get(2, 3);
            positionKey.value = new float[]{x, y, z};
//            positionKey.value = new float[]{0.0f, 0.0f, 1.0f};
//            System.out.println("x: " + x);
//            System.out.println("y: " + y);
//            System.out.println("z: " + z);

            // get euler rotation
            double r11 = matrix.matrix4f.get(0, 0);
            double r21 = matrix.matrix4f.get(1, 0);
            double r31 = matrix.matrix4f.get(2, 0);

            double r12 = matrix.matrix4f.get(0, 1);
            double r22 = matrix.matrix4f.get(1, 1);
            double r32 = matrix.matrix4f.get(2, 1);

            double r13 = matrix.matrix4f.get(0, 2);
            double r23 = matrix.matrix4f.get(1, 2);
            double r33 = matrix.matrix4f.get(2, 2);

            double eulerX = Math.atan2(r32, r33);
            double eulerY = Math.atan2(-r31, Math.sqrt(r32*r32 + r33*r33));
            double eulerZ = Math.atan2(r21,  r11);

            Quat4d quat = new Quat4d();
            Tuple3d euler = new Tuple3d() {
            };
            euler.set(eulerX*180.0, eulerY*180.0, eulerZ*180.0);
            eulerToQuat(euler, quat);
            rotationKey.value = new float[]{(float)quat.getX(), (float)quat.getY(), (float)quat.getZ(), (float)quat.getW()};
//          System.out.println("rotX: " + eulerX);
//          System.out.println("rotY: " + eulerY);
//          System.out.println("rotZ: " + eulerZ);

            // TODO get scale
            scaleKey.value = new float[]{1.0f, 1.0f, 1.0f};

            positionTrack.keys.add(positionKey);
            rotationTrack.keys.add(rotationKey);
            scaleTrack.keys.add(scaleKey);

        }
    }

    private static void loadComponentTrack(XMLAnimation animation, RigUtil.AnimationTrack track) {
        float[] t = animation.getInput();
        float[] outputs = animation.getOutput();

        boolean hasCurve = false;
        float[] inTangents = animation.getInTangents();
        float[] outTangents = animation.getOutTangents();
        String[] interpolations = null;
        if (inTangents != null && outTangents != null) {
            hasCurve = true;
            interpolations = animation.getInterpolations();
        }

        for (int i = 0; i < t.length; i++) {
            AnimationKey key = new AnimationKey();
            key.t = t[i];
            switch (track.property) {
            case POSITION_COMPONENT:
                key.value = new float[] {outputs[i]};
                break;
            case ROTATION_COMPONENT:
              key.value = new float[] {outputs[i]};
                break;
            default:
                System.out.println("loadTrack for " + track.property.toString() + " not implemented yet!");
                break;
            }
            if (hasCurve) {
                AnimationCurve curve = new AnimationCurve();
                if (interpolations.length > i && interpolations[i] != null && interpolations[i].equals("BEZIER")) {
                    curve.interpolation = CurveIntepolation.BEZIER;
                } else {
                    curve.interpolation = CurveIntepolation.LINEAR;
                }
                // TEMP: Force linear curve interpolation!
                curve.interpolation = CurveIntepolation.LINEAR;
                curve.x0 = (float)inTangents[i*2];
                curve.y0 = (float)inTangents[i*2+1];
                curve.x1 = (float)outTangents[i*2];
                curve.y1 = (float)outTangents[i*2+1];
                key.curve = curve;
            } else {
                key.stepped = true;
            }
            track.keys.add(key);
        }
    }

    private static void loadComponentTrack(XMLAnimation animation, AnimationTrack.Builder trackBuilder, RigUtil.AnimationTrack.Property componentProperty, double defaultValue, float duration, float sampleRate, double spf) {
        RigUtil.PositionComponentBuilder posXBuilder = new RigUtil.PositionComponentBuilder(trackBuilder);
        RigUtil.AnimationTrack track = new RigUtil.AnimationTrack();
        track.property = RigUtil.AnimationTrack.Property.POSITION_COMPONENT;
        loadComponentTrack(animation, track);
        RigUtil.sampleTrack(track, posXBuilder, defaultValue, duration, sampleRate, spf, true);
    }

    private static void toDDF(RigAnimation.Builder animBuilder, com.dynamo.rig.proto.Rig.Skeleton skeleton, String animationId, long meshBoneId, HashMap<Long, HashMap<String, XMLAnimation>> boneToAnimations, float duration, float sampleRate) {
        animBuilder.setId(MurmurHash.hash64(animationId));
        animBuilder.setDuration(duration);
        animBuilder.setSampleRate(sampleRate);

        double spf = 1.0 / sampleRate;

        // We loop through all bones in the skeleton, this also includes the "fake root" representing the model.
        for (int bi = 0; bi < skeleton.getBonesCount(); bi++) {
            com.dynamo.rig.proto.Rig.Bone bone = skeleton.getBones(bi);

            System.out.println("bone index " + bi + ", hash:" + bone.getId());
            // Check if there exist any animation for the current bone
            if (boneToAnimations.containsKey(bone.getId())) {
                System.out.println("has animation tracks");
                HashMap<String, XMLAnimation> propAnimations = boneToAnimations.get(bone.getId());

                AnimationTrack.Builder animTrackBuilder = AnimationTrack.newBuilder();
                animTrackBuilder.setBoneIndex(bi);

                if (propAnimations.containsKey("transform")) {
                    System.out.println("has transformation track");
                    XMLAnimation anim = propAnimations.get("transform");

                    AnimationTrack.Builder animTrackPosition = AnimationTrack.newBuilder();
                    AnimationTrack.Builder animTrackRotation = AnimationTrack.newBuilder();
                    AnimationTrack.Builder animTrackScale = AnimationTrack.newBuilder();
                    RigUtil.PositionBuilder positionBuilder = new RigUtil.PositionBuilder(animTrackPosition);
                    RigUtil.RotationBuilder rotationBuilder = new RigUtil.RotationBuilder(animTrackRotation);
                    RigUtil.ScaleBuilder scaleBuilder = new RigUtil.ScaleBuilder(animTrackScale);

                    RigUtil.AnimationTrack trackPosition = new RigUtil.AnimationTrack();
                    trackPosition.property = RigUtil.AnimationTrack.Property.POSITION;

                    RigUtil.AnimationTrack trackRotation = new RigUtil.AnimationTrack();
                    trackRotation.property = RigUtil.AnimationTrack.Property.ROTATION;

                    RigUtil.AnimationTrack trackScale = new RigUtil.AnimationTrack();
                    trackScale.property = RigUtil.AnimationTrack.Property.SCALE;

                    loadTransformTrack(anim, trackPosition, trackRotation, trackScale);
                    RigUtil.sampleTrack(trackPosition, positionBuilder, new Point3d(0.0, 0.0, 0.0), duration, sampleRate, spf, true);
                    RigUtil.sampleTrack(trackRotation, rotationBuilder, new Quat4d(0.0, 0.0, 0.0, 1.0), duration, sampleRate, spf, true);
                    RigUtil.sampleTrack(trackScale, scaleBuilder, new Vector3d(1.0, 1.0, 1.0), duration, sampleRate, spf, true);

                    // TODO do this better...
                    for (int i = 0; i < animTrackPosition.getPositionsCount(); i++) {
                        animTrackBuilder.addPositions(animTrackPosition.getPositions(i));
                    }
                    for (int i = 0; i < animTrackPosition.getRotationsCount(); i++) {
                        animTrackBuilder.addRotations(animTrackRotation.getRotations(i));
                    }
                    for (int i = 0; i < animTrackPosition.getScaleCount(); i++) {
                        animTrackBuilder.addScale(animTrackScale.getScale(i));
                    }

                } else {
                    System.out.println("has split composite tracks");

                    // Animation tracks in collada are split on different components of properties,
                    // i.e. X, Y and Z components of the position/location property.
                    //
                    // We load each of these tracks (for each component), then sample them individually,
                    // finally we assemble the final track by picking samples from each component-track.

                    // Positions
                    if (propAnimations.containsKey("location.X") || propAnimations.containsKey("location.Y") || propAnimations.containsKey("location.Z")) {

                        AnimationTrack.Builder animTrackBuilderX = AnimationTrack.newBuilder();
                        AnimationTrack.Builder animTrackBuilderY = AnimationTrack.newBuilder();
                        AnimationTrack.Builder animTrackBuilderZ = AnimationTrack.newBuilder();

                        loadComponentTrack(propAnimations.get("location.X"), animTrackBuilderX, Property.POSITION_COMPONENT, 0.0, duration, sampleRate, spf);
                        loadComponentTrack(propAnimations.get("location.Y"), animTrackBuilderY, Property.POSITION_COMPONENT, 0.0, duration, sampleRate, spf);
                        loadComponentTrack(propAnimations.get("location.Z"), animTrackBuilderZ, Property.POSITION_COMPONENT, 0.0, duration, sampleRate, spf);

                        assert(animTrackBuilderX.getPositionsCount() == animTrackBuilderY.getPositionsCount());
                        assert(animTrackBuilderY.getPositionsCount() == animTrackBuilderZ.getPositionsCount());

                        for (int i = 0; i < animTrackBuilderX.getPositionsCount(); i++) {
                            animTrackBuilder.addPositions(animTrackBuilderY.getPositions(i));
                            animTrackBuilder.addPositions(animTrackBuilderZ.getPositions(i));
                            animTrackBuilder.addPositions(animTrackBuilderX.getPositions(i));

                        }
                    }

                    // Euler rotations
                    if (propAnimations.containsKey("rotationX.ANGLE") || propAnimations.containsKey("rotationY.ANGLE") || propAnimations.containsKey("rotationZ.ANGLE")) {

                        AnimationTrack.Builder animTrackBuilderX = AnimationTrack.newBuilder();
                        AnimationTrack.Builder animTrackBuilderY = AnimationTrack.newBuilder();
                        AnimationTrack.Builder animTrackBuilderZ = AnimationTrack.newBuilder();

                        loadComponentTrack(propAnimations.get("rotationX.ANGLE"), animTrackBuilderX, Property.ROTATION_COMPONENT, 0.0, duration, sampleRate, spf);
                        loadComponentTrack(propAnimations.get("rotationY.ANGLE"), animTrackBuilderY, Property.ROTATION_COMPONENT, 0.0, duration, sampleRate, spf);
                        loadComponentTrack(propAnimations.get("rotationZ.ANGLE"), animTrackBuilderZ, Property.ROTATION_COMPONENT, 0.0, duration, sampleRate, spf);

                        assert(animTrackBuilderX.getPositionsCount() == animTrackBuilderY.getPositionsCount());
                        assert(animTrackBuilderY.getPositionsCount() == animTrackBuilderZ.getPositionsCount());

                        for (int i = 0; i < animTrackBuilderX.getPositionsCount(); i++) {
                            Quat4d quat = new Quat4d();
                            Tuple3d euler = new Tuple3d() {
                            };
                            euler.set(animTrackBuilderY.getPositions(i),
                                      animTrackBuilderZ.getPositions(i),
                                      animTrackBuilderX.getPositions(i));
                            eulerToQuat(euler, quat);

                            animTrackBuilder.addRotations((float)quat.getX());
                            animTrackBuilder.addRotations((float)quat.getY());
                            animTrackBuilder.addRotations((float)quat.getZ());
                            animTrackBuilder.addRotations((float)quat.getW());
                        }
                    }

                    // Scale
                    if (propAnimations.containsKey("scale.X") || propAnimations.containsKey("scale.Y") || propAnimations.containsKey("scale.Z")) {

                        AnimationTrack.Builder animTrackBuilderX = AnimationTrack.newBuilder();
                        AnimationTrack.Builder animTrackBuilderY = AnimationTrack.newBuilder();
                        AnimationTrack.Builder animTrackBuilderZ = AnimationTrack.newBuilder();

                        loadComponentTrack(propAnimations.get("scale.X"), animTrackBuilderX, Property.POSITION_COMPONENT, 1.0, duration, sampleRate, spf);
                        loadComponentTrack(propAnimations.get("scale.Y"), animTrackBuilderY, Property.POSITION_COMPONENT, 1.0, duration, sampleRate, spf);
                        loadComponentTrack(propAnimations.get("scale.Z"), animTrackBuilderZ, Property.POSITION_COMPONENT, 1.0, duration, sampleRate, spf);

                        assert(animTrackBuilderX.getPositionsCount() == animTrackBuilderY.getPositionsCount());
                        assert(animTrackBuilderY.getPositionsCount() == animTrackBuilderZ.getPositionsCount());

                        for (int i = 0; i < animTrackBuilderX.getPositionsCount(); i++) {
                            animTrackBuilder.addScale(animTrackBuilderY.getPositions(i));
                            animTrackBuilder.addScale(animTrackBuilderZ.getPositions(i));
                            animTrackBuilder.addScale(animTrackBuilderX.getPositions(i));
                        }
                    }

                }

                animBuilder.addTracks(animTrackBuilder.build());
            }
        }
    }

    public static AnimationSet loadAnimations(XMLCOLLADA collada, com.dynamo.rig.proto.Rig.Skeleton skeleton, float sampleRate) throws IOException, XMLStreamException, LoaderException {

        AnimationSet.Builder animationSetBuilder = AnimationSet.newBuilder();
        if (collada.libraryAnimations.size() != 1 || collada.libraryGeometries.isEmpty()) {
            return animationSetBuilder.build();
        }

        // We only support one model per scene for now, get first geo entry.
        ArrayList<XMLLibraryGeometries> geometries = collada.libraryGeometries;
        String geometryName = "";
        for (XMLGeometry geometry : geometries.get(0).geometries.values()) {
            geometryName = geometry.name;
            break;
        }

        float maxAnimationLength = 0.0f;
        //for (int i = 0; i < collada.libraryAnimations.size(); i++) {
        {
            HashMap<Long, HashMap<String, XMLAnimation>> boneToAnimations = new HashMap<Long, HashMap<String, XMLAnimation>>();

            // Loop through all animations (where an animation is keyframed animation on different properties)
            //XMLLibraryAnimations libraryAnimation = collada.libraryAnimations.get(i);
            XMLLibraryAnimations libraryAnimation = collada.libraryAnimations.get(0);
            Iterator<Entry<String, XMLAnimation>> it = libraryAnimation.animations.entrySet().iterator();
            while (it.hasNext()) {
                HashMap.Entry pair = (HashMap.Entry)it.next();

                String animationId = (String)pair.getKey();
                XMLAnimation animation = (XMLAnimation)pair.getValue();

                // Get animations for root node (ie model)
                String targetPath = animation.channels.get(0).target;
                String[] targetParts = targetPath.split("/");
                String boneTarget = targetParts[0];
                String propertyTarget = targetParts[1];

                System.out.println("Animation target available: " + boneTarget);

                if (!boneToAnimations.containsKey(MurmurHash.hash64(boneTarget))) {
                    boneToAnimations.put(MurmurHash.hash64(boneTarget), new HashMap<String, XMLAnimation>());
                }
                boneToAnimations.get(MurmurHash.hash64(boneTarget)).put(propertyTarget, animation);

                // Figure out the total duration of the animation.
                HashMap<String, XMLSource> samplersLUT = getSamplersLUT(animation);
                if (samplersLUT == null || samplersLUT.isEmpty()) {
                    throw new LoaderException("Animation lacks samplers.");
                }

                // Find time input
                if (!samplersLUT.containsKey("INPUT")) {
                    throw new LoaderException("Animation lacks INPUT sampler.");
                }
                XMLSource inputSampler = samplersLUT.get("INPUT");
                if (inputSampler.techniqueCommon.accessor.params.get(0).name != XMLParam.Name.TIME) {
                    throw new LoaderException("Animation input is not a time channel: '" + inputSampler.techniqueCommon.accessor.params.get(0).name + "'.");
                }
                float animLength = inputSampler.floatArray.floats[inputSampler.floatArray.count-1];
                maxAnimationLength = Math.max(maxAnimationLength, animLength);

                assert(maxAnimationLength > 0.0f);
            }

            RigAnimation.Builder animBuilder = RigAnimation.newBuilder();
            toDDF(animBuilder, skeleton, "default", MurmurHash.hash64(geometryName), boneToAnimations, maxAnimationLength, sampleRate);
            animationSetBuilder.addAnimations(animBuilder.build());

        }

        return animationSetBuilder.build();
    }

    public static Mesh loadMesh(XMLCOLLADA collada) throws IOException,
            XMLStreamException, LoaderException {

        Mesh.Builder meshBuilder = Mesh.newBuilder();

        if (collada.libraryGeometries.size() != 1) {
            if (collada.libraryGeometries.isEmpty()) {
                return meshBuilder.build();
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
        for (int i = 0; i < positions.floatArray.count; ++i) {
            position_list.add(positions.floatArray.floats[i]*meter);
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

        return meshBuilder.build();
    }

    public static com.dynamo.rig.proto.Rig.Skeleton loadSkeleton(XMLCOLLADA collada) throws IOException, XMLStreamException, LoaderException {
        com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder = com.dynamo.rig.proto.Rig.Skeleton.newBuilder();

        if (collada.libraryVisualScenes.size() != 1 || collada.libraryGeometries.isEmpty()) {
            return skeletonBuilder.build();
        }

        // We only support one model per scene for now, get first geo entry.
        ArrayList<XMLLibraryGeometries> geometries = collada.libraryGeometries;
        String geometryName = "";
        for (XMLGeometry geometry : geometries.get(0).geometries.values()) {
            geometryName = geometry.name;
            break;
        }

        // Add a fake root bone that mesh animation is performed upon
        ArrayList<com.dynamo.rig.proto.Rig.Bone> bones = new ArrayList<com.dynamo.rig.proto.Rig.Bone>();
        Bone fakeRootBone = new Bone(geometryName, geometryName, geometryName, Matrix4f.IDENTITY, Quaternion4f.IDENTITY);

        XMLNode rootNode = null;
        for ( XMLVisualScene scene : collada.libraryVisualScenes.get(0).scenes.values() ) {
            for ( XMLNode node : scene.nodes.values() ) {
                rootNode = findFirstSkeleton(node);
                if(rootNode != null) {
                    break;
                }
            }
        }

        if(rootNode != null) {
            Vector3f upVector;
            if(collada.asset.upAxis.equals(UpAxis.Y_UP)) {
                upVector = new Vector3f(0.0f, 1.0f, 0.0f);
            } else {
                upVector = new Vector3f(0.0f, 0.0f, 1.0f);
            }
            Point3f skeletonPos;
            Matrix4f localToWorld;
            localToWorld = rootNode.matrix.matrix4f;
            skeletonPos = new Point3f( localToWorld.m03(), localToWorld.m13(), localToWorld.m23() );

            Bone rootBone = new Bone( rootNode.id, rootNode.sid, rootNode.name, localToWorld, new Quaternion4f( 0f, 0f, 0f, 1f ) );
            ArrayList<Bone> boneList = new ArrayList<Bone>();
            loadJoint( localToWorld, rootNode, upVector, Point3f.ZERO, Point3f.ZERO, rootBone, boneList );

            fakeRootBone.addChild(rootBone);
        }

        toDDF(bones, fakeRootBone, 0xffff);

        skeletonBuilder.addAllBones(bones);
        return skeletonBuilder.build();
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

    private static void loadJoint( Matrix4f localToWorld, XMLNode node, Vector3f upVector, Point3f parentRoot, Point3f parentTip, Bone bone, ArrayList<Bone> boneList )
    {
        boneList.add( bone );
        if ( node.childrenList == null || ( node.childrenList != null && node.childrenList.isEmpty() ) ) {
            return;
        }

        // Get the node tip
        Matrix4f colMatrix = node.childrenList.get( 0 ).matrix.matrix4f;
        Point3f nodeTip = new Point3f(
                parentTip.getX() + colMatrix.get( 0, 3 ),
                parentTip.getY() + colMatrix.get( 1, 3 ),
                parentTip.getZ() + colMatrix.get( 2, 3 )
        );

        // Transform all into world coordinates
        Point3f parentRootW = new Point3f( parentRoot );
        localToWorld.transform( parentRootW );
        Point3f parentTipW = new Point3f( parentTip );
        localToWorld.transform( parentTipW );
        Point3f nodeTipW = new Point3f( nodeTip );
        localToWorld.transform( nodeTipW );

        // Compute the vectors
        Vector3f nodeVecW = new Vector3f();
        nodeVecW.sub( nodeTipW, parentTipW );
        Vector3f parentVecW = new Vector3f();
        parentVecW.sub( parentTipW, parentRootW );
        float length = nodeVecW.length();
        parentVecW.normalize();
        nodeVecW.normalize();

        // Compute the angle
        float angle = FastMath.acos( parentVecW.dot( nodeVecW ) );
        if ( Float.isNaN( angle ) )
        {
            // Singularity : if a vector is the 0-vector, the angle will be NaN so set it to 0.
            angle = 0f;
        }
        Vector3f axis = new Vector3f();
        axis.cross( parentVecW, nodeVecW );
        if ( Float.isNaN( axis.getX() ) | Float.isNaN( axis.getY() ) | Float.isNaN( axis.getZ() ) )
        {
            // Singularity : Angle = 0. The axis found is (NaN, NaN, NaN)
            // In this case we reset it to UP.
            axis.set( upVector );
        }
        if ( axis.lengthSquared() == 0f )
        {
            // Singularity : Angle = 180, there is no single axis. In this case, we take an axis which is perpendicular
            // to one of the two vectors. This avoid NaNs.
            axis.set( parentVecW.getZ(), parentVecW.getX(), parentVecW.getY() );
            axis.normalize();
            axis.negate();
        }
        else
        {
            // For quaternion conversion
            axis.normalize();
            axis.negate();
        }
        Quaternion4f quat = Rotations.toQuaternion( axis, angle );
        bone.setBindRotation( quat );
        bone.setLength( length );

        for ( XMLNode child : node.childrenList )
        {
            Bone newBone = new Bone( child.id, child.sid, child.name, child.matrix.matrix4f, new Quaternion4f( 0f, 0f, 0f, 1f ) );
            bone.addChild( newBone );
            loadJoint( localToWorld, child, upVector, parentTip, nodeTip, newBone, boneList );
        }
    }

    private static void toDDF(ArrayList<com.dynamo.rig.proto.Rig.Bone> builderList, Bone bone, int parentIndex) {

        int boneIndex = builderList.size();
        com.dynamo.rig.proto.Rig.Bone.Builder b = com.dynamo.rig.proto.Rig.Bone.newBuilder();

        System.out.println("bone.getSourceId(): " + bone.getId());
        System.out.println("            hashed: " + MurmurHash.hash64(bone.getId()));


        b.setParent(parentIndex);
        b.setId(MurmurHash.hash64(bone.getId()));

        Vector3f v = bone.getAbsoluteTranslation();
        Point3 position = MathUtil.vecmathToDDF(new Point3d(v.getX(), v.getY(), v.getZ()));
        b.setPosition(position);

        Quaternion4f q = bone.getBindRotation();
        Quat rotation = MathUtil.vecmathToDDF(new Quat4d(q.getA(), q.getB(), q.getC(), q.getD()));
        b.setRotation(rotation);

        Tuple3f t = bone.getAbsoluteScaling();
        Vector3 scale = MathUtil.vecmathToDDF(new Vector3d(t.getX(), t.getY(), t.getZ()));
        b.setScale(scale);

        b.setLength(bone.getLength());

        builderList.add(b.build());

        for(int i = 0; i < bone.numChildren(); i++) {
            Bone childBone = bone.getChild(i);
            toDDF(builderList, childBone, boneIndex);
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

        if (collada.libraryControllers.isEmpty()) {
            return;
        }
        XMLSkin skin = findFirstSkin(collada.libraryControllers.get(0));
        if(skin == null) {

            // DAE does not have any weights, so we need to add dummy entries so all vertices use the fake root bone.
            for ( int i = 0; i < position_indices_list.size(); i++ ) {
                bone_indices_list.add(0);
                bone_indices_list.add(0);
                bone_indices_list.add(0);
                bone_indices_list.add(0);
                bone_weights_list.add(1f);
                bone_weights_list.add(0f);
                bone_weights_list.add(0f);
                bone_weights_list.add(0f);
            }

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
                    // influences[ i ][ j ] = new Influence( bindShapeMatrix.matrix4f, weight );
                    bone_indices_list.add(0);
                    bone_weights_list.add(0f);
                    bi = 0;
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
