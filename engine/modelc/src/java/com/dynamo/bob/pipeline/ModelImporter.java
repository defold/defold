//
// License: MIT
//

package com.dynamo.bob.pipeline;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.lang.reflect.Method;

public class ModelImporter {

    static final String ROOT_BONE_NAME = "root";
    static final String LIBRARY_NAME = "modelc_shared";

    static {
        Class<?> clsbob = null;

        try {
            ClassLoader clsloader = ClassLoader.getSystemClassLoader();
            clsbob = clsloader.loadClass("com.dynamo.bob.Bob");
        } catch (Exception e) {
            System.out.printf("Didn't find Bob class in default system class loader: %s\n", e);
        }

        if (clsbob == null) {
            try {
                // ClassLoader.getSystemClassLoader() doesn't work with junit
                ClassLoader clsloader = ModelImporter.class.getClassLoader();
                clsbob = clsloader.loadClass("com.dynamo.bob.Bob");
            } catch (Exception e) {
                System.out.printf("Didn't find Bob class in default test class loader: %s\n", e);
            }
        }

        if (clsbob != null)
        {
            try {
                Method getSharedLib = clsbob.getMethod("getSharedLib", String.class);
                Method addToPaths = clsbob.getMethod("addToPaths", String.class);
                File lib = (File)getSharedLib.invoke(null, LIBRARY_NAME);
                System.load(lib.getAbsolutePath());
            } catch (Exception e) {
                System.err.printf("Failed to find functions in Bob: %s\n", e);
                System.exit(1);
            }
        }
        else {
            try {
                System.out.printf("Fallback to regular System.loadLibrary(%s)\n", LIBRARY_NAME);
                System.loadLibrary(LIBRARY_NAME); // Requires the java.library.path to be set
            } catch (Exception e) {
                System.err.printf("Native code library failed to load: %s\n", e);
                System.exit(1);
            }
        }
    }

    // The suffix of the path dictates which loader it will use
    public static native Scene LoadFromBufferInternal(String path, byte[] buffer, Object data_resolver);
    public static native int AddressOf(Object o);
    public static native void TestException(String message);

