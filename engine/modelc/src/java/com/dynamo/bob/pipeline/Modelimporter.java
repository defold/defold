// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

// generated, do not edit

package com.dynamo.bob.pipeline;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.lang.reflect.Method;

public class Modelimporter {
    public static final int INVALID_INDEX = 2147483647;
    public enum AlphaMode {
        ALPHA_MODE_OPAQUE(0),
        ALPHA_MODE_MASK(1),
        ALPHA_MODE_BLEND(2),
        ALPHA_MODE_MAX_ENUM(3);
        private final int value;
        private AlphaMode(int value) {
            this.value = value;
        }
        public int getValue() {
            return this.value;
        }
        static public AlphaMode fromValue(int value) throws IllegalArgumentException {
            for (AlphaMode e : AlphaMode.values()) {
                if (e.value == value)
                    return e;
            }
            throw new IllegalArgumentException(String.format("Invalid value to AlphaMode: %d", value) );
        }
    };

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
    public static class Image {
        public String name;
        public String uri;
        public String mimeType;
        public Buffer buffer;
        public int index = 0;
    };
    public static class Sampler {
        public String name;
        public int index = 0;
        public int magFilter = 0;
        public int minFilter = 0;
        public int wrapS = 0;
        public int wrapT = 0;
    };
    public static class Texture {
        public String name;
        public Image image;
        public Sampler sampler;
        public Image basisuImage;
        public int index = 0;
    };
    public static class TextureTransform {
        public float[] offset;
        public float rotation = 0.0f;
        public float[] scale;
        public int texcoord = 0;
    };
    public static class TextureView {
        public Texture texture;
        public int texcoord = 0;
        public float scale = 0.0f;
        public boolean hasTransform = false;
        public TextureTransform transform;
    };
    public static class PbrMetallicRoughness {
        public TextureView baseColorTexture;
        public TextureView metallicRoughnessTexture;
        public float[] baseColorFactor;
        public float metallicFactor = 0.0f;
        public float roughnessFactor = 0.0f;
    };
    public static class PbrSpecularGlossiness {
        public TextureView diffuseTexture;
        public TextureView specularGlossinessTexture;
        public float[] diffuseFactor;
        public float[] specularFactor;
        public float glossinessFactor = 0.0f;
    };
    public static class Clearcoat {
        public TextureView clearcoatTexture;
        public TextureView clearcoatRoughnessTexture;
        public TextureView clearcoatNormalTexture;
        public float clearcoatFactor = 0.0f;
        public float clearcoatRoughnessFactor = 0.0f;
    };
    public static class Transmission {
        public TextureView transmissionTexture;
        public float transmissionFactor = 0.0f;
    };
    public static class Ior {
        public float ior = 0.0f;
    };
    public static class Specular {
        public TextureView specularTexture;
        public TextureView specularColorTexture;
        public float[] specularColorFactor;
        public float specularFactor = 0.0f;
    };
    public static class Volume {
        public TextureView thicknessTexture;
        public float thicknessFactor = 0.0f;
        public float[] attenuationColor;
        public float attenuationDistance = 0.0f;
    };
    public static class Sheen {
        public TextureView sheenColorTexture;
        public TextureView sheenRoughnessTexture;
        public float[] sheenColorFactor;
        public float sheenRoughnessFactor = 0.0f;
    };
    public static class EmissiveStrength {
        public float emissiveStrength = 0.0f;
    };
    public static class Iridescence {
        public float iridescenceFactor = 0.0f;
        public TextureView iridescenceTexture;
        public float iridescenceIor = 0.0f;
        public float iridescenceThicknessMin = 0.0f;
        public float iridescenceThicknessMax = 0.0f;
        public TextureView iridescenceThicknessTexture;
    };
    public static class Material {
        public String name;
        public int index = 0;
        public byte isSkinned = 0;
        public PbrMetallicRoughness pbrMetallicRoughness;
        public PbrSpecularGlossiness pbrSpecularGlossiness;
        public Clearcoat clearcoat;
        public Ior ior;
        public Specular specular;
        public Sheen sheen;
        public Transmission transmission;
        public Volume volume;
        public EmissiveStrength emissiveStrength;
        public Iridescence iridescence;
        public TextureView normalTexture;
        public TextureView occlusionTexture;
        public TextureView emissiveTexture;
        public float[] emissiveFactor;
        public float alphaCutoff = 0.0f;
        public AlphaMode alphaMode = AlphaMode.ALPHA_MODE_OPAQUE;
        public boolean doubleSided = false;
        public boolean unlit = false;
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
        public byte[] buffer;
    };
    public static class Scene {
        public long opaqueSceneData;
        public Node[] nodes;
        public Model[] models;
        public Skin[] skins;
        public Node[] rootNodes;
        public Animation[] animations;
        public Material[] materials;
        public Sampler[] samplers;
        public Image[] images;
        public Texture[] textures;
        public Buffer[] buffers;
        public Material[] dynamicMaterials;
    };
    public static class Options {
        public int dummy = 0;
    };
}

