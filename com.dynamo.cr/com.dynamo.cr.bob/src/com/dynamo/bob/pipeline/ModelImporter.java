//
// License: MIT
//

package com.dynamo.bob.pipeline;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.util.List;
import java.util.Arrays;
import java.lang.reflect.Method;

import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.PointerType;
import com.sun.jna.Structure;
import com.sun.jna.ptr.IntByReference;

// Type Mapping in JNA
// https://java-native-access.github.io/jna/3.5.1/javadoc/overview-summary.html#marshalling


public class ModelImporter {

    static boolean isBob() {
        try {
            Class.forName("com.dynamo.bob.Bob");
            return true;
        } catch(Exception e) {
            return false;
        }
    }

    static final String LIBRARY_NAME = "model_shared";

    static {
        try {
            // Extract and append Bob bundled texc_shared path.
            if (isBob())
            {
                Class<?> bob_cls = Class.forName("com.dynamo.bob.Bob");
                Method getSharedLib = bob_cls.getMethod("getSharedLib", String.class);
                Method addToPaths = bob_cls.getMethod("addToPaths", File.class);

                File lib = (File)getSharedLib.invoke(null, LIBRARY_NAME);
                addToPaths.invoke(null, lib.getParent());
            }
            Native.register(LIBRARY_NAME);

        } catch (Exception e) {
            System.out.println("FATAL: " + e.getMessage());
        }
    }

