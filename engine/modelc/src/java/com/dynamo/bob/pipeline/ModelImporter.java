// generated, do not edit

package com.dynamo.bob.pipeline;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.lang.reflect.Method;

public class Modelimporter {
    public static final int INVALID_INDEX = 2147483647;
    public static class Vector3 {
        public float x = 0.0f;
        public float y = 0.0f;
        public float z = 0.0f;
    };
    public static class Quat {
        public float x = 0.0f;
        public float y = 0.0f;
        public float z = 0.0f;
        public float w = 0.0f;
    };
    public static class Transform {
        public Quat rotation;
        public Vector3 translation;
        public Vector3 scale;
    };
    public static class Aabb {
        public Vector3 min;
        public Vector3 max;
    };
    public static class Material {
        public String name;
        public int index = 0;
        public byte isSkinned = 0;
    };
    public static class Mesh {
        public String name;
        public Material material;
        public float[] positions;
        public float[] normals;
        public float[] tangents;
        public float[] colors;
        public float[] weights;
        public int[] bones;
        public int texCoords0NumComponents = 0;
        public float[] texCoords0;
        public int texCoords1NumComponents = 0;
        public float[] texCoords1;
        public Aabb aabb;
        public int[] indices;
        public int vertexCount = 0;
    };
    public static class Model {
        public String name;
        public Mesh[] meshes;
        public int index = 0;
        public Bone parentBone;
    };
    public static class Bone {
        public Transform invBindPose;
        public String name;
        public Node node;
        public Bone parent;
        public int parentIndex = 0;
        public int index = 0;
        public Bone[] children;
    };
    public static class Skin {
        public String name;
        public Bone[] bones;
        public int index = 0;
        public int[] boneRemap;
    };
    public static class Node {
        public Transform local;
        public Transform world;
        public String name;
        public Model model;
        public Skin skin;
        public Node parent;
        public Node[] children;
        public int index = 0;
        public long nameHash = 0;
    };
    public static class KeyFrame {
        public float[] value;
        public float time = 0.0f;
    };
    public static class NodeAnimation {
        public Node node;
        public KeyFrame[] translationKeys;
        public KeyFrame[] rotationKeys;
        public KeyFrame[] scaleKeys;
        public float startTime = 0.0f;
        public float endTime = 0.0f;
    };
    public static class Animation {
        public String name;
        public NodeAnimation[] nodeAnimations;
        public float duration = 0.0f;
    };
    public static class Buffer {
        public String uri;
        public long buffer;
        public int bufferSize = 0;
    };
    public static class Scene {
        public long opaqueSceneData;
        public Node[] nodes;
        public Model[] models;
        public Skin[] skins;
        public Node[] rootNodes;
        public Animation[] animations;
        public Material[] materials;
        public Buffer[] buffers;
        public Material[] dynamicMaterials;
    };
    public static class Options {
        public int dummy = 0;
    };
}

