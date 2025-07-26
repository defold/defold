//
// License: MIT
//

package com.dynamo.bob.pipeline;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.util.Arrays;
import java.util.Comparator;

public class ModelImporterJni {

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
                ClassLoader clsloader = ModelImporterJni.class.getClassLoader();
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
            } catch (InvocationTargetException e) {
                Throwable cause = e.getCause();
                System.err.printf("Failed to find functions in Bob: %s\n", cause.getMessage());
                cause.printStackTrace();
                System.exit(1);
            } catch (Exception e) {
                System.err.printf("Failed to find functions in Bob: %s\n", e);
                e.printStackTrace();
                System.exit(1);
            }
        }
        else {
            try {
                System.out.printf("Fallback to regular System.loadLibrary(%s)\n", LIBRARY_NAME);
                System.loadLibrary(LIBRARY_NAME); // Requires the java.library.path to be set
            } catch (Exception e) {
                System.err.printf("Native code library failed to load: %s\n", e);
                e.printStackTrace();
                System.exit(1);
            }
        }
    }

    // The suffix of the path dictates which loader it will use
    public static native Modelimporter.Scene LoadFromBufferInternal(String path, byte[] buffer, Object data_resolver);
    //public static native int AddressOf(Object o);
    public static native void TestException(String message);

    public static class ModelException extends Exception {
        public ModelException(String errorMessage) {
            super(errorMessage);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////

    public interface DataResolver {
        public byte[] getData(String path, String uri);
    }

    public static Modelimporter.Scene LoadFromBuffer(Modelimporter.Options options, String path, byte[] bytes, DataResolver data_resolver)
    {
        return ModelImporterJni.LoadFromBufferInternal(path, bytes, data_resolver);
    }


    //////////////////////////////////////////////////////////////////////////////////
    // AABB

    public static Modelimporter.Aabb newAabb()
    {
        Modelimporter.Aabb aabb = new Modelimporter.Aabb();
        aabb.min = new Modelimporter.Vector3();
        aabb.max = new Modelimporter.Vector3();
        aabb.min.x = aabb.min.y = aabb.min.z = 1000000.0f;
        aabb.max.x = aabb.max.y = aabb.max.z = -1000000.0f;
        return aabb;
    }

    public static Modelimporter.Aabb expandAabb(Modelimporter.Aabb aabb, float x, float y, float z)
    {
        aabb.min.x = Math.min(aabb.min.x, x);
        aabb.min.y = Math.min(aabb.min.y, y);
        aabb.min.z = Math.min(aabb.min.z, z);

        aabb.max.x = Math.max(aabb.max.x, x);
        aabb.max.y = Math.max(aabb.max.y, y);
        aabb.max.z = Math.max(aabb.max.z, z);
        return aabb;
    }

    public static Modelimporter.Vector3 aabbCalcCenter(Modelimporter.Aabb aabb, Modelimporter.Vector3 center) {
        center.x = (aabb.min.x + aabb.max.x)*0.5f;
        center.y = (aabb.min.y + aabb.max.y)*0.5f;
        center.z = (aabb.min.z + aabb.max.z)*0.5f;
        return center;
    }

    public static boolean aabbIsIsValid(Modelimporter.Aabb aabb) {
        return aabb.min.x <= aabb.max.x && (aabb.min.y <= aabb.max.y) && (aabb.min.z <= aabb.max.z);
    }

    //////////////////////////////////////////////////////////////////////////////////

    private static void Usage() {
        System.out.printf("Usage: Modelimporter.class <model_file>\n");
        System.out.printf("\n");
    }

    public static void PrintIndent(int indent) {
        for (int i = 0; i < indent; ++i) {
            System.out.printf("  ");
        }
    }

    public static void DebugPrintTransform(Modelimporter.Transform transform, int indent) {
        PrintIndent(indent);
        System.out.printf("t: %f, %f, %f\n", transform.translation.x, transform.translation.y, transform.translation.z);
        PrintIndent(indent);
        System.out.printf("r: %f, %f, %f, %f\n", transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
        PrintIndent(indent);
        System.out.printf("s: %f, %f, %f\n", transform.scale.x, transform.scale.y, transform.scale.z);
    }

    public static void DebugPrintNode(Modelimporter.Node node, int indent) {
        PrintIndent(indent);
        System.out.printf("Node: %s  idx: %d   mesh: %s\n", node.name, node.index, node.model==null?"null":node.model.name);

        PrintIndent(indent+1); System.out.printf("local\n");
        DebugPrintTransform(node.local, indent+1);

        PrintIndent(indent+1); System.out.printf("world\n");
        DebugPrintTransform(node.world, indent+1);
    }

    public static void DebugPrintTree(Modelimporter.Node node, int indent) {
        DebugPrintNode(node, indent);

        for (Modelimporter.Node child : node.children) {
            DebugPrintTree(child, indent+1);
        }
    }

    public static void DebugPrintFloatArray(int indent, String name, float[] arr, int count, int elements)
    {
        if (arr == null || arr.length == 0)
            return;
        PrintIndent(indent);
        System.out.printf("%s: %d tuple\t", name, elements);
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
        if (arr == null || arr.length == 0)
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

    private static boolean IsModelImporterClass(Class<?> cls)
    {
        return cls.getName().startsWith("com.dynamo.bob.pipeline.Modelimporter");
    }

    public static void PrintArray(Object arr, StringBuilder builder)
    {
        builder.append("[");
        int length = Array.getLength(arr);
        for (int i = 0; i < length; i ++) {
            Object arrayElement = Array.get(arr, i);

            builder.append(arrayElement);
            if (i != (length-1))
            {
                builder.append(", ");
            }
        }
        builder.append("]");
    }

    public static void PrintFields(Object obj, Field[] fields, boolean skip_tructs, StringBuilder builder, int indent)
    {
        String newLine = System.getProperty("line.separator");
        for ( Field field : fields  )
        {
            Class field_cls = field.getType();

            boolean is_struct = IsModelImporterClass(field.getType());
            if (field_cls.isEnum())
                is_struct = false;

            if (skip_tructs && is_struct)
                continue;
            else if (!skip_tructs && !is_struct)
                continue;

            Object field_obj = null;
            try {
                //requires access to private field:
                field.setAccessible(true);
                field_obj = field.get(obj);
            } catch ( IllegalAccessException ex ) {
                System.out.println(ex);
                continue;
            }

            for (int i = 0; i < indent; ++i) {
                builder.append("  ");
            }

            if (is_struct)
            {
                builder.append( field.getName() );
                builder.append(": ");
                if (field_obj == null)
                {
                    builder.append( field_obj );
                    builder.append(newLine);
                }
                else
                {
                    builder.append(newLine);

                    ToString(field_obj, builder, indent+1);
                }
                continue;
            }

            builder.append( field.getName() );
            builder.append(": ");

            if (field_obj == null)
            {
                builder.append( field_obj );
            }
            else if (field_cls.isArray())
            {
                PrintArray(field_obj, builder);
            }
            else
            {
                builder.append( field_obj.toString() );
            }

            builder.append(newLine);
        }
    }

    public static StringBuilder ToString(Object obj, StringBuilder builder, int indent)
    {
        if (obj == null)
        {
            builder.append(obj);
            return builder;
        }

        Class cls = obj.getClass();

        //determine fields declared in this class only (no fields of superclass)
        Field[] fields = cls.getDeclaredFields();
        // Print "simple" fields first
        PrintFields(obj, fields, true, builder, indent);
        // Print "model" fields last
        PrintFields(obj, fields, false, builder, indent);

        return builder;
    }

    public static void DebugPrintObject(Object object, int indent) {
        StringBuilder builder = new StringBuilder();
        builder = ToString(object, builder, indent+1);
        System.out.printf("%s\n", builder.toString());
    }

    public static void DebugPrintMaterial(Modelimporter.Material material, int indent) {
        StringBuilder builder = new StringBuilder();
        builder = ToString(material, builder, indent+1);
        System.out.printf("%s\n", builder.toString());
    }

    public static void DebugPrintMesh(Modelimporter.Mesh mesh, int indent) {
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

        DebugPrintFloatArray(indent+1, "texCoords0", mesh.texCoords0, max_count, mesh.texCoords0NumComponents);
        DebugPrintFloatArray(indent+1, "texCoords1", mesh.texCoords1, max_count, mesh.texCoords1NumComponents);

        if (max_count > mesh.indices.length/3)
            max_count = mesh.indices.length/3;
        DebugPrintIntArray(indent+1, "indices", mesh.indices, max_count, 3);
    }

    public static void DebugPrintModel(Modelimporter.Model model, int indent) {
        PrintIndent(indent);
        System.out.printf("Model: %s", model.name);
            if (model.parentBone != null && !model.parentBone.name.isEmpty())
                System.out.printf("  bone: %s", model.parentBone.name);
        System.out.printf("\n");

        for (Modelimporter.Mesh mesh : model.meshes) {
            DebugPrintMesh(mesh, indent+1);
        }
    }

    public static void DebugPrintKeyFrames(String name, Modelimporter.KeyFrame[] keys, int indent)
    {
        PrintIndent(indent);
        System.out.printf("node: %s\t", name);
        for (Modelimporter.KeyFrame key : keys)
        {
            System.out.printf("(t=%f: %f, %f, %f, %f), ", key.time, key.value[0],key.value[1],key.value[2],key.value[3]);
        }
        System.out.printf("\n");
    }

    public static void DebugPrintNodeAnimation(Modelimporter.NodeAnimation nodeAnimation, int indent) {
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

    public static void DebugPrintAnimation(Modelimporter.Animation animation, int indent) {
        PrintIndent(indent);
        System.out.printf("animation: %s  duration: %f\n", animation.name, animation.duration);

        for (Modelimporter.NodeAnimation nodeAnim : animation.nodeAnimations) {
            DebugPrintNodeAnimation(nodeAnim, indent+1);
        }
    }

    public static void DebugPrintBone(Modelimporter.Bone bone, int indent) {
        PrintIndent(indent);
        System.out.printf("Bone: %s  node: %s\n", bone.name, bone.node==null?"null":bone.node.name);
        DebugPrintTransform(bone.invBindPose, indent+1);
    }

    public static void DebugPrintSkin(Modelimporter.Skin skin, int indent) {
        PrintIndent(indent);
        System.out.printf("skin: %s\n", skin.name);

        for (Modelimporter.Bone bone : skin.bones) {
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
            if (uri == null)
                return null; // Usually compressed buffers, which we don't support yet

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
                ModelImporterJni.TestException("Testing exception: " + args[i+1]);
                return; // exit code 0
            }
        }

        String path = args[0];       // name.glb/.gltf
        long timeStart = System.currentTimeMillis();

        FileDataResolver buffer_resolver = new FileDataResolver();
        Modelimporter.Scene scene = LoadFromBuffer(new Modelimporter.Options(), path, ReadFile(path), buffer_resolver);

        long timeEnd = System.currentTimeMillis();

        if (scene == null)
        {
            System.exit(1);
            return;
        }

        System.out.printf("Loaded %s %s\n", path, scene!=null ? "ok":"failed");
        System.out.printf("Loading took %d ms\n", (timeEnd - timeStart));

        System.out.printf("--------------------------------\n");

        if (scene.buffers == null)
        {
            System.out.printf("Buffers are null\n");
        }

        for (Modelimporter.Buffer buffer : scene.buffers) {
            System.out.printf("Buffer: uri: %s  data: %s\n", buffer.uri, buffer.buffer);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num images: %d\n", scene.images.length);
        for (Modelimporter.Image image : scene.images)
        {
            PrintIndent(1);
            System.out.printf("-----------------\n");
            DebugPrintObject(image, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Samplers: %d\n", scene.samplers.length);
        for (Modelimporter.Sampler sampler : scene.samplers)
        {
            PrintIndent(1);
            System.out.printf("-----------------\n");
            DebugPrintObject(sampler, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Textures: %d\n", scene.textures.length);
        for (Modelimporter.Texture texture : scene.textures)
        {
            PrintIndent(1);
            System.out.printf("-----------------\n");
            DebugPrintObject(texture, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Materials: %d\n", scene.materials.length);
        for (Modelimporter.Material material : scene.materials)
        {
            PrintIndent(1);
            System.out.printf("-----------------\n");
            DebugPrintObject(material, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Nodes: %d\n", scene.nodes.length);
        for (Modelimporter.Node node : scene.nodes)
        {
            PrintIndent(1);
            System.out.printf("-----------------\n");
            DebugPrintNode(node, 0);
        }

        System.out.printf("--------------------------------\n");

        for (Modelimporter.Node root : scene.rootNodes) {
            System.out.printf("Root Node: %s\n", root.name);
            DebugPrintTree(root, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Skins: %d\n", scene.skins.length);
        for (Modelimporter.Skin skin : scene.skins)
        {
            DebugPrintSkin(skin, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Models: %d\n", scene.models.length);
        for (Modelimporter.Model model : scene.models) {
            DebugPrintModel(model, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Animations: %d\n", scene.animations.length);
        for (Modelimporter.Animation animation : scene.animations) {
            DebugPrintAnimation(animation, 0);
        }

        System.out.printf("--------------------------------\n");
    }
}
