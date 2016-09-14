package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map.Entry;

import javax.vecmath.Point3d;
import javax.xml.stream.XMLInputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.loaders.models.collada.stax.XMLAnimation;
import org.jagatoo.loaders.models.collada.stax.XMLCOLLADA;
import org.jagatoo.loaders.models.collada.stax.XMLGeometry;
import org.jagatoo.loaders.models.collada.stax.XMLInput;
import org.jagatoo.loaders.models.collada.stax.XMLLibraryAnimations;
import org.jagatoo.loaders.models.collada.stax.XMLLibraryGeometries;
import org.jagatoo.loaders.models.collada.stax.XMLMesh;
import org.jagatoo.loaders.models.collada.stax.XMLParam;
import org.jagatoo.loaders.models.collada.stax.XMLSampler;
import org.jagatoo.loaders.models.collada.stax.XMLSource;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.AnimationTrack;
import com.dynamo.rig.proto.Rig.Bone;
import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.rig.proto.Rig.RigAnimation;
import com.dynamo.rig.proto.Rig.RigScene;
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

    private static HashMap<String, XMLSource> getSourcesMap(XMLMesh mesh,
            List<XMLSource> sources) {
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
        animationSetBuilder.mergeFrom(loadAnimations(collada, 1.0f/24.0f));
        // TODO skeleton
        
        // Add root bone
        Bone.Builder rootBone = Bone.newBuilder();
        rootBone.setId(0);
        rootBone.setParent(0xffff);
        Point3 p = Point3.newBuilder().setX(0).setY(0).setZ(0).build();
        Quat q = Quat.newBuilder().setX(0).setY(0).setZ(0).setW(1).build();
        Vector3 s = Vector3.newBuilder().setX(0).setY(0).setZ(0).build();
        rootBone.setPosition(p);
        rootBone.setRotation(q);
        rootBone.setScale(s);
        rootBone.setLength(0);
        skeletonBuilder.addBones(rootBone.build());
        
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
    
    private static RigAnimation generateAnimation(String id, String rootBoneId, HashMap<String, HashMap<String, XMLAnimation>> boneToAnimations, float duration, float sampleRate) {
        RigAnimation.Builder animBuilder = RigAnimation.newBuilder();
        animBuilder.setId(MurmurHash.hash64(id));
        animBuilder.setDuration(duration);
        animBuilder.setSampleRate(sampleRate);
        System.out.println("boneToAnimations: " + boneToAnimations.get(rootBoneId).get("location.X"));
        
        // TODO loop over bones here instead
        AnimationTrack.Builder trackBuilder = AnimationTrack.newBuilder();
        trackBuilder.setBoneIndex(0); // TODO this should match the correct index for a bone (we set 0 now since we only have one bone, the root)
        // sampleTrack(track, posBuilder, new Point3d(0.0, 0.0, 0.0), duration, sampleRate, spf, true);
        
        // TODO rotations and scale
        
        animBuilder.addTracks(trackBuilder.build());
        return animBuilder.build();
    }
    
    public static AnimationSet loadAnimations(XMLCOLLADA collada, float sampleRate) {
        
        AnimationSet.Builder animationSetBuilder = AnimationSet.newBuilder();

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
            HashMap<String, HashMap<String, XMLAnimation>> boneToAnimations = new HashMap<String, HashMap<String, XMLAnimation>>();
            
            // Loop through all animations (where an animation is a keyframed animation on different properties)
            //XMLLibraryAnimations libraryAnimation = collada.libraryAnimations.get(i);
            XMLLibraryAnimations libraryAnimation = collada.libraryAnimations.get(0);
            Iterator<Entry<String, XMLAnimation>> it = libraryAnimation.animations.entrySet().iterator();
            while (it.hasNext()) {
                HashMap.Entry pair = (HashMap.Entry)it.next();
                
                String animationId = (String)pair.getKey();
                XMLAnimation animation = (XMLAnimation)pair.getValue();
                System.out.println("animationId: " + animationId);
                
                // Get animations for root node (ie model)
                String targetPath = animation.channels.get(0).target;
                String[] targetParts = targetPath.split("/");
                String boneTarget = targetParts[0];
                String propertyTarget = targetParts[1];
                
                if (!boneToAnimations.containsKey(boneTarget)) {
                    boneToAnimations.put(boneTarget, new HashMap<String, XMLAnimation>());
                }
                boneToAnimations.get(boneTarget).put(propertyTarget, animation);
                
                // TODO remove this check later when we support animated bones.
                if (targetParts[0].equals(geometryName)) {
                    
                    // Figure out the total duration of the animation.
                    HashMap<String, XMLSource> samplersLUT = getSamplersLUT(animation);
                    if (samplersLUT == null || samplersLUT.isEmpty()) {
                        System.out.println("Animation lacks samplers!");
                        return null;
                    }
                    
                    // Find time input
                    if (!samplersLUT.containsKey("INPUT")) {
                        System.out.println("Animation lacks INPUT sampler!");
                        return null;
                    }
                    XMLSource inputSampler = samplersLUT.get("INPUT");
                    if (inputSampler.techniqueCommon.accessor.params.get(0).name != XMLParam.Name.TIME) {
                        System.out.println("Animation input is not a time channel: '" + inputSampler.techniqueCommon.accessor.params.get(0).name + "'");
                        return null;
                    }
                    float animLength = inputSampler.floatArray.floats[inputSampler.floatArray.count-1];
                    maxAnimationLength = Math.max(maxAnimationLength, animLength);
                    
                } else {
                    System.out.println("Only support animations on geometry for now! " + geometryName);
                }
            
                assert(maxAnimationLength > 0.0f);
            }
            
            
            //System.out.println("------");
            animationSetBuilder.addAnimations(generateAnimation("default", geometryName, boneToAnimations, maxAnimationLength, sampleRate));
            
        }
        
        return animationSetBuilder.build();
    }

    public static Mesh loadMesh(XMLCOLLADA collada) throws IOException,
            XMLStreamException, LoaderException {

        if (collada.libraryGeometries.size() != 1) {
            throw new LoaderException("Only a single geometry is supported");
        }

        XMLGeometry geom = collada.libraryGeometries.get(0).geometries.values()
                .iterator().next();

        XMLMesh mesh = geom.mesh;

        List<XMLSource> sources = mesh.sources;
        HashMap<String, XMLSource> sourcesMap = getSourcesMap(mesh, sources);

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

        List<Float> position_list = new ArrayList<Float>();
        List<Float> normal_list = new ArrayList<Float>();
        List<Float> texcoord_list = new ArrayList<Float>();
        List<Integer> indices_list = new ArrayList<Integer>();

        float meter = collada.asset.unit.meter;
        for (int i = 0; i < mesh.triangles.count; ++i) {
            int idx = i * stride * 3 + vertex_input.offset;
            
            // TMP since rig needs indexed rendering for now...
            indices_list.add(i*3);
            indices_list.add(i*3+1);
            indices_list.add(i*3+2);

            for (int j = 0; j < 3; ++j) {
                int tri_ind = mesh.triangles.p[idx + stride * j];

                float px = positions.floatArray.floats[3 * tri_ind + 0];
                float py = positions.floatArray.floats[3 * tri_ind + 1];
                float pz = positions.floatArray.floats[3 * tri_ind + 2];

                position_list.add(px * meter);
                position_list.add(py * meter);
                position_list.add(pz * meter);
            }

            for (int j = 0; j < 3; ++j) {

                float px, py, pz;

                if (normals == null) {
                    px = py = 0;
                    pz = 1;
                } else {
                    idx = i * stride * 3 + normalOffset;
                    int tri_ind = mesh.triangles.p[idx + stride * j];
                    px = normals.floatArray.floats[3 * tri_ind + 0];
                    py = normals.floatArray.floats[3 * tri_ind + 1];
                    pz = normals.floatArray.floats[3 * tri_ind + 2];
                }

                normal_list.add(px);
                normal_list.add(py);
                normal_list.add(pz);
            }


            for (int j = 0; j < 3; ++j) {

                float u, v;

                if (normals == null || texcoords == null) {
                    u  = v  = 0;
                } else {
                    idx = i * stride * 3 + texcoord_input.offset;
                    int tri_ind = mesh.triangles.p[idx + stride * j];
                    u = texcoords.floatArray.floats[2 * tri_ind + 0];
                    v = texcoords.floatArray.floats[2 * tri_ind + 1];
                }

                texcoord_list.add(u);
                texcoord_list.add(v);
            }

        }

        Mesh.Builder b = Mesh.newBuilder();
        b.addAllPositions(position_list);
        b.addAllNormals(normal_list);
        b.addAllTexcoord0(texcoord_list);
        b.addAllIndices(indices_list);
        return b.build();
    }

}
