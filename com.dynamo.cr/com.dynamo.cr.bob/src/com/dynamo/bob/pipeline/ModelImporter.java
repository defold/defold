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

        public int         vertexCount;
        public String      material;
        public String      name;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {
                "positions","normals","tangents","colors","weights","bones",
                "texCoords0NumComponents","texCoords0","texCoords1NumComponents","texCoords1",
                "vertexCount","material","name"
            });
        }
        public Mesh() {}

        @SuppressWarnings("unchecked")
        public Mesh[] castToArray(int size) {
            return (Mesh[])super.toArray(size);
        }
    }

    static public class Model extends Structure {
        public static class ByReference extends Model implements Structure.ByReference {}

        public String               name;
        public Mesh.ByReference     meshes;
        public int                  meshesCount;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"name","meshes","meshesCount"});
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

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"name","bones","bonesCount"});
        }
        public Skin() {}

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

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {
                "transform", "name",
                "model","skin","parent","children","childrenCount"
            });
        }
        public Node() {}
        public Node(Pointer p) {
            super(p);
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
                Node node = new Node(p);
                node.read();
                childNodes[i++] = node;
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
            return this.skins.castToArray(this.skinsCount);
        }

        public Model[] getModels() {
            return this.models.castToArray(this.modelsCount);
        }

        public Animation[] getAnimations() {
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


    // public static native AABB.ByValue SPINE_GetAABB(SpinePointer spine);

    // ////////////////////////////////////////////////////////////////////////////////

    // static public class SpineBone extends Structure {
    //     public String name;
    //     public int parent;
    //     public float posX, posY, rotation, scaleX, scaleY, length;

    //     protected List getFieldOrder() {
    //         return Arrays.asList(new String[] {"name", "parent", "posX", "posY", "rotation", "scaleX", "scaleY", "length"});
    //     }
    //     public SpineBone() {
    //         this.name = "<empty>";
    //         this.parent = -1;
    //         this.posX = this.posY = 0.0f;
    //         this.scaleX = this.scaleY = 1.0f;
    //         this.rotation = 0.0f;
    //         this.length = 100.0f;
    //     }
    // }

    // static public class Bone {
    //     public String name;
    //     public int   index;
    //     public int   parent;
    //     public int[] children;
    //     public float posX, posY, rotation, scaleX, scaleY, length;
    // }

    // public static native int SPINE_GetNumBones(SpinePointer spine);
    // public static native String SPINE_GetBoneInternal(SpinePointer spine, int index, SpineBone bone);
    // public static native int SPINE_GetNumChildBones(SpinePointer spine, int bone);
    // public static native int SPINE_GetChildBone(SpinePointer spine, int bone, int index);

    // protected static Bone SPINE_GetBone(SpinePointer spine, int index) {
    //     Bone bone = new Bone();
    //     SpineBone internal = new SpineBone();
    //     SPINE_GetBoneInternal(spine, index, internal);
    //     bone.index = index;
    //     bone.name = internal.name;
    //     bone.posX = internal.posX;
    //     bone.posY = internal.posY;
    //     bone.scaleX = internal.scaleX;
    //     bone.scaleY = internal.scaleY;
    //     bone.rotation = internal.rotation;
    //     bone.length = internal.length;
    //     bone.parent = internal.parent;

    //     int num_children = SPINE_GetNumChildBones(spine, index);
    //     bone.children = new int[num_children];

    //     for (int i = 0; i < num_children; ++i) {
    //         bone.children[i] = SPINE_GetChildBone(spine, index, i);
    //     }
    //     return bone;
    // }

    // public static Bone[] SPINE_GetBones(SpinePointer spine) {
    //     int num_bones = SPINE_GetNumBones(spine);
    //     Bone[] bones = new Bone[num_bones];
    //     for (int i = 0; i < num_bones; ++i) {
    //         bones[i] = SPINE_GetBone(spine, i);
    //     }
    //     return bones;
    // }

    // ////////////////////////////////////////////////////////////////////////////////

    private static void Usage() {
        System.out.printf("Usage: ModelImporter.class <model_file>\n");
        System.out.printf("\n");
    }

    // private static void DebugPrintBone(Bone bone, Bone[] bones, int indent) {
    //     String tab = " ".repeat(indent * 4);
    //     System.out.printf("Bone:%s %s: idx: %d parent = %d, pos: %f, %f  scale: %f, %f  rot: %f  length: %f\n",
    //             tab, bone.name, bone.index, bone.parent, bone.posX, bone.posY, bone.scaleX, bone.scaleY, bone.rotation, bone.length);

    //     int num_children = bone.children.length;
    //     for (int i = 0; i < num_children; ++i){
    //         int child_index = bone.children[i];
    //         Bone child = bones[child_index];
    //         DebugPrintBone(child, bones, indent+1);
    //     }
    // }

    private static void PrintIndent(int indent) {
        for (int i = 0; i < indent; ++i) {
            System.out.printf("  ");
        }
    }

    private static void DebugPrintTree(Node node, int indent) {
        PrintIndent(indent);
        System.out.printf("Node: %s\n", node.name);

        for (Node child : node.getChildren()) {
            DebugPrintTree(child, indent+1);
        }
    }

    private static void DebugPrintMesh(Mesh mesh, int indent) {
        PrintIndent(indent);
        System.out.printf("Mesh: %s\n", mesh.name);
        PrintIndent(indent+1);
        System.out.printf("Num Vertices: %d\n", mesh.vertexCount);
        PrintIndent(indent+1);
        System.out.printf("Material: %s\n", mesh.material);
    }

    private static void DebugPrintModel(Model model, int indent) {
        PrintIndent(indent);
        System.out.printf("Model: %s\n", model.name);

        for (Mesh mesh : model.getMeshes()) {
            DebugPrintMesh(mesh, indent+1);
        }
    }

    private static void DebugPrintNodeAnimation(NodeAnimation nodeAnimation, int indent) {
        PrintIndent(indent);
        System.out.printf("node: %s num keys: t: %d  r: %d  s: %d\n", nodeAnimation.node.name,
            nodeAnimation.translationKeysCount,
            nodeAnimation.rotationKeysCount,
            nodeAnimation.scaleKeysCount);
    }

    private static void DebugPrintAnimation(Animation animation, int indent) {
        PrintIndent(indent);
        System.out.printf("animation: %s\n", animation.name);

        for (NodeAnimation nodeAnim : animation.getNodeAnimations()) {
            DebugPrintNodeAnimation(nodeAnim, indent+1);
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
            System.out.printf("Node: %s\n", node.name);
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
            System.out.printf("Skin: %s\n", skin.name);
            for (Bone bone : skin.getBones())
            {
                PrintIndent(1);
                //System.out.printf("Bone: %s\n", bone.name);
                System.out.printf("Bone: %s  node: %s\n", bone.name, bone.node==null?"null":bone.node.name);
            }
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

        /*
        SpinePointer p = new SpinePointer(spine_file);

        {
            int i = 0;
            for (String name : SPINE_GetAnimations(p)) {
                System.out.printf("Animation %d: %s\n", i++, name);
            }
        }

        {
            int i = 0;
            for (String name : SPINE_GetSkins(p)) {
                System.out.printf("Skin %d: %s\n", i++, name);
            }
        }

        Bone[] bones = SPINE_GetBones(p);
        DebugPrintBones(bones);

        SPINE_UpdateVertices(p, 0.0f);

        int count = 0;
        SpineVertex[] vertices = SPINE_GetVertexBuffer(p);

        System.out.printf("Vertices: count: %d  size: %d bytes\n", vertices.length, vertices.length>0 ? vertices.length * vertices[0].size() : 0);

        for (SpineVertex vertex : vertices) {
            if (count > 10) {
                System.out.printf(" ...\n");
                break;
            }
            System.out.printf(" vertex %d: %.4f, %.4f\n", count++, vertex.x, vertex.y);
        }

        count = 0;
        RenderObject[] ros = SPINE_GetRenderObjects(p);

        System.out.printf("Render Objects: count %d\n", ros.length);
        for (RenderObject ro : ros) {
            if (count > 10) {
                System.out.printf(" ...\n");
                break;
            }

            System.out.printf(" ro %d: fw(ccw): %b  offset: %d  count: %d  constants: %d\n", count++, ro.m_FaceWindingCCW, ro.m_VertexStart, ro.m_VertexCount, ro.m_NumConstants);

            for (int i = 0; i < ro.m_NumConstants && i < 2; ++i)
            {
                System.out.printf("    var %d: %s %.3f, %.3f, %.3f, %.3f\n", i, Long.toUnsignedString(ro.m_Constants[i].m_NameHash), ro.m_Constants[i].m_Value.x, ro.m_Constants[i].m_Value.y, ro.m_Constants[i].m_Value.z, ro.m_Constants[i].m_Value.w);
            }
        }
        */
    }
}
