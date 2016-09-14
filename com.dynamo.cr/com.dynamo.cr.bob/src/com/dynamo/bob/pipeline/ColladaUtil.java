package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;


import org.jagatoo.loaders.models.collada.Rotations;
import org.jagatoo.loaders.models.collada.datastructs.animation.Bone;

import javax.vecmath.Point3d;
import javax.vecmath.Vector3d;
import javax.vecmath.Quat4d;
import javax.xml.stream.XMLInputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.loaders.models.collada.stax.XMLCOLLADA;
import org.jagatoo.loaders.models.collada.stax.XMLGeometry;
import org.jagatoo.loaders.models.collada.stax.XMLInput;
import org.jagatoo.loaders.models.collada.stax.XMLMesh;
import org.jagatoo.loaders.models.collada.stax.XMLNode;
import org.jagatoo.loaders.models.collada.stax.XMLSource;
import org.jagatoo.loaders.models.collada.stax.XMLVisualScene;
import org.openmali.FastMath;
import org.openmali.vecmath2.Matrix4f;
import org.openmali.vecmath2.Point3f;
import org.openmali.vecmath2.Quaternion4f;
import org.openmali.vecmath2.Tuple3f;
import org.openmali.vecmath2.Vector3f;


import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.rig.proto.Rig.Mesh;


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

    public static Mesh loadMesh(InputStream is) throws IOException,
            XMLStreamException, LoaderException {
        XMLInputFactory factory = XMLInputFactory.newInstance();
        XMLStreamReader stream_reader = factory.createXMLStreamReader(is);

        XMLCOLLADA collada = new XMLCOLLADA();
        collada.parse(stream_reader);


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


    public static com.dynamo.rig.proto.Rig.Skeleton loadSkeleton(InputStream is) throws IOException, XMLStreamException, LoaderException {
        XMLInputFactory factory = XMLInputFactory.newInstance();
        XMLStreamReader stream_reader = factory.createXMLStreamReader(is);

        com.dynamo.rig.proto.Rig.Skeleton.Builder skeletonBuilder = com.dynamo.rig.proto.Rig.Skeleton.newBuilder();
        XMLCOLLADA collada = new XMLCOLLADA();
        collada.parse(stream_reader);

        if (collada.libraryVisualScenes.size() != 1) {
            return skeletonBuilder.build();
        }

        XMLNode rootNode = null;
        for ( XMLVisualScene scene : collada.libraryVisualScenes.get(0).scenes.values() ) {
            for ( XMLNode node : scene.nodes.values() ) {
                rootNode = findSkeleton(node);
                if(rootNode != null) {
                    break;
                }
            }
        }
        if(rootNode != null) {
            Vector3f upVector = new Vector3f(0.0f, 1.0f, 0.0f);
            Point3f skeletonPos;
            Matrix4f localToWorld;
            localToWorld = rootNode.matrix.matrix4f;
            skeletonPos = new Point3f( localToWorld.m03(), localToWorld.m13(), localToWorld.m23() );

            Bone rootBone = new Bone( rootNode.sid, rootNode.name, localToWorld, new Quaternion4f( 0f, 0f, 0f, 1f ) );
            ArrayList<Bone> boneList = new ArrayList<Bone>();
            loadJoint( localToWorld, rootNode, upVector, Point3f.ZERO, Point3f.ZERO, rootBone, boneList );

            ArrayList<com.dynamo.rig.proto.Rig.Bone> bones = new ArrayList<com.dynamo.rig.proto.Rig.Bone>();
            toDDF(bones, rootBone, 0xffff);

            skeletonBuilder.addAllBones(bones);
        }
        return skeletonBuilder.build();
    }

    private static XMLNode findSkeleton(XMLNode node) {
        if(node.type == XMLNode.Type.JOINT) {
            return node;
        }
        XMLNode rootNode = null;
        if(!node.childrenList.isEmpty()) {
            for(XMLNode childNode : node.childrenList) {
                rootNode = findSkeleton(childNode);
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
            Bone newBone = new Bone( child.sid, child.name, child.matrix.matrix4f, new Quaternion4f( 0f, 0f, 0f, 1f ) );
            bone.addChild( newBone );
            loadJoint( localToWorld, child, upVector, parentTip, nodeTip, newBone, boneList );
        }
    }

    private static void toDDF(ArrayList<com.dynamo.rig.proto.Rig.Bone> builderList, Bone bone, int parentIndex) {

        int boneIndex = builderList.size();
        com.dynamo.rig.proto.Rig.Bone.Builder b = com.dynamo.rig.proto.Rig.Bone.newBuilder();

        b.setParent(parentIndex);
        b.setId(MurmurHash.hash64(bone.getName()));

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


}
