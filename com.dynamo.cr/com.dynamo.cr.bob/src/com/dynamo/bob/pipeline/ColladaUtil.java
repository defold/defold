package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import javax.xml.stream.XMLInputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamReader;

import org.jagatoo.loaders.models.collada.stax.XMLCOLLADA;
import org.jagatoo.loaders.models.collada.stax.XMLGeometry;
import org.jagatoo.loaders.models.collada.stax.XMLInput;
import org.jagatoo.loaders.models.collada.stax.XMLMesh;
import org.jagatoo.loaders.models.collada.stax.XMLSource;

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

}