    public static class ModelException extends Exception {
        public ModelException(String errorMessage) {
            super(errorMessage);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    static public class Options extends Structure {
        public int dummy;
        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"dummy"});
        }
        public Options() {
            this.dummy = 0;
        }
    }

    static public class Vec4 extends Structure { // simd Vector3/Vector4/Quat
        public float x, y, z, w;
        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"x","y","z","w"});
        }
        public Vec4() {super();}
    }

    static public class Transform extends Structure {
        // The order is determined by dmTransform::Transform
        public Vec4 rotation;
        public Vec4 translation;
        public Vec4 scale;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"rotation", "translation","scale"});
        }
        public Transform() {super();}
    }

    static public class Mesh extends Structure {
        public static class ByReference extends Mesh implements Structure.ByReference {}

        public Pointer     positions; // float3
        public Pointer     normals; // float3
        public Pointer     tangents; // float3
        public Pointer     colors; // float4
        public Pointer     weights; // float4
        public Pointer     bones; // uint4
        public int         texCoords0NumComponents; // 2 or 3
        public Pointer     texCoords0; // float2 or float3
        public int         texCoords1NumComponents; // 2 or 3
        public Pointer     texCoords1; // float2 or float3

        public Pointer     indices;

        public int         vertexCount;
        public int         indexCount;
        public String      material;
        public String      name;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {
                "positions","normals","tangents","colors","weights","bones",
                "texCoords0NumComponents","texCoords0","texCoords1NumComponents","texCoords1",
                "indices",
                "vertexCount","indexCount","material","name"
            });
        }
        public Mesh() {}

        @SuppressWarnings("unchecked")
        public Mesh[] castToArray(int size) {
            return (Mesh[])super.toArray(size);
        }

        private float[] getFloatArray(Pointer p, int count) {
            if (p == null)
                return null;
            return p.getFloatArray(0, count);
        }
        public float[] getPositions() {
            return getFloatArray(this.positions, this.vertexCount*3);
        }
        public float[] getNormals() {
            return getFloatArray(this.normals, this.vertexCount*3);
        }
        public float[] getTangents() {
            return getFloatArray(this.tangents, this.vertexCount*3);
        }
        public float[] getColors() {
            return getFloatArray(this.tangents, this.vertexCount*4);
        }
        public float[] getWeights() {
            return getFloatArray(this.weights, this.vertexCount*4);
        }
        public int[] getBones() {
            if (this.bones == null)
                return null;
            return this.bones.getIntArray(0, this.vertexCount*4);
        }
        public int[] getIndices() {
            if (this.indices == null || this.indexCount == 0)
                return null;
            return this.indices.getIntArray(0, this.indexCount);
        }

        public float[] getTexCoords(int index) {
            assert(index < 2);
            Pointer p = this.texCoords0;
            int num_elements = this.texCoords0NumComponents;
            if (index == 1) {
                p = this.texCoords1;
                num_elements = this.texCoords1NumComponents;
            }
            return getFloatArray(p, this.vertexCount*num_elements);
        }
    }

    static public class Model extends Structure {
        public static class ByReference extends Model implements Structure.ByReference {}

        public String               name;
        public Mesh.ByReference     meshes;
        public int                  meshesCount;
        public int                  index;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"name","meshes","meshesCount","index"});
        }
        public Model() {}

        @SuppressWarnings("unchecked")
        public Model[] castToArray(int size) {
            return (Model[])super.toArray(size);
        }

        public Mesh[] getMeshes() {
            return this.meshes.castToArray(this.meshesCount);
        }
    }

    static public class Bone extends Structure {
        public static class ByReference extends Bone implements Structure.ByReference {}

        public Transform            invBindPose;
        public String               name;
        public Node.ByReference     node;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"invBindPose","name","node"});
        }
        public Bone() {}

        @SuppressWarnings("unchecked")
        public Bone[] castToArray(int size) {
            return (Bone[])super.toArray(size);
        }
    }

    static public class Skin extends Structure {
        public static class ByReference extends Skin implements Structure.ByReference {}

        public String               name;
        public Bone.ByReference     bones;
        public int                  bonesCount;
        public int                  index;


        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"name","bones","bonesCount","index"});
        }
        public Skin() {}
        public Skin(Pointer p) {
            super(p);
            this.read();
            System.out.printf("Skin constructor\n");
        }

        @SuppressWarnings("unchecked")
        public Skin[] castToArray(int size) {
            return (Skin[])super.toArray(size);
        }

        public Bone[] getBones() {
            return this.bones.castToArray(this.bonesCount);
        }
    }

    static public class Node extends Structure {
        public static class ByReference extends Node implements Structure.ByReference {}

        public Transform            transform;
        public String               name;
        public Model.ByReference    model;
        public Skin.ByReference     skin;
        public Node.ByReference     parent;
        public Pointer              children;
        public int                  childrenCount;
        public int                  index;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {
                "transform", "name",
                "model","skin","parent","children","childrenCount","index"
            });
        }
        public Node() {}
        public Node(Pointer p) {
            super(p);
            this.read();
            System.out.printf("Node constructor\n");
        }

        @SuppressWarnings("unchecked")
        public Node[] castToArray(int size) {
            return (Node[])super.toArray(size);
        }

        public Node[] getChildren() {
            Pointer[] childPointers = this.children.getPointerArray(0, this.childrenCount);
            Node[] childNodes = new Node[this.childrenCount];

            int i = 0;
            for (Pointer p : childPointers) {
                childNodes[i++] = new Node(p);
            }
            return childNodes;
        }
    }

    static public class KeyFrame extends Structure {
        public static class ByReference extends KeyFrame implements Structure.ByReference {}

        public float                    value[] = new float[4];
        public float                    time;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"value","time"});
        }
        public KeyFrame() {}

        @SuppressWarnings("unchecked")
        public KeyFrame[] castToArray(int size) {
            return (KeyFrame[])super.toArray(size);
        }
    }

    static public class NodeAnimation extends Structure {
        public static class ByReference extends NodeAnimation implements Structure.ByReference {}

        public Node.ByReference         node;
        public KeyFrame.ByReference     translationKeys;
        public KeyFrame.ByReference     rotationKeys;
        public KeyFrame.ByReference     scaleKeys;
        public int                      translationKeysCount;
        public int                      rotationKeysCount;
        public int                      scaleKeysCount;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"node","translationKeys","rotationKeys","scaleKeys",
            "translationKeysCount","rotationKeysCount","scaleKeysCount"});
        }
        public NodeAnimation() {}

        @SuppressWarnings("unchecked")
        public NodeAnimation[] castToArray(int size) {
            return (NodeAnimation[])super.toArray(size);
        }

        public KeyFrame[] getTranslationKeys() {
            return this.translationKeys.castToArray(this.translationKeysCount);
        }
        public KeyFrame[] getRotationKeys() {
            return this.rotationKeys.castToArray(this.rotationKeysCount);
        }
        public KeyFrame[] getScaleKeys() {
            return this.scaleKeys.castToArray(this.scaleKeysCount);
        }
    }

    static public class Animation extends Structure {
        public static class ByReference extends Animation implements Structure.ByReference {}

        public String                       name;
        public NodeAnimation.ByReference    nodeAnimations;
        public int                          nodeAnimationsCount;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"name","nodeAnimations", "nodeAnimationsCount" });
        }
        public Animation() {}

        @SuppressWarnings("unchecked")
        public Animation[] castToArray(int size) {
            return (Animation[])super.toArray(size);
        }

        public NodeAnimation[] getNodeAnimations() {
            return this.nodeAnimations.castToArray(this.nodeAnimationsCount);
        }
    }

    static public class Scene extends Structure {
        public Pointer opaqueSceneData;
        public Pointer destroyFn;

        public Node.ByReference         nodes;
        public int                      nodesCount;

        public Model.ByReference        models;
        public int                      modelsCount;

        public Skin.ByReference         skins;
        public int                      skinsCount;

        public Pointer                  rootNodes;
        public int                      rootNodesCount;

        public Animation.ByReference    animations;
        public int                      animationsCount;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {
                "opaqueSceneData", "destroyFn",
                "nodes", "nodesCount",
                "models", "modelsCount",
                "skins", "skinsCount",
                "rootNodes", "rootNodesCount",
                "animations", "animationsCount",
            });
        }

        public Scene() {super();}

        public Node[] getNodes() {
            return this.nodes.castToArray(this.nodesCount);
        }

        public Node[] getRootNodes() {
            Pointer[] rootPointers = this.rootNodes.getPointerArray(0, this.rootNodesCount);
            Node[] rootNodes = new Node[this.rootNodesCount];

            int i = 0;
            for (Pointer p : rootPointers) {
                Node node = new Node(p);
                node.read();
                rootNodes[i++] = node;
            }
            return rootNodes;
        }

        public Skin[] getSkins() {
            if (this.skins == null || this.skinsCount == 0)
                return new Skin[0];
            return this.skins.castToArray(this.skinsCount);
        }

        public Model[] getModels() {
            if (this.models == null || this.modelsCount == 0)
                return new Model[0];
            return this.models.castToArray(this.modelsCount);
        }

        public Animation[] getAnimations() {
            if (this.animations == null || this.animationsCount == 0)
                return new Animation[0];
            return this.animations.castToArray(this.animationsCount);
        }
    }


    ////////////////////////////////////////////////////////////////////////////////
    public static native void AssertSizes(int sz_transform,
                                         int sz_mesh,
                                         int sz_model,
                                         int sz_bone,
                                         int sz_skin,
                                         int sz_node,
                                         int sz_keyframe,
                                         int sz_nodeanimation,
                                         int sz_animation,
                                         int sz_scene);
    public static native Scene LoadFromBuffer(Options options, String suffix, Buffer buffer, int bufferSize);
    public static native Scene LoadFromPath(Options options, String path);
    public static native void DestroyScene(Scene scene);

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

    private static void DebugPrintTransform(Transform transform, int indent) {
        PrintIndent(indent);
        System.out.printf("t: %f, %f, %f\n", transform.translation.x, transform.translation.y, transform.translation.z);
        PrintIndent(indent);
        System.out.printf("r: %f, %f, %f, %f\n", transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.z);
        PrintIndent(indent);
        System.out.printf("s: %f, %f, %f\n", transform.scale.x, transform.scale.y, transform.scale.z);
    }

    private static void DebugPrintNode(Node node, int indent) {
        PrintIndent(indent);
        System.out.printf("Node: %s  idx: %d   mesh: %s\n", node.name, node.index, node.model!=null?node.model.name:"-");
        DebugPrintTransform(node.transform, indent+1);
    }

    private static void DebugPrintTree(Node node, int indent) {
        DebugPrintNode(node, indent);

        for (Node child : node.getChildren()) {
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

    private static void DebugPrintMesh(Mesh mesh, int indent) {
        PrintIndent(indent);
        System.out.printf("Mesh: %s\n", mesh.name);
        PrintIndent(indent+1);
        System.out.printf("Num Vertices: %d\n", mesh.vertexCount);
        PrintIndent(indent+1);
        System.out.printf("Material: %s\n", mesh.material);

        // Print out the first ten of each array
        int max_count = 10;
        if (max_count > mesh.vertexCount)
            max_count = mesh.vertexCount;
        DebugPrintFloatArray(indent+1, "positions", mesh.getPositions(), max_count, 3);
        DebugPrintFloatArray(indent+1, "normals", mesh.getNormals(), max_count, 3);
        DebugPrintFloatArray(indent+1, "tangents", mesh.getTangents(), max_count, 3);
        DebugPrintFloatArray(indent+1, "colors", mesh.getColors(), max_count, 4);
        DebugPrintFloatArray(indent+1, "weights", mesh.getWeights(), max_count, 4);
        DebugPrintIntArray(indent+1, "bones", mesh.getBones(), max_count, 4);

        DebugPrintFloatArray(indent+1, "texCoords0", mesh.getTexCoords(0), max_count, mesh.texCoords0NumComponents);
        DebugPrintFloatArray(indent+1, "texCoords1", mesh.getTexCoords(1), max_count, mesh.texCoords1NumComponents);

        if (max_count > mesh.indexCount)
            max_count = mesh.indexCount;
        DebugPrintIntArray(indent+1, "indices", mesh.getIndices(), max_count, 3);
    }

    private static void DebugPrintModel(Model model, int indent) {
        PrintIndent(indent);
        System.out.printf("Model: %s\n", model.name);

        for (Mesh mesh : model.getMeshes()) {
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
        System.out.printf("node: %s num keys: t: %d  r: %d  s: %d\n", nodeAnimation.node.name,
            nodeAnimation.translationKeysCount,
            nodeAnimation.rotationKeysCount,
            nodeAnimation.scaleKeysCount);

        DebugPrintKeyFrames("translation", nodeAnimation.getTranslationKeys(), indent+1);
        DebugPrintKeyFrames("rotation", nodeAnimation.getRotationKeys(), indent+1);
        DebugPrintKeyFrames("scale", nodeAnimation.getScaleKeys(), indent+1);
    }

    private static void DebugPrintAnimation(Animation animation, int indent) {
        PrintIndent(indent);
        System.out.printf("animation: %s\n", animation.name);

        for (NodeAnimation nodeAnim : animation.getNodeAnimations()) {
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

        for (Bone bone : skin.getBones()) {
            DebugPrintBone(bone, indent+1);
        }
    }

    private static <T extends Structure> int sizeof(Class<T> cls)
    {
        return Structure.newInstance(cls).size();
    }

    private static void CheckSizes()
    {
        AssertSizes(sizeof(Transform.class),
                    sizeof(Mesh.class),
                    sizeof(Model.class),
                    sizeof(Bone.class),
                    sizeof(Skin.class),
                    sizeof(Node.class),
                    sizeof(KeyFrame.class),
                    sizeof(NodeAnimation.class),
                    sizeof(Animation.class),
                    sizeof(Scene.class));

    }

    // Used for testing functions
    // See ./com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/pipeline/test_model_importer.sh
    public static void main(String[] args) throws IOException {
        System.setProperty("java.awt.headless", "true");

        if (args.length < 1) {
            Usage();
            return;
        }

        CheckSizes();

        String path = args[0];       // .glb

        Options options = new Options();
        Scene scene = LoadFromPath(options, path);

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Nodes: %d\n", scene.nodesCount);
        for (Node node : scene.getNodes())
        {
            DebugPrintNode(node, 0);
        }

        System.out.printf("--------------------------------\n");

        for (Node root : scene.getRootNodes()) {
            System.out.printf("Root Node: %s\n", root.name);
            DebugPrintTree(root, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Skins: %d\n", scene.skinsCount);
        for (Skin skin : scene.getSkins())
        {
            DebugPrintSkin(skin, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Models: %d\n", scene.modelsCount);
        for (Model model : scene.getModels()) {
            DebugPrintModel(model, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num Animations: %d\n", scene.animationsCount);
        for (Animation animation : scene.getAnimations()) {
            DebugPrintAnimation(animation, 0);
        }

        System.out.printf("--------------------------------\n");
    }
}
