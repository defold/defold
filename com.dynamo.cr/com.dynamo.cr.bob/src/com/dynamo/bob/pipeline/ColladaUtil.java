package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map.Entry;

import org.jagatoo.loaders.models.collada.datastructs.animation.Bone;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Tuple3d;
import javax.vecmath.Vector3d;
import javax.xml.stream.XMLInputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.loaders.models.collada.stax.XMLAnimation;
import org.jagatoo.loaders.models.collada.stax.XMLAsset;
import org.jagatoo.loaders.models.collada.stax.XMLAnimationClip;
import org.jagatoo.loaders.models.collada.stax.XMLCOLLADA;
import org.jagatoo.loaders.models.collada.stax.XMLController;
import org.jagatoo.loaders.models.collada.stax.XMLGeometry;
import org.jagatoo.loaders.models.collada.stax.XMLInput;
import org.jagatoo.loaders.models.collada.stax.XMLLibraryAnimationClips;
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
        loadMesh(collada, meshBuilder);
        loadSkeleton(collada, skeletonBuilder, new ArrayList<String>());
        loadAnimations(collada, animationSetBuilder, skeletonBuilder.clone().build(), 30.0f, new ArrayList<String>()); // TODO pick the sample rate from a parameter
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

    private static float[] quatFromMatrix(XMLMatrix4x4 matrix) {
        double m00 = matrix.matrix4f.get(0, 0);
        double m10 = matrix.matrix4f.get(1, 0);
        double m20 = matrix.matrix4f.get(2, 0);

        double m01 = matrix.matrix4f.get(0, 1);
        double m11 = matrix.matrix4f.get(1, 1);
        double m21 = matrix.matrix4f.get(2, 1);

        double m02 = matrix.matrix4f.get(0, 2);
        double m12 = matrix.matrix4f.get(1, 2);
        double m22 = matrix.matrix4f.get(2, 2);

        double tr = m00 + m11 + m22;

        double qw = 0.0;
        double qx = 0.0;
        double qy = 0.0;
        double qz = 0.0;

        if (tr > 0) {
          double S = Math.sqrt(tr+1.0) * 2; // S=4*qw
          qw = 0.25 * S;
          qx = (m21 - m12) / S;
          qy = (m02 - m20) / S;
          qz = (m10 - m01) / S;
        } else if ((m00 > m11)&(m00 > m22)) {
          double S = Math.sqrt(1.0 + m00 - m11 - m22) * 2; // S=4*qx
          qw = (m21 - m12) / S;
          qx = 0.25 * S;
          qy = (m01 + m10) / S;
          qz = (m02 + m20) / S;
        } else if (m11 > m22) {
          double S = Math.sqrt(1.0 + m11 - m00 - m22) * 2; // S=4*qy
          qw = (m02 - m20) / S;
          qx = (m01 + m10) / S;
          qy = 0.25 * S;
          qz = (m12 + m21) / S;
        } else {
          double S = Math.sqrt(1.0 + m22 - m00 - m11) * 2; // S=4*qz
          qw = (m10 - m01) / S;
          qx = (m02 + m20) / S;
          qy = (m12 + m21) / S;
          qz = 0.25 * S;
        }

        return new float[]{(float)qx, (float)qy, (float)qz, (float)qw};
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
            AnimationKey rotationKey = new AnimationKey();
            AnimationKey scaleKey = new AnimationKey();
            positionKey.t = t[i];
            rotationKey.t = t[i];
            scaleKey.t = t[i];


            // Translation
            float x = matrix.matrix4f.get(0, 3);
            float y = matrix.matrix4f.get(1, 3);
            float z = matrix.matrix4f.get(2, 3);
            positionKey.value = new float[]{x, y, z};

            // Rotation
            rotationKey.value = quatFromMatrix(matrix);
//            System.out.println("rotation keyframe: " + rotationKey.value[0] + ", " + rotationKey.value[1] + ", " + rotationKey.value[2] + ", " + rotationKey.value[3]);

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

    private static float[] swizzleXYZ(XMLAsset.UpAxis upAxis, float[] input) {
        float[] output = new float[3];
        if (upAxis == UpAxis.Z_UP) {
            output[0] = input[0];
            output[1] = input[2];
            output[2] = -input[1];
        } else {
            output[0] = input[0];
            output[1] = input[1];
            output[2] = input[2];
        }
        return output;
    }

    private static void loadComponentTrack(XMLAnimation animation, AnimationTrack.Builder trackBuilder, RigUtil.AnimationTrack.Property componentProperty, double defaultValue, float duration, float sampleRate, double spf) {
        RigUtil.PositionComponentBuilder posXBuilder = new RigUtil.PositionComponentBuilder(trackBuilder);
        RigUtil.AnimationTrack track = new RigUtil.AnimationTrack();
        track.property = RigUtil.AnimationTrack.Property.POSITION_COMPONENT;
        loadComponentTrack(animation, track);
        RigUtil.sampleTrack(track, posXBuilder, defaultValue, duration, sampleRate, spf, true, false);
    }

    private static void toDDF(XMLCOLLADA collada, RigAnimation.Builder animBuilder, com.dynamo.rig.proto.Rig.Skeleton skeleton, String animationId, HashMap<Long, HashMap<String, XMLAnimation>> boneToAnimations, float duration, float sampleRate) {
        animBuilder.setId(MurmurHash.hash64(animationId));
        animBuilder.setDuration(duration);
        animBuilder.setSampleRate(sampleRate);

        double spf = 1.0 / sampleRate;
        float meter = collada.asset.unit.meter;

        // We loop through all bones in the skeleton, this also includes the "fake root" representing the model.
        for (int bi = 0; bi < skeleton.getBonesCount(); bi++) {
            com.dynamo.rig.proto.Rig.Bone bone = skeleton.getBones(bi);

            // Check if there exist any animation for the current bone
            if (boneToAnimations.containsKey(bone.getId())) {
                HashMap<String, XMLAnimation> propAnimations = boneToAnimations.get(bone.getId());

                AnimationTrack.Builder animTrackBuilder = AnimationTrack.newBuilder();
                animTrackBuilder.setBoneIndex(bi);
//                System.out.println("bone index: " + bi);

                if (propAnimations.containsKey("transform")) {
//                    System.out.println("has transform animation");
                    XMLAnimation anim = propAnimations.get("transform");

                    AnimationTrack.Builder animTrackPosition = AnimationTrack.newBuilder();
                    AnimationTrack.Builder animTrackRotation = AnimationTrack.newBuilder();
                    AnimationTrack.Builder animTrackScale = AnimationTrack.newBuilder();
                    RigUtil.PositionBuilder positionBuilder = new RigUtil.PositionBuilder(animTrackPosition);
                    RigUtil.QuatRotationBuilder rotationBuilder = new RigUtil.QuatRotationBuilder(animTrackRotation);
                    RigUtil.ScaleBuilder scaleBuilder = new RigUtil.ScaleBuilder(animTrackScale);

                    RigUtil.AnimationTrack trackPosition = new RigUtil.AnimationTrack();
                    trackPosition.property = RigUtil.AnimationTrack.Property.POSITION;

                    RigUtil.AnimationTrack trackRotation = new RigUtil.AnimationTrack();
                    trackRotation.property = RigUtil.AnimationTrack.Property.ROTATION;

                    RigUtil.AnimationTrack trackScale = new RigUtil.AnimationTrack();
                    trackScale.property = RigUtil.AnimationTrack.Property.SCALE;

                    loadTransformTrack(anim, trackPosition, trackRotation, trackScale);
                    RigUtil.sampleTrack(trackPosition, positionBuilder, new Point3d(0.0, 0.0, 0.0), duration, sampleRate, spf, true, false);
                    RigUtil.sampleTrack(trackRotation, rotationBuilder, new Quat4d(0.0, 0.0, 0.0, 1.0), duration, sampleRate, spf, true, true);
                    RigUtil.sampleTrack(trackScale, scaleBuilder, new Vector3d(1.0, 1.0, 1.0), duration, sampleRate, spf, true, false);

//                    System.out.println(bone.getPosition());

                    // TODO make this prettier
                    for (int i = 0; i < animTrackPosition.getPositionsCount() / 3; i++) {
//                        float[] xyz = swizzleXYZ(collada.asset.upAxis, new float[]{animTrackPosition.getPositions(i*3), animTrackPosition.getPositions(i*3+1), animTrackPosition.getPositions(i*3+2)});
                        float [] xyz = new float[]{animTrackPosition.getPositions(i*3), animTrackPosition.getPositions(i*3+1), animTrackPosition.getPositions(i*3+2)};
                        animTrackBuilder.addPositions((xyz[0]-bone.getPosition().getX())*meter);
                        animTrackBuilder.addPositions((xyz[1]-bone.getPosition().getY())*meter);
                        animTrackBuilder.addPositions((xyz[2]-bone.getPosition().getZ())*meter);
//                        animTrackBuilder.addPositions((xyz[0]-bone.getPosition().getX())*meter);
//                        animTrackBuilder.addPositions((xyz[1]-bone.getPosition().getY())*meter);
//                        animTrackBuilder.addPositions((xyz[2]-bone.getPosition().getZ())*meter);
                        Quaternion4f bind_quat = new Quaternion4f(animTrackRotation.getRotations(i*4), animTrackRotation.getRotations(i*4+1), animTrackRotation.getRotations(i*4+2), animTrackRotation.getRotations(i*4+3));
                        Quaternion4f key_quat = new Quaternion4f(bone.getRotation().getX(), bone.getRotation().getY(), bone.getRotation().getZ(), bone.getRotation().getW());
//                        animTrackBuilder.addPositions(0.0f);
//                        animTrackBuilder.addPositions(0.0f);
//                        animTrackBuilder.addPositions(0.0f);

                        bind_quat = bind_quat.invert();
                        key_quat = key_quat.mul(bind_quat);


//                        animTrackBuilder.addRotations(animTrackRotation.getRotations(i*4));
//                        animTrackBuilder.addRotations(animTrackRotation.getRotations(i*4+1));
//                        animTrackBuilder.addRotations(animTrackRotation.getRotations(i*4+2));
//                        animTrackBuilder.addRotations(animTrackRotation.getRotations(i*4+3));

                        animTrackBuilder.addRotations(key_quat.getA());
                        animTrackBuilder.addRotations(key_quat.getB());
                        animTrackBuilder.addRotations(key_quat.getC());
                        animTrackBuilder.addRotations(key_quat.getD());

                        animTrackBuilder.addScale(animTrackScale.getScale(i*3));
                        animTrackBuilder.addScale(animTrackScale.getScale(i*3+1));
                        animTrackBuilder.addScale(animTrackScale.getScale(i*3+2));
                    }

                } else {
//                    System.out.println("has component animation");

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
                            animTrackBuilder.addPositions(animTrackBuilderX.getPositions(i)*meter);
                            animTrackBuilder.addPositions(animTrackBuilderY.getPositions(i)*meter);
                            animTrackBuilder.addPositions(animTrackBuilderZ.getPositions(i)*meter);

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
                            euler.set(animTrackBuilderX.getPositions(i),
                                      animTrackBuilderY.getPositions(i),
                                      animTrackBuilderZ.getPositions(i));
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
                            animTrackBuilder.addScale(animTrackBuilderX.getPositions(i));
                            animTrackBuilder.addScale(animTrackBuilderY.getPositions(i));
                            animTrackBuilder.addScale(animTrackBuilderZ.getPositions(i));
                        }
                    }

                }

                animBuilder.addTracks(animTrackBuilder.build());
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
            if(clip.id == null)
            {
                throw new LoaderException("Animation clip lacks id.");
            }
            animationIds.add(clip.id);
        }
    }

    public static void loadAnimations(InputStream is, AnimationSet.Builder animationSetBuilder, com.dynamo.rig.proto.Rig.Skeleton skeleton, float sampleRate, ArrayList<String> animationIds) throws IOException, XMLStreamException, LoaderException {
        XMLCOLLADA collada = loadDAE(is);
        loadAnimations(collada, animationSetBuilder, skeleton, sampleRate, animationIds);
    }

    public static void loadAnimations(XMLCOLLADA collada, AnimationSet.Builder animationSetBuilder, com.dynamo.rig.proto.Rig.Skeleton skeleton, float sampleRate, ArrayList<String> animationIds) throws IOException, XMLStreamException, LoaderException {

        if (collada.libraryAnimations.size() != 1) {
            return;
        }

        // Animation clips
        ArrayList<XMLLibraryAnimationClips> animClips = collada.libraryAnimationClips;

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

                XMLAnimation animation = (XMLAnimation)pair.getValue();

                // Get animations for root node (ie model)
                String targetPath = animation.channels.get(0).target;
                String[] targetParts = targetPath.split("/");
                String boneTarget = targetParts[0];
                String propertyTarget = targetParts[1];

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

            // Temp "fake" loop adding same animation to all clips.
            // TODO: Code above should iterate over animclips and create each aninm from .start to .end time for each riganim
            if(!animClips.isEmpty()) {
                for (XMLAnimationClip clip : animClips.get(0).animationClips.values()) {
                    RigAnimation.Builder animBuilder = RigAnimation.newBuilder();
                    if(clip.id == null)
                    {
                        throw new LoaderException("Animation clip lacks id.");
                    }
                    toDDF(collada, animBuilder, skeleton, clip.id, boneToAnimations, maxAnimationLength, sampleRate);
                    animationSetBuilder.addAnimations(animBuilder.build());
                    animationIds.add(clip.id);
                    // System.out.println("name: " + clip.name);
                    // System.out.println("id: " + clip.id);
                    // System.out.println("start: " + clip.start);
                    // System.out.println("end: " + clip.end);
                    //System.out.println("animinst-name: " + clip.animations.get(0).url);
                }
            } else {
                // If no clips are provided, add a "Default" clip that is the whole animation as one clip
                RigAnimation.Builder animBuilder = RigAnimation.newBuilder();
                toDDF(collada, animBuilder, skeleton, "Default", boneToAnimations, maxAnimationLength, sampleRate);
                animationSetBuilder.addAnimations(animBuilder.build());
                animationIds.add("Default");
            }


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
//            float[] xyz = swizzleXYZ(collada.asset.upAxis, new float[]{positions.floatArray.floats[i*3], positions.floatArray.floats[i*3+1], positions.floatArray.floats[i*3+2]});
            float[] xyz = new float[]{positions.floatArray.floats[i*3], positions.floatArray.floats[i*3+1], positions.floatArray.floats[i*3+2]};
//            position_list.add(positions.floatArray.floats[i]*meter);
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


    public static void loadSkeleton(InputStream is, com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder, ArrayList<String> boneIds) throws IOException, XMLStreamException, LoaderException {
        loadSkeleton(loadDAE(is), skeletonBuilder, boneIds);
    }

    public static void loadSkeleton(XMLCOLLADA collada, com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder, ArrayList<String> boneIds) throws IOException, XMLStreamException, LoaderException {
        if (collada.libraryVisualScenes.size() != 1 || collada.libraryGeometries.isEmpty()) {
            return;
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

//            System.out.println(rootNode.matrix);

            Bone rootBone = new Bone( rootNode.id, rootNode.sid, rootNode.name, localToWorld, new Quaternion4f( 0f, 0f, 0f, 1f ) );

            // Translation
            float x = rootNode.matrix.matrix4f.get(0, 3);
            float y = rootNode.matrix.matrix4f.get(1, 3);
            float z = rootNode.matrix.matrix4f.get(2, 3);

            // Rotation
            float[] quat = quatFromMatrix(rootNode.matrix);

            Quaternion4f quat2 = new Quaternion4f(quat);
            rootBone.setBindRotation( quat2 );
            rootBone.setLength( 0.0f );

            Vector3f bonePos = new Vector3f(x, y, z);
            rootBone.setAbsoluteTranslation(bonePos);

            ArrayList<Bone> boneList = new ArrayList<Bone>();
            loadJoint( localToWorld, rootNode, upVector, Point3f.ZERO, quat2, Point3f.ZERO, rootBone, boneList );
            fakeRootBone.addChild(rootBone);
        }



        toDDF(bones, fakeRootBone, 0xffff, boneIds);

        skeletonBuilder.addAllBones(bones);
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

    private static void loadJoint( Matrix4f localToWorld, XMLNode node, Vector3f upVector, Point3f parentRoot, Quaternion4f parentQuat, Point3f parentTip, Bone bone, ArrayList<Bone> boneList )
    {
//        System.out.println("--\nadding bone: " + bone.toString());
        boneList.add( bone );
        Matrix4f colMatrix = Matrix4f.IDENTITY;
//        if ( node.childrenList == null || ( node.childrenList != null && node.childrenList.isEmpty() ) ) {
//            return;
//        }

        // Get the node tip
        if (node.childrenList != null && node.childrenList.size() > 0) {
            colMatrix = node.childrenList.get( 0 ).matrix.matrix4f;
        }

        XMLMatrix4x4 matrix = node.matrix;

        // Translation
        float x = matrix.matrix4f.get(0, 3);
        float y = matrix.matrix4f.get(1, 3);
        float z = matrix.matrix4f.get(2, 3);

        // Rotation
        float[] quat = quatFromMatrix(matrix);

        Quaternion4f quat2 = new Quaternion4f(quat);
        //quat2 = parentQuat.mul(quat2);
        bone.setBindRotation( quat2 );
        bone.setLength( 1.0f );

        Vector3f bonePos = new Vector3f(x, y, z);
        bone.setAbsoluteTranslation(bonePos);

//        System.out.println("bone pos: " + x + ", " + y + ", " + z);
//        System.out.println("bone rot: " + quat[0] + ", " + quat[1] + ", " + quat[2] + ", " + quat[3]);

        for ( XMLNode child : node.childrenList )
        {
            Bone newBone = new Bone( child.id, child.sid, child.name, child.matrix.matrix4f, new Quaternion4f( 0f, 0f, 0f, 1f ) );
            bone.addChild( newBone );
            loadJoint( localToWorld, child, upVector, parentTip, quat2, null, newBone, boneList );
        }

        /*
//        Matrix4f colMatrix = node.childrenList.get( 0 ).matrix.matrix4f;
        Point3f nodePos = new Point3f(colMatrix.get( 0, 3 ), colMatrix.get( 1, 3 ), colMatrix.get( 2, 3 ));
        Point3f nodeTip = new Point3f(
                parentTip.getX() + colMatrix.get( 0, 3 ),
                parentTip.getY() + colMatrix.get( 1, 3 ),
                parentTip.getZ() + colMatrix.get( 2, 3 )
        );
        System.out.println("nodePos: " + nodePos.x() + ", " + nodePos.y() + ", " + nodePos.z());
        System.out.println("parentTip: " + parentTip.x() + ", " + parentTip.y() + ", " + parentTip.z());
        System.out.println("nodeTip: " + nodeTip.x() + ", " + nodeTip.y() + ", " + nodeTip.z());

        // Transform all into world coordinates
        Point3f parentRootW = new Point3f( parentRoot );
        localToWorld.transform( parentRootW );
        Point3f parentTipW = new Point3f( parentTip );
        localToWorld.transform( parentTipW );
        Point3f nodeTipW = new Point3f( nodeTip );
        localToWorld.transform( nodeTipW );

        System.out.println("parentRootW: " + parentRootW.x() + ", " + parentRootW.y() + ", " + parentRootW.z());
        System.out.println("parentTipW: " + parentTipW.x() + ", " + parentTipW.y() + ", " + parentTipW.z());
        System.out.println("nodeTipW: " + nodeTipW.x() + ", " + nodeTipW.y() + ", " + nodeTipW.z());

        // Compute the vectors
        Vector3f nodeVecW = new Vector3f();
        nodeVecW.sub( nodeTipW, parentTipW );
        Vector3f parentVecW = new Vector3f();
        parentVecW.sub( parentTipW, parentRootW );
        float length = nodeVecW.length();
        parentVecW.normalize();
        nodeVecW.normalize();

        System.out.println("nodeVecW: " + nodeVecW.x() + ", " + nodeVecW.y() + ", " + nodeVecW.z());

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

        Vector3f bonePos = new Vector3f(parentTip.getX(), parentTip.getY(), parentTip.getZ());
        bone.setAbsoluteTranslation(bonePos);
//        bone.setAbsoluteTranslation(nodeVecW);

        for ( XMLNode child : node.childrenList )
        {
            Bone newBone = new Bone( child.id, child.sid, child.name, child.matrix.matrix4f, new Quaternion4f( 0f, 0f, 0f, 1f ) );
            bone.addChild( newBone );
            loadJoint( localToWorld, child, upVector, parentTip, nodeTipW, newBone, boneList );
        }
        */
    }

    private static void toDDF(ArrayList<com.dynamo.rig.proto.Rig.Bone> builderList, Bone bone, int parentIndex, ArrayList<String> boneIds) {

        int boneIndex = builderList.size();
        com.dynamo.rig.proto.Rig.Bone.Builder b = com.dynamo.rig.proto.Rig.Bone.newBuilder();

        boneIds.add(bone.getId());

        b.setParent(parentIndex);
        b.setId(MurmurHash.hash64(bone.getId()));

//        Matrix4f absTrans = bone.getAbsoluteTransformation();
//        float xyz[] = new float[]{absTrans.get(0, 3), absTrans.get(1, 3), absTrans.get(2, 3)};
//        Point3 position = MathUtil.vecmathToDDF(new Point3d(xyz[0], xyz[1], xyz[2]));
//        System.out.println("v: " + position.getX() + ", " + position.getY() + ", " + position.getZ() );
//        float xyz[] = swizzleXYZ(XMLAsset.UpAxis.Z_UP, new float[]{absTrans.get(0, 3), absTrans.get(1, 3), absTrans.get(2, 3)});

        Vector3f xyz = bone.getAbsoluteTranslation();
        Point3 position = MathUtil.vecmathToDDF(new Point3d(xyz.getX(), xyz.getY(), xyz.getZ()));

        b.setPosition(position);

        Quaternion4f q = bone.getBindRotation();
        Quat rotation = MathUtil.vecmathToDDF(new Quat4d(q.getA(), q.getB(), q.getC(), q.getD()));
//        System.out.println("rotation: " + rotation);
        b.setRotation(rotation);

        Tuple3f t = bone.getAbsoluteScaling();
        Vector3 scale = MathUtil.vecmathToDDF(new Vector3d(t.getX(), t.getY(), t.getZ()));
        b.setScale(scale);

        b.setLength(bone.getLength());

        builderList.add(b.build());

        for(int i = 0; i < bone.numChildren(); i++) {
            Bone childBone = bone.getChild(i);
            toDDF(builderList, childBone, boneIndex, boneIds);
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
                bi = skin.vertexWeights.v.ints[ vIndex + j * 2 + 0 ] + 1; // + 1 to compensate for the "fake root"
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
