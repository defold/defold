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

    // static void addToPath(String variable, String path) {
    //     String newPath = null;

    //     // Check if jna.library.path is set externally.
    //     if (System.getProperty(variable) != null) {
    //         newPath = System.getProperty(variable);
    //     }

    //     if (newPath == null) {
    //         // Set path where model_shared library is found.
    //         newPath = path;
    //     } else {
    //         // Append path where model_shared library is found.
    //         newPath += File.pathSeparator + path;
    //     }

    //     // Set the concatenated jna.library path
    //     System.setProperty(variable, newPath);
    // }

    static final String LIBRARY_NAME = "model_shared";

    // static String getPlatform() {
    //     String os_name = System.getProperty("os.name").toLowerCase();
    //     String os_arch = System.getProperty("os.arch").toLowerCase();

    //     String os = "darwin";
    //     String arch = "x86_64";

    //     if (os_name.contains("win")) {
    //         os = "win32";
    //     }
    //     else if (os_name.contains("linux")) {
    //         os = "linux";
    //     }

    //     return arch + "-" + os;
    // }

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

    // public static class ModelPointer extends PointerType {
    //     public ModelPointer() { super(); }
    //     public ModelPointer(Pointer p) { super(p); }
    //     @Override
    //     public void finalize() {
    //         SPINE_Destroy(this);
    //     }
    // }

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


    static public class TestInfo extends Structure {
        public static class ByReference extends TestInfo implements Structure.ByReference {}
        public String name;
        protected List getFieldOrder() { return Arrays.asList(new String[] {"name"});}
        public TestInfo() {}

        @SuppressWarnings("unchecked")
        public TestInfo[] castToArray(int size) {
            return (TestInfo[])super.toArray(size);
        }
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

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {
                "positions","normals","tangents","colors","weights","bones",
                "texCoords0NumComponents","texCoords0","texCoords1NumComponents","texCoords1",
                "vertexCount","material"
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
    }

    static public class Bone extends Structure {
        public static class ByReference extends Bone implements Structure.ByReference {}

        public Transform            invBindPose;
        public String               name;
        public Pointer              node;

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
    }

    static public class Node extends Structure {
        public static class ByReference extends Node implements Structure.ByReference {}

        public Transform            transform;
        public String               name;
        public Model.ByReference    model;
        public Skin.ByReference     skin;
        public Node.ByReference     parent;
        //public Node.ByReference[]     children = new Node.ByReference[];
        public Pointer              children;
        public int                  childrenCount;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {
                "transform", "name",
                "model","skin","parent","children","childrenCount"
                //"parent", "children", "childrenCount"
            });
        }
        public Node() {}

        @SuppressWarnings("unchecked")
        public Node[] castToArray(int size) {
            return (Node[])super.toArray(size);
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
    }

    static public class Scene extends Structure {
        public Pointer opaqueSceneData;
        public Pointer destroyFn;

        public TestInfo.ByReference     testInfos;
        public int                      testInfosCount;

        public Node.ByReference         nodes;
        public int                      nodesCount;

        // public Model.ByReference        models;
        // public int                      modelsCount;

        // public Skin.ByReference         skins;
        // public int                      skinsCount;

        // public Node.ByReference         rootNodes;
        // public int                      rootNodesCount;

        // public Animation.ByReference    animations;
        // public int                      animationsCount;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {
                "opaqueSceneData", "destroyFn",
                "testInfos", "testInfosCount",
                "nodes", "nodesCount",
                // "models", "modelsCount",
                // "skins", "skinsCount",
                // "rootNodes", "rootNodesCount",
                // "animations", "animationsCount",
            });
        }
        // public Scene(Pointer p) {
        //     super(p);
        // }
        public Scene() {super();}
    }


    ////////////////////////////////////////////////////////////////////////////////

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

    // private static void DebugPrintBones(Bone[] bones) {
    //     for (Bone bone : bones) {
    //         if (bone.parent == -1) {
    //             DebugPrintBone(bone, bones, 0);
    //         }
    //     }
    // }

    // static int size(Class<? extends Structure> type) {
    //   return size(type, null);
    // }

    // Used for testing functions
    // See ./com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/pipeline/test_model_importer.sh
    public static void main(String[] args) throws IOException {
        System.setProperty("java.awt.headless", "true");

        if (args.length < 1) {
            Usage();
            return;
        }

        System.out.printf("sizeof(Transform) == %d\n", Structure.newInstance(Transform.class).size());
        System.out.printf("sizeof(Mesh) == %d\n", Structure.newInstance(Mesh.class).size());
        System.out.printf("sizeof(Model) == %d\n", Structure.newInstance(Model.class).size());
        System.out.printf("sizeof(Bone) == %d\n", Structure.newInstance(Bone.class).size());
        System.out.printf("sizeof(Skin) == %d\n", Structure.newInstance(Skin.class).size());
        System.out.printf("sizeof(Node) == %d\n", Structure.newInstance(Node.class).size());
        System.out.printf("sizeof(KeyFrame) == %d\n", Structure.newInstance(KeyFrame.class).size());
        System.out.printf("sizeof(NodeAnimation) == %d\n", Structure.newInstance(NodeAnimation.class).size());
        System.out.printf("sizeof(Animation) == %d\n", Structure.newInstance(Animation.class).size());
        System.out.printf("sizeof(Scene) == %d\n", Structure.newInstance(Scene.class).size());


        String path = args[0];       // .glb

        Options options = new Options();
        Scene scene = LoadFromPath(options, path);

        // if (scene != null) {
        //     System.out.printf("Loaded %s\n", path);
        // } else {
        //     System.err.printf("Failed to load %s\n", path);
        //     return;
        // }

        //Node[] nodes = scene.nodes.getValue().toArray(scene.nodesCount);

        //Node[] nodes = scene.nodes.castToArray(scene.nodesCount);
        //System.out.printf("nodes: %d\n", (int)scene.nodes);

        System.out.printf("Num Nodes: %d\n", scene.nodesCount);
        Node[] nodes = scene.nodes.castToArray(scene.nodesCount);
        for (Node node : nodes)
        {
            System.out.printf("Node: %s\n", node.name != null ? node.name : "null");
        }

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