    public static class ModelException extends Exception {
        public ModelException(String errorMessage) {
            super(errorMessage);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////

    static public class Options {
        public int dummy;

        public Options() {
            this.dummy = 0;
        }
    }

    public static class Vec4 { // simd Vector3/Vector4/Quat
        public float x, y, z, w;

        public Vec4() {}
        public Vec4(Vec4 other)
        {
            this.x = other.x;
            this.y = other.y;
            this.z = other.z;
            this.w = other.w;
        }
        public Vec4(float x, float y, float z, float w) {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }
    }

    public static class Transform {
        public Vec4 translation;
        public Vec4 rotation;
        public Vec4 scale;

        public Transform() {
            this.setIdentity();
        }

        public Transform setIdentity() {
            this.translation = new Vec4(0,0,0,1); // i.e. a Point3(x,y,z,1)
            this.rotation = new Vec4(0,0,0,1);
            this.scale = new Vec4(1,1,1,1);
            return this;
        }
    }

    public static class Aabb {
        public Vec4 min;
        public Vec4 max;

        public Aabb() {
            this.min = new Vec4(1000000.0f,1000000.0f,1000000.0f,1.0f);
            this.max = new Vec4(-1000000.0f,-1000000.0f,-1000000.0f,1.0f);
        }

        public Aabb(Aabb other) {
            this.min = new Vec4(other.min);
            this.max = new Vec4(other.max);
        }

        public void expand(float x, float y, float z) {
            if (x < min.x) min.x = x;
            if (y < min.y) min.y = y;
            if (z < min.z) min.z = z;
            if (x > max.x) max.x = x;
            if (y > max.y) max.y = y;
            if (z > max.z) max.z = z;
        }

        public Vec4 center() {
            return new Vec4((min.x + max.x)*0.5f, (min.y + max.y)*0.5f, (min.z + max.z)*0.5f, 1.0f);
        }

        public boolean isValid() {
            return min.x <= max.x && (min.y <= max.y) && (min.z <= max.z);
        }
    }
    public static class Material {
        public String           name;
        public int              index;

        public Material() {
            name = "";
            index = 0;
        }
    }

    public static class Mesh {
        public String      name;
        public Material    material;

        public Aabb        aabb;

        public float[]     positions; // float3
        public float[]     normals; // float3
        public float[]     tangents; // float4
        public float[]     colors; // float4
        public float[]     weights; // float4
        public int[]       bones; // uint4

        public int         texCoords0NumComponents; // 2 or 3
        public float[]     texCoords0; // float2 or float3
        public int         texCoords1NumComponents; // 2 or 3
        public float[]     texCoords1; // float2 or float3

        public int[]       indices;

        public int         vertexCount;
        public int         indexCount;

        public float[] getTexCoords(int index) {
            assert(index < 2);
            if (index == 1) {
                return this.texCoords1;
            }
            return this.texCoords0;
        }
    }

    public static class Model {
        public String   name;
        public Mesh[]   meshes;
        public int      index;
        public String   boneParentName; // If set, this is a child of a bone
    }

    public static class Bone {
        public Transform invBindPose;
        public String    name;
        public int       index;      // index into list of bones
        public Node      node;
        public Bone      parent;
    }

    public static class Skin {
        public String    name;
        public Bone[]    bones;
        public int       index;
    }

    public static class Node {
        public Transform    local;
        public Transform    world;
        public String       name;
        public int          index;
        public Node         parent;
        public Node[]       children;
        public Model        model;
        public Skin         skin;
    }

    public static class KeyFrame {
        public float value[] = new float[4];
        public float time;
    }

    public static class NodeAnimation {
        public Node         node;
        public KeyFrame[]   translationKeys;
        public KeyFrame[]   rotationKeys;
        public KeyFrame[]   scaleKeys;
        public float        startTime;
        public float        endTime;
    }

    public static class Animation {
        public String           name;
        public NodeAnimation[]  nodeAnimations;
        public float            duration;
    }

    public static class Buffer {
        public String           uri;
        public byte[]           buffer;
    }
    public static class Scene {

        public Node[]         nodes;
        public Model[]        models;
        public Skin[]         skins;
        public Node[]         rootNodes;
        public Animation[]    animations;
        public Material[]     materials;
        public Buffer[]       buffers;
    }

    public interface DataResolver {
        public byte[] getData(String path, String uri);
    }

    public static Scene LoadFromBuffer(Options options, String path, byte[] bytes, DataResolver data_resolver)
    {
        return ModelImporter.LoadFromBufferInternal(path, bytes, data_resolver);
    }

    // ////////////////////////////////////////////////////////////////////////////////

    private static void Usage() {
        System.out.printf("Usage: ModelImporter.class <model_file>\n");
        System.out.printf("\n");
    }

    private static void PrintIndent(int indent) {
        for (int i = 0; i < indent; ++i) {
            System.out.printf("  ");
        }
    }

    public static void DebugPrintTransform(Transform transform, int indent) {
        PrintIndent(indent);
        System.out.printf("t: %f, %f, %f\n", transform.translation.x, transform.translation.y, transform.translation.z);
        PrintIndent(indent);
        System.out.printf("r: %f, %f, %f, %f\n", transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
        PrintIndent(indent);
        System.out.printf("s: %f, %f, %f\n", transform.scale.x, transform.scale.y, transform.scale.z);
    }

    private static void DebugPrintNode(Node node, int indent) {
        PrintIndent(indent);
        System.out.printf("Node: %s  idx: %d   mesh: %s\n", node.name, node.index, node.model==null?"null":node.model.name);

        PrintIndent(indent+1); System.out.printf("local\n");
        DebugPrintTransform(node.local, indent+1);

        PrintIndent(indent+1); System.out.printf("world\n");
        DebugPrintTransform(node.world, indent+1);
    }

    private static void DebugPrintTree(Node node, int indent) {
        DebugPrintNode(node, indent);

        for (Node child : node.children) {
            DebugPrintTree(child, indent+1);
        }
    }

    private static void DebugPrintFloatArray(int indent, String name, float[] arr, int count, int elements)
    {
        if (arr == null)
            return;
        PrintIndent(indent);
        System.out.printf("%s:\t", name);
        for (int i = 0; i < count; ++i) {
            System.out.printf("(");
            for (int e = 0; e < elements; ++e) {
                System.out.printf("%f", arr[i*elements + e]);
                if (e < (elements-1))
                    System.out.printf(", ");
            }
            System.out.printf(")");
            if (i < (count-1))
                System.out.printf(", ");
        }
        System.out.printf("\n");
    }

    private static void DebugPrintIntArray(int indent, String name, int[] arr, int count, int elements)
    {
        if (arr == null)
            return;
        PrintIndent(indent);
        System.out.printf("%s:\t", name);
        for (int i = 0; i < count; ++i) {
            System.out.printf("(");
            for (int e = 0; e < elements; ++e) {
                System.out.printf("%d", arr[i*elements + e]);
                if (e < (elements-1))
                    System.out.printf(", ");
            }
            System.out.printf(")");
            if (i < (count-1))
                System.out.printf(", ");
        }
        System.out.printf("\n");
    }

    private static void DebugPrintMaterial(Material material, int indent) {
        PrintIndent(indent);
        System.out.printf("Material: %s\n", material.name);
        // PrintIndent(indent+1);
        // System.out.printf("Num Vertices: %d\n", mesh.vertexCount);
    }

    private static void DebugPrintMesh(Mesh mesh, int indent) {
        PrintIndent(indent);
        System.out.printf("Mesh: %s\n", mesh.name);
        PrintIndent(indent+1);
        System.out.printf("Num Vertices: %d\n", mesh.vertexCount);
        PrintIndent(indent+1);
        System.out.printf("Material: %s\n", mesh.material!=null?mesh.material.name:"null");
        PrintIndent(indent+1);
        System.out.printf("Aabb: (%f, %f, %f), (%f, %f, %f)\n", mesh.aabb.min.x,mesh.aabb.min.y,mesh.aabb.min.z, mesh.aabb.max.x,mesh.aabb.max.y,mesh.aabb.max.z);

        // Print out the first ten of each array
        int max_count = 10;
        if (max_count > mesh.vertexCount)
            max_count = mesh.vertexCount;
        DebugPrintFloatArray(indent+1, "positions", mesh.positions, max_count, 3);
        DebugPrintFloatArray(indent+1, "normals", mesh.normals, max_count, 3);
        DebugPrintFloatArray(indent+1, "tangents", mesh.tangents, max_count, 4);
        DebugPrintFloatArray(indent+1, "colors", mesh.colors, max_count, 4);
        DebugPrintFloatArray(indent+1, "weights", mesh.weights, max_count, 4);
        DebugPrintIntArray(indent+1, "bones", mesh.bones, max_count, 4);

        DebugPrintFloatArray(indent+1, "texCoords0", mesh.getTexCoords(0), max_count, mesh.texCoords0NumComponents);
        DebugPrintFloatArray(indent+1, "texCoords1", mesh.getTexCoords(1), max_count, mesh.texCoords1NumComponents);

        if (max_count > mesh.indexCount/3)
            max_count = mesh.indexCount/3;
        DebugPrintIntArray(indent+1, "indices", mesh.indices, max_count, 3);
    }

    private static void DebugPrintModel(Model model, int indent) {
        PrintIndent(indent);
        System.out.printf("Model: %s", model.name);
            if (!model.boneParentName.isEmpty())
                System.out.printf("  bone: %s", model.boneParentName);
        System.out.printf("\n");

        for (Mesh mesh : model.meshes) {
            DebugPrintMesh(mesh, indent+1);
        }
    }

    private static void DebugPrintKeyFrames(String name, KeyFrame[] keys, int indent)
    {
        PrintIndent(indent);
        System.out.printf("node: %s\t", name);
        for (KeyFrame key : keys)
        {
            System.out.printf("(t=%f: %f, %f, %f, %f), ", key.time, key.value[0],key.value[1],key.value[2],key.value[3]);
        }
        System.out.printf("\n");
    }

    private static void DebugPrintNodeAnimation(NodeAnimation nodeAnimation, int indent) {
        PrintIndent(indent);
        System.out.printf("node: %s num keys: t: %d  r: %d  s: %d  time: %f / %f\n", nodeAnimation.node.name,
            nodeAnimation.translationKeys.length,
            nodeAnimation.rotationKeys.length,
            nodeAnimation.scaleKeys.length,
            nodeAnimation.startTime,
            nodeAnimation.endTime);

        DebugPrintKeyFrames("translation", nodeAnimation.translationKeys, indent+1);
        DebugPrintKeyFrames("rotation", nodeAnimation.rotationKeys, indent+1);
        DebugPrintKeyFrames("scale", nodeAnimation.scaleKeys, indent+1);
    }

    private static void DebugPrintAnimation(Animation animation, int indent) {
        PrintIndent(indent);
        System.out.printf("animation: %s  duration: %f\n", animation.name, animation.duration);

        for (NodeAnimation nodeAnim : animation.nodeAnimations) {
            DebugPrintNodeAnimation(nodeAnim, indent+1);
        }
    }

    private static void DebugPrintBone(Bone bone, int indent) {
        PrintIndent(indent);
        System.out.printf("Bone: %s  node: %s\n", bone.name, bone.node==null?"null":bone.node.name);
        DebugPrintTransform(bone.invBindPose, indent+1);
    }

    private static void DebugPrintSkin(Skin skin, int indent) {
        PrintIndent(indent);
        System.out.printf("skin: %s\n", skin.name);

        for (Bone bone : skin.bones) {
            DebugPrintBone(bone, indent+1);
        }
    }

    public static byte[] ReadFile(File file) throws IOException
    {
        InputStream inputStream = new FileInputStream(file);
        byte[] bytes = new byte[inputStream.available()];
        inputStream.read(bytes);
        return bytes;
    }

    public static byte[] ReadFile(String path) throws IOException
    {
        return ReadFile(new File(path));
    }

    public static class FileDataResolver implements DataResolver
    {
        File cwd = null;

        FileDataResolver() {}
        FileDataResolver(File _cwd) {
            cwd = _cwd;
        }

        public byte[] getData(String path, String uri) {
            File file;
            if (cwd != null)
                file = new File(cwd, path);
            else
                file = new File(path);
            File bufferFile = new File(file.getParentFile(), uri);
            try {
                return ReadFile(bufferFile);
            } catch (Exception e) {
                System.out.printf("Failed to read file '%s': %s\n", uri, e);
                return null;
            }
        }
    };

    // Used for testing the importer. Usage:
    //    cd engine/modelc
    //   ./scripts/test_model_importer.sh <model path>
    public static void main(String[] args) throws IOException {
        System.setProperty("java.awt.headless", "true");

        if (args.length < 1) {
            Usage();
            return;
        }

        // TODO: Setup a java test suite for the model importer
        for (int i = 0; i < args.length; ++i)
        {
            if (args[i].equalsIgnoreCase("--test-exception"))
            {
                ModelImporter.TestException("Testing exception: " + args[i+1]);
                return; // exit code 0
            }
        }

        String path = args[0];       // name.glb/.gltf
        long timeStart = System.currentTimeMillis();

        FileDataResolver buffer_resolver = new FileDataResolver();
        Scene scene = LoadFromBuffer(new Options(), path, ReadFile(path), buffer_resolver);

        long timeEnd = System.currentTimeMillis();

        System.out.printf("Loaded %s %s\n", path, scene!=null ? "ok":"failed");
        System.out.printf("Loading took %d ms\n", (timeEnd - timeStart));

        if (scene == null)
            return;

        System.out.printf("--------------------------------\n");

        for (Buffer buffer : scene.buffers) {
            System.out.printf("Buffer: uri: %s  data: %s\n", buffer.uri, buffer.buffer);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Materials: %d\n", scene.materials.length);
        for (Material material : scene.materials)
        {
            DebugPrintMaterial(material, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Nodes: %d\n", scene.nodes.length);
        for (Node node : scene.nodes)
        {
            DebugPrintNode(node, 0);
        }

        System.out.printf("--------------------------------\n");

        for (Node root : scene.rootNodes) {
            System.out.printf("Root Node: %s\n", root.name);
            DebugPrintTree(root, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Skins: %d\n", scene.skins.length);
        for (Skin skin : scene.skins)
        {
            DebugPrintSkin(skin, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Models: %d\n", scene.models.length);
        for (Model model : scene.models) {
            DebugPrintModel(model, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Animations: %d\n", scene.animations.length);
        for (Animation animation : scene.animations) {
            DebugPrintAnimation(animation, 0);
        }

        System.out.printf("--------------------------------\n");
    }
}
