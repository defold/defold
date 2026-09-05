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

package com.dynamo.bob.fs.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.awt.image.BufferedImage;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintStream;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Base64;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.CRC32;

import javax.imageio.ImageIO;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.ClassLoaderResourceScanner;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.AbstractResource;
import com.dynamo.bob.fs.FileSystemMountPoint;
import com.dynamo.bob.fs.FileSystemWalker;
import com.dynamo.bob.fs.GltfContainer;
import com.dynamo.bob.fs.GltfImageResource;
import com.dynamo.bob.fs.GltfMaterialResource;
import com.dynamo.bob.fs.GltfMeshResource;
import com.dynamo.bob.fs.GltfMountPoint;
import com.dynamo.bob.fs.GltfResource;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ModelImporterJni;
import com.dynamo.bob.pipeline.Modelimporter;
import com.dynamo.bob.test.util.MockFileSystem;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.google.protobuf.TextFormat;

public class GltfMountPointTest {

    private static final String GEOMETRY_BUFFER_BASE64 =
            "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA";

    private IntermittentlyUnreadableFileSystem fileSystem;
    private GltfMountPoint mountPoint;
    private byte[] png;

    @Before
    public void setUp() throws IOException {
        fileSystem = new IntermittentlyUnreadableFileSystem();
        fileSystem.setRootDirectory(".");
        fileSystem.setBuildDirectory("build/default");

        png = createPng(0xff336699);

        fileSystem.addFile("models/external.png", png);
        fileSystem.addFile("models/robot.gltf", gltf("external.png").getBytes(StandardCharsets.UTF_8));
        mountPoint = new GltfMountPoint(fileSystem);
    }

    @After
    public void tearDown() {
        fileSystem.close();
    }

    @Test
    public void testMountLookupAndWalk() throws Exception {
        assertNull(mountPoint.get("models/robot.gltf/materials/0.material"));
        assertNull(mountPoint.get("models/robot.gltf/meshes/Mesh 0"));
        fileSystem.addMountPoint(mountPoint);

        IResource material = fileSystem.get("/models/robot.gltf/materials/0.material");
        assertTrue(material instanceof GltfMaterialResource);
        assertEquals("models/robot.gltf", ((GltfResource)material).getSourceResource().getPath());

        GltfMeshResource mesh = (GltfMeshResource)fileSystem.get("models/robot.gltf/meshes/Mesh 0");
        assertNotNull(mesh);
        assertEquals("models/robot.gltf", mesh.getSourceResource().getPath());
        assertEquals(GltfResource.Kind.MESH, mesh.getKind());
        assertEquals("model_0", mesh.getName());
        assertTrue(mesh.isNameGenerated());
        assertEquals(1, mesh.getPrimitiveCount());
        assertEquals(3, mesh.getVertexCount());
        assertEquals(0, mesh.getContent().length);
        assertSetContentFails(mesh);

        assertNull(mountPoint.get("models/robot.gltf/materials/0.materialc"));
        assertNull(mountPoint.get("models/robot.gltf/materials/1.material"));
        assertNull(mountPoint.get("models/robot.gltf"));

        Collection<String> results = new ArrayList<String>();
        fileSystem.walk("models/robot.gltf", new FileSystemWalker(), results);
        assertEquals(list(
                "models/robot.gltf",
                "models/robot.gltf/materials/0.material",
                "models/robot.gltf/images/0.png",
                "models/robot.gltf/images/1.png",
                "models/robot.gltf/images/2.png",
                "models/robot.gltf/meshes/Mesh 0"), results);

        results.clear();
        fileSystem.walk("models/robot.gltf/images", new FileSystemWalker(), results);
        assertEquals(list(
                "models/robot.gltf/images/0.png",
                "models/robot.gltf/images/1.png",
                "models/robot.gltf/images/2.png"), results);

        results.clear();
        mountPoint.walk("models/robot.gltf", new FileSystemWalker() {
            @Override
            public boolean handleDirectory(String path, Collection<String> directories) {
                directories.add(path);
                return true;
            }

            @Override
            public void handleFile(String path, Collection<String> directories) {
            }
        }, results);
        assertEquals(list(
                "models/robot.gltf",
                "models/robot.gltf/materials",
                "models/robot.gltf/images",
                "models/robot.gltf/meshes"), results);

        mountPoint.unmount();
        assertNull(mountPoint.get("models/robot.gltf/images/0.png"));
    }

    @Test
    public void testFileSystemIndependentExtraction() throws Exception {
        byte[] sourceBytes = gltf("external.png").getBytes(StandardCharsets.UTF_8);
        ModelImporterJni.DataResolver resolver = new ModelImporterJni.DataResolver() {
            @Override
            public byte[] getData(String path, String uri) {
                assertEquals("nested/robot.gltf", path);
                return "external.png".equals(uri) ? png : null;
            }
        };

        GltfContainer.Extraction extraction = GltfContainer.extract(
                sourceBytes, "nested/robot.gltf", resolver);

        assertTrue(extraction.getDiagnostics().isEmpty());
        assertEquals(5, extraction.getAssets().size());
        assertEquals(1, extraction.getMeshes().size());

        GltfContainer.MeshMetadata mesh = extraction.getMeshes().get(0);
        assertSame(mesh, extraction.getAssets().get(4));
        assertEquals(GltfContainer.AssetKind.MESH, mesh.getKind());
        assertEquals("meshes/Mesh 0", mesh.getPath());
        assertEquals(0, mesh.getIndex());
        assertEquals("model_0", mesh.getName());
        assertTrue(mesh.isNameGenerated());
        assertEquals(1, mesh.getPrimitiveCount());
        assertEquals(3, mesh.getVertexCount());
        assertEquals(0, mesh.getContent().length);

        GltfContainer.Asset material = extraction.getAssets().get(0);
        assertTrue(material instanceof GltfContainer.MaterialAsset);
        assertEquals(GltfContainer.AssetKind.MATERIAL, material.getKind());
        assertEquals("materials/0.material", material.getPath());
        assertEquals("Paint", material.getName());
        GltfContainer.MaterialAsset materialAsset = (GltfContainer.MaterialAsset)material;
        assertEquals("Paint", materialAsset.getMaterialDesc().getName());
        assertEquals("images/0.png", materialAsset.getSamplerBindings()
                .get("PbrMetallicRoughness_baseColorTexture").getImagePath());

        GltfContainer.Asset externalImage = extraction.getAssets().get(1);
        assertTrue(externalImage instanceof GltfContainer.ImageAsset);
        assertEquals(GltfContainer.AssetKind.IMAGE, externalImage.getKind());
        assertEquals("images/0.png", externalImage.getPath());
        assertArrayEquals(png, externalImage.getContent());
        GltfContainer.ImageAsset image = (GltfContainer.ImageAsset)externalImage;
        assertEquals("external.png", image.getUri());
        assertEquals("image/png", image.getMimeType());
        assertEquals("external-uri", image.getSourceKind());
        assertEquals(2, image.getTextures().size());

        byte[] firstRead = externalImage.getContent();
        firstRead[0] = (byte)(firstRead[0] + 1);
        assertArrayEquals(png, externalImage.getContent());

        try {
            extraction.getAssets().clear();
            fail("Expected immutable extraction assets");
        } catch (UnsupportedOperationException expected) {
        }
        try {
            extraction.getMeshes().clear();
            fail("Expected immutable mesh metadata");
        } catch (UnsupportedOperationException expected) {
        }

        assertEquals("textures/albedo.png", GltfContainer.resolveExternalResourcePath(
                "models/robot.gltf", "../textures/albedo.png"));
        assertEquals("models/Box With Spaces.bin", GltfContainer.resolveExternalResourcePath(
                "models/robot.gltf", "Box With Spaces.bin"));
        assertExternalResourcePathFails("models/robot.gltf", "../../outside.png");
        assertExternalResourcePathFails("models/robot.gltf", "external.png?cache=1");
    }

    @Test
    public void testExternalBufferUriWithSpaces() throws Exception {
        String embeddedGeometry = "\"uri\":\"data:application/octet-stream;base64,"
                + GEOMETRY_BUFFER_BASE64 + "\",\"byteLength\":42";
        String source = gltf("external.png").replace(embeddedGeometry,
                "\"uri\":\"Box With Spaces.bin\",\"byteLength\":42");
        fileSystem.addFile("models/Box With Spaces.bin", Base64.getDecoder().decode(GEOMETRY_BUFFER_BASE64));
        fileSystem.addFile("models/spaces.gltf", source.getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        GltfContainer container = mountPoint.getContainer("models/spaces.gltf");
        assertTrue(container.getDiagnostics().isEmpty());
        assertNotNull(container.getResource("meshes/Mesh 0"));
    }

    @Test
    public void testBinaryGltfMountLookupAndWalk() throws Exception {
        fileSystem.addFile("models/robot.glb", glb("external.png"));
        fileSystem.addMountPoint(mountPoint);

        IResource material = fileSystem.get("models/robot.glb/materials/0.material");
        assertTrue(material instanceof GltfMaterialResource);
        assertEquals("models/robot.glb", ((GltfResource)material).getSourceResource().getPath());

        GltfMeshResource mesh = (GltfMeshResource)fileSystem.get("models/robot.glb/meshes/Mesh 0");
        assertNotNull(mesh);
        assertEquals("model_0", mesh.getName());
        assertTrue(mesh.isNameGenerated());

        GltfImageResource image = (GltfImageResource)fileSystem.get("models/robot.glb/images/2.png");
        assertNotNull(image);
        assertArrayEquals(png, image.getContent());
        assertEquals("buffer-view", image.getSourceKind());

        Collection<String> results = new ArrayList<String>();
        fileSystem.walk("models/robot.glb", new FileSystemWalker(), results);
        assertEquals(list(
                "models/robot.glb",
                "models/robot.glb/materials/0.material",
                "models/robot.glb/images/0.png",
                "models/robot.glb/images/1.png",
                "models/robot.glb/images/2.png",
                "models/robot.glb/meshes/Mesh 0"), results);
    }

    @Test
    public void testNamedMeshLookupAndWalkForGltfAndGlb() throws Exception {
        String source = gltf("external.png").replace(
                "\"meshes\":[{", "\"meshes\":[{\"name\":\"mymesh\",");
        fileSystem.addFile("models/named.gltf", source.getBytes(StandardCharsets.UTF_8));
        fileSystem.addFile("models/named.glb", glbFromGltf(source));
        fileSystem.addMountPoint(mountPoint);

        for (String containerPath : list("models/named.gltf", "models/named.glb")) {
            String meshPath = containerPath + "/meshes/mymesh";
            GltfMeshResource mesh = (GltfMeshResource)fileSystem.get(meshPath);
            assertNotNull(mesh);
            assertEquals(meshPath, mesh.getPath());
            assertEquals(0, mesh.getIndex());
            assertEquals("mymesh", mesh.getName());
            assertFalse(mesh.isNameGenerated());
            assertEquals(1, mesh.getPrimitiveCount());
            assertEquals(3, mesh.getVertexCount());
            assertEquals(0, mesh.getContent().length);

            Collection<String> results = new ArrayList<String>();
            fileSystem.walk(containerPath + "/meshes", new FileSystemWalker(), results);
            assertEquals(list(meshPath), results);
        }
    }

    @Test
    public void testMeshPathNaming() throws Exception {
        String longName = String.join("", Collections.nCopies(256, "a"));
        String maxCollidingName = String.join("", Collections.nCopies(255, "b"));
        String maxUniqueName = String.join("", Collections.nCopies(255, "c"));
        String source = meshMetadataGltf(
                "Cube.001",
                null,
                "",
                "bad/name",
                "CON.txt",
                "Shared",
                "shared",
                "\u00c9",
                "E\u0301",
                "Part",
                "part",
                "Part [9]",
                "trailing.",
                longName,
                " leading",
                maxCollidingName,
                maxCollidingName,
                maxUniqueName,
                "Mesh 15");
        fileSystem.addFile("models/naming.gltf", source.getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        GltfContainer container = mountPoint.getContainer("models/naming.gltf");
        assertEquals(list(
                "meshes/Cube.001",
                "meshes/Mesh 1",
                "meshes/Mesh 2",
                "meshes/Mesh 3",
                "meshes/Mesh 4",
                "meshes/Shared [5]",
                "meshes/shared [6]",
                "meshes/\u00c9 [7]",
                "meshes/E\u0301 [8]",
                "meshes/Part [9] [9]",
                "meshes/part [10]",
                "meshes/Part [9] [11]",
                "meshes/Mesh 12",
                "meshes/Mesh 13",
                "meshes/Mesh 14",
                "meshes/Mesh 15 [15]",
                "meshes/Mesh 16",
                "meshes/" + maxUniqueName,
                "meshes/Mesh 15 [18]"), meshPaths(container));

        assertEquals("model_1", container.getResource("meshes/Mesh 1").getName());
        assertEquals("", container.getResource("meshes/Mesh 2").getName());
        assertEquals("bad/name", container.getResource("meshes/Mesh 3").getName());
        assertEquals("CON.txt", container.getResource("meshes/Mesh 4").getName());
        assertEquals(longName, container.getResource("meshes/Mesh 13").getName());
        assertEquals(" leading", container.getResource("meshes/Mesh 14").getName());
        assertEquals(maxUniqueName, container.getResource("meshes/" + maxUniqueName).getName());
    }

    @Test
    public void testGeneratedPbrMaterial() throws Exception {
        fileSystem.addMountPoint(mountPoint);
        GltfMaterialResource resource = (GltfMaterialResource)fileSystem.get("models/robot.gltf/materials/0.material");

        MaterialDesc.Builder builder = MaterialDesc.newBuilder();
        TextFormat.merge(new String(resource.getContent(), StandardCharsets.UTF_8), builder);
        MaterialDesc material = builder.build();

        assertEquals("Paint", material.getName());
        assertEquals("/defold-pbr/shaders/pbr.vp", material.getVertexProgram());
        assertEquals("/defold-pbr/shaders/pbr.fp", material.getFragmentProgram());
        assertEquals(MaterialDesc.VertexSpace.VERTEX_SPACE_LOCAL, material.getVertexSpace());
        assertEquals(list("model"), material.getTagsList());
        assertEquals(4, material.getVertexConstantsCount());

        Map<String, MaterialDesc.ConstantType> constants = new HashMap<String, MaterialDesc.ConstantType>();
        for (MaterialDesc.Constant constant : material.getVertexConstantsList()) {
            constants.put(constant.getName(), constant.getType());
        }
        assertEquals(4, constants.size());
        assertEquals(MaterialDesc.ConstantType.CONSTANT_TYPE_WORLDVIEW, constants.get("mtx_worldview"));
        assertEquals(MaterialDesc.ConstantType.CONSTANT_TYPE_VIEW, constants.get("mtx_view"));
        assertEquals(MaterialDesc.ConstantType.CONSTANT_TYPE_PROJECTION, constants.get("mtx_proj"));
        assertEquals(MaterialDesc.ConstantType.CONSTANT_TYPE_NORMAL, constants.get("mtx_normal"));

        assertEquals(5, material.getSamplersCount());
        assertEquals("PbrMetallicRoughness_baseColorTexture", material.getSamplers(0).getName());
        assertEquals(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE, material.getSamplers(0).getWrapU());
        assertEquals(MaterialDesc.WrapMode.WRAP_MODE_MIRRORED_REPEAT, material.getSamplers(0).getWrapV());
        assertEquals(MaterialDesc.FilterModeMin.FILTER_MODE_MIN_NEAREST_MIPMAP_NEAREST,
                material.getSamplers(0).getFilterMin());
        assertEquals(MaterialDesc.FilterModeMag.FILTER_MODE_MAG_NEAREST, material.getSamplers(0).getFilterMag());
        assertEquals("PbrMetallicRoughness_metallicRoughnessTexture", material.getSamplers(1).getName());
        assertEquals("PbrMaterial_normalTexture", material.getSamplers(2).getName());
        assertEquals("PbrMaterial_occlusionTexture", material.getSamplers(3).getName());
        assertEquals("PbrMaterial_emissiveTexture", material.getSamplers(4).getName());

        Modelimporter.Material sourceMaterial = resource.getSourceMaterial();
        assertSame(sourceMaterial, resource.getSourceMaterial());
        assertEquals(0, sourceMaterial.index);
        assertEquals("Paint", sourceMaterial.name);
        assertArrayEquals(new float[] { 0.25f, 0.5f, 0.75f, 1.0f },
                sourceMaterial.pbrMetallicRoughness.baseColorFactor, 0.0f);
        assertEquals(0.6f, sourceMaterial.pbrMetallicRoughness.metallicFactor, 0.0f);
        assertEquals(0.4f, sourceMaterial.pbrMetallicRoughness.roughnessFactor, 0.0f);
        assertEquals(0.75f, sourceMaterial.normalTexture.scale, 0.0f);
        assertEquals(0.5f, sourceMaterial.occlusionTexture.scale, 0.0f);
        assertArrayEquals(new float[] { 0.1f, 0.2f, 0.3f }, sourceMaterial.emissiveFactor, 0.0f);
        assertEquals(0, sourceMaterial.pbrMetallicRoughness.baseColorTexture.texture.index);
        assertEquals(9984,
                sourceMaterial.pbrMetallicRoughness.baseColorTexture.texture.sampler.minFilter);
        assertEquals(Modelimporter.AlphaMode.ALPHA_MODE_BLEND, sourceMaterial.alphaMode);
        assertTrue(sourceMaterial.doubleSided);
    }

    @Test
    public void testMaterialSamplerBindingsFollowNativeTextureGraph() throws Exception {
        fileSystem.addMountPoint(mountPoint);
        GltfMaterialResource resource = (GltfMaterialResource)fileSystem.get(
                "models/robot.gltf/materials/0.material");

        Map<String, GltfContainer.SamplerBinding> bindings = resource.getSamplerBindings();
        assertEquals(5, bindings.size());
        assertSamplerBinding(bindings, "PbrMetallicRoughness_baseColorTexture",
                0, 0, 0, "images/0.png");
        assertSamplerBinding(bindings, "PbrMetallicRoughness_metallicRoughnessTexture",
                0, 1, 1, "images/1.png");
        assertSamplerBinding(bindings, "PbrMaterial_normalTexture",
                0, 1, 1, "images/1.png");
        assertSamplerBinding(bindings, "PbrMaterial_occlusionTexture",
                0, 2, 2, "images/2.png");
        assertSamplerBinding(bindings, "PbrMaterial_emissiveTexture",
                0, 3, 0, "images/0.png");

        assertEquals(bindings.get("PbrMetallicRoughness_baseColorTexture").getImagePath(),
                bindings.get("PbrMaterial_emissiveTexture").getImagePath());
        assertEquals(bindings.get("PbrMetallicRoughness_metallicRoughnessTexture").getImagePath(),
                bindings.get("PbrMaterial_normalTexture").getImagePath());
        try {
            bindings.clear();
            fail("Expected immutable sampler bindings");
        } catch (UnsupportedOperationException expected) {
        }
    }

    @Test
    public void testTextureMetadataUsesTheMaterialImageSelection() throws Exception {
        String source = gltf("external.png")
                .replace("\"asset\":{\"version\":\"2.0\"},",
                         "\"asset\":{\"version\":\"2.0\"},\"extensionsUsed\":[\"KHR_texture_basisu\"],")
                .replace("\"name\":\"ExternalTexture\",\"sampler\":0,\"source\":0}",
                         "\"name\":\"ExternalTexture\",\"sampler\":0,\"source\":0,"
                                 + "\"extensions\":{\"KHR_texture_basisu\":{\"source\":1}}}")
                .replace("data:image/png;base64," + Base64.getEncoder().encodeToString(png), "preferred.png");
        fileSystem.addFile("models/robot.gltf", source.getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        // PNG stand-ins exercise image selection independently of GPU texture decoding.
        for (boolean preferredAvailable : new boolean[] { false, true, false }) {
            if (preferredAvailable) {
                fileSystem.addFile("models/preferred.png", png);
            } else if (fileSystem.get("models/preferred.png").exists()) {
                fileSystem.get("models/preferred.png").remove();
            }

            GltfContainer container = mountPoint.getContainer("models/robot.gltf");
            GltfMaterialResource material = (GltfMaterialResource)container.getResource("materials/0.material");
            int selectedImageIndex = preferredAvailable ? 1 : 0;
            assertSamplerBinding(material.getSamplerBindings(), "PbrMetallicRoughness_baseColorTexture",
                    0, 0, selectedImageIndex, "images/" + selectedImageIndex + ".png");

            Map<Integer, Integer> imageIndexByTextureIndex = new HashMap<Integer, Integer>();
            for (GltfResource resource : container.getResources()) {
                if (resource instanceof GltfImageResource) {
                    GltfImageResource image = (GltfImageResource)resource;
                    for (GltfContainer.TextureMetadata texture : image.getTextures()) {
                        assertNull(imageIndexByTextureIndex.put(texture.getIndex(), image.getIndex()));
                        if (texture.getIndex() == 0) {
                            assertEquals(preferredAvailable, texture.isBasisu());
                        }
                    }
                }
            }
            assertEquals(Integer.valueOf(selectedImageIndex), imageIndexByTextureIndex.get(0));
            assertNotNull(container.getResource("images/0.png"));
        }
    }

    @Test
    public void testGeneratedPbrMaterialOnlyEmitsReferencedSamplers() throws Exception {
        String baseColorOnly = withoutMaterialTextureSlots(gltf("external.png"), true)
                .replace("\"samplers\":[{\"name\":\"NearestSampler\",\"wrapS\":33071,\"wrapT\":33648,"
                                + "\"minFilter\":9984,\"magFilter\":9728}]",
                         "\"samplers\":[{\"name\":\"LinearSampler\",\"wrapS\":10497,\"wrapT\":10497,"
                                + "\"minFilter\":9987,\"magFilter\":9729}]");
        String untextured = withoutMaterialTextureSlots(gltf("external.png"), false);
        fileSystem.addFile("models/base-color-only.gltf", baseColorOnly.getBytes(StandardCharsets.UTF_8));
        fileSystem.addFile("models/untextured.gltf", untextured.getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        GltfMaterialResource baseColorResource = (GltfMaterialResource)fileSystem.get(
                "models/base-color-only.gltf/materials/0.material");
        MaterialDesc baseColorMaterial = baseColorResource.getMaterialDesc();
        assertEquals(1, baseColorMaterial.getSamplersCount());
        assertEquals("PbrMetallicRoughness_baseColorTexture", baseColorMaterial.getSamplers(0).getName());
        assertEquals(MaterialDesc.WrapMode.WRAP_MODE_REPEAT,
                baseColorMaterial.getSamplers(0).getWrapU());
        assertEquals(MaterialDesc.WrapMode.WRAP_MODE_REPEAT,
                baseColorMaterial.getSamplers(0).getWrapV());
        assertEquals(MaterialDesc.FilterModeMin.FILTER_MODE_MIN_LINEAR_MIPMAP_LINEAR,
                baseColorMaterial.getSamplers(0).getFilterMin());
        assertEquals(MaterialDesc.FilterModeMag.FILTER_MODE_MAG_LINEAR,
                baseColorMaterial.getSamplers(0).getFilterMag());
        assertEquals(1, baseColorResource.getSamplerBindings().size());
        assertSamplerBinding(baseColorResource.getSamplerBindings(),
                "PbrMetallicRoughness_baseColorTexture", 0, 0, 0, "images/0.png");
        assertNotNull(fileSystem.get("models/base-color-only.gltf/images/1.png"));
        assertNotNull(fileSystem.get("models/base-color-only.gltf/images/2.png"));

        Modelimporter.Material sourceMaterial = baseColorResource.getSourceMaterial();
        assertArrayEquals(new float[] { 0.25f, 0.5f, 0.75f, 1.0f },
                sourceMaterial.pbrMetallicRoughness.baseColorFactor, 0.0f);
        assertEquals(0.6f, sourceMaterial.pbrMetallicRoughness.metallicFactor, 0.0f);
        assertEquals(0.4f, sourceMaterial.pbrMetallicRoughness.roughnessFactor, 0.0f);
        assertArrayEquals(new float[] { 0.1f, 0.2f, 0.3f }, sourceMaterial.emissiveFactor, 0.0f);
        assertEquals(Modelimporter.AlphaMode.ALPHA_MODE_BLEND, sourceMaterial.alphaMode);
        assertTrue(sourceMaterial.doubleSided);

        GltfMaterialResource untexturedResource = (GltfMaterialResource)fileSystem.get(
                "models/untextured.gltf/materials/0.material");
        MaterialDesc untexturedMaterial = untexturedResource.getMaterialDesc();
        assertEquals(0, untexturedMaterial.getSamplersCount());
        assertEquals("/defold-pbr/shaders/pbr.vp", untexturedMaterial.getVertexProgram());
        assertEquals("/defold-pbr/shaders/pbr.fp", untexturedMaterial.getFragmentProgram());
        assertArrayEquals(new float[] { 0.25f, 0.5f, 0.75f, 1.0f },
                untexturedResource.getSourceMaterial().pbrMetallicRoughness.baseColorFactor, 0.0f);
    }

    @Test
    public void testImageBytesAndTextureMetadata() throws Exception {
        fileSystem.addMountPoint(mountPoint);

        GltfImageResource external = imageResource(0);
        assertArrayEquals(png, external.getContent());
        assertEquals("external-uri", external.getSourceKind());
        assertEquals("external.png", external.getUri());
        assertEquals("image/png", external.getMimeType());
        assertEquals(1, external.getWidth());
        assertEquals(1, external.getHeight());
        assertEquals(2, external.getTextures().size());
        assertEquals(0, external.getTextures().get(0).getIndex());
        assertEquals("ExternalTexture", external.getTextures().get(0).getName());
        assertEquals(3, external.getTextures().get(1).getIndex());

        GltfImageResource dataUri = imageResource(1);
        assertArrayEquals(png, dataUri.getContent());
        assertEquals("data-uri", dataUri.getSourceKind());
        assertEquals("image/png", dataUri.getMimeType());
        assertEquals(1, dataUri.getTextures().size());
        assertEquals(1, dataUri.getTextures().get(0).getIndex());

        GltfImageResource bufferView = imageResource(2);
        assertArrayEquals(png, bufferView.getContent());
        assertEquals("buffer-view", bufferView.getSourceKind());
        assertNull(bufferView.getUri());
        assertEquals(1, bufferView.getTextures().size());
        assertEquals(2, bufferView.getTextures().get(0).getIndex());
    }

    @Test
    public void testVirtualResourcesAreReadOnlyAndProduceNormalOutputs() throws Exception {
        fileSystem.addMountPoint(mountPoint);
        GltfResource resource = (GltfResource)fileSystem.get("models/robot.gltf/materials/0.material");

        byte[] firstRead = resource.getContent();
        byte original = firstRead[0];
        firstRead[0] = (byte)(firstRead[0] + 1);
        assertEquals(original, resource.getContent()[0]);

        assertSetContentFails(resource);
        assertAppendContentFails(resource);
        assertSetStreamFails(resource);
        try {
            resource.remove();
            fail("Expected a read-only resource failure");
        } catch (UnsupportedOperationException expected) {
        }

        IResource output = resource.changeExt(".materialc");
        assertEquals("build/default/models/robot.gltf/materials/0.materialc", output.getPath());
        assertTrue(output.isOutput());
        assertFalse(output.exists());
        assertTrue(mountPoint.getErrors().isEmpty());
    }

    @Test
    public void testBadImageDoesNotHideOtherVirtualResources() throws Exception {
        fileSystem.addFile("models/missing-image.gltf", gltf("missing.png").getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        GltfContainer container = mountPoint.getContainer("models/missing-image.gltf");
        GltfMaterialResource material = (GltfMaterialResource)container.getResource("materials/0.material");
        assertNotNull(material);
        assertNull(container.getResource("images/0.png"));
        assertNotNull(container.getResource("images/1.png"));
        assertNotNull(container.getResource("images/2.png"));
        assertEquals(1, container.getDiagnostics().size());
        assertTrue(container.getDiagnostics().get(0).contains("missing.png"));
        assertFalse(material.getSamplerBindings().containsKey(
                "PbrMetallicRoughness_baseColorTexture"));
        assertFalse(material.getSamplerBindings().containsKey("PbrMaterial_emissiveTexture"));
        assertEquals(3, material.getSamplerBindings().size());

        fileSystem.addFile("models/missing.png", png);
        GltfContainer refreshed = mountPoint.getContainer("models/missing-image.gltf");
        assertNotSame(container, refreshed);
        assertNotNull(refreshed.getResource("images/0.png"));
        assertTrue(refreshed.getDiagnostics().isEmpty());
        assertEquals(5, ((GltfMaterialResource)refreshed.getResource("materials/0.material"))
                .getSamplerBindings().size());
    }

    @Test
    public void testUnsupportedImageProducesDiagnostic() throws Exception {
        String unsupported = gltf("external.png")
                .replace("\"name\":\"ExternalImage\",\"uri\":\"external.png\",\"mimeType\":\"image/png\"",
                         "\"name\":\"ExternalImage\",\"uri\":\"external.png\",\"mimeType\":\"image/webp\"");
        fileSystem.addFile("models/unsupported.gltf", unsupported.getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        GltfContainer container = mountPoint.getContainer("models/unsupported.gltf");
        assertNull(container.getResource("images/0.webp"));
        assertNull(container.getResource("images/0.png"));
        assertEquals(1, container.getDiagnostics().size());
        assertTrue(container.getDiagnostics().get(0).contains("unsupported image MIME type 'image/webp'"));
    }

    @Test
    public void testJsonEscapedSlashInImageMimeType() throws Exception {
        String source = gltf("external.png").replace(
                "\"mimeType\":\"image/png\"", "\"mimeType\":\"image\\/png\"");
        fileSystem.addFile("models/escaped-mime.gltf", source.getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        GltfContainer container = mountPoint.getContainer("models/escaped-mime.gltf");
        assertTrue(container.getDiagnostics().isEmpty());
        assertNotNull(container.getResource("images/0.png"));
        assertNotNull(container.getResource("images/2.png"));
    }

    @Test
    public void testJpegImageUsesJpgVirtualExtension() throws Exception {
        byte[] jpeg = createJpeg(0xff8844);
        fileSystem.addFile("models/external.jpg", jpeg);
        String jpegSource = gltf("external.jpg")
                .replace("\"name\":\"ExternalImage\",\"uri\":\"external.jpg\",\"mimeType\":\"image/png\"",
                         "\"name\":\"ExternalImage\",\"uri\":\"external.jpg\",\"mimeType\":\"image/jpeg\"");
        fileSystem.addFile("models/jpeg.gltf", jpegSource.getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        GltfImageResource image = (GltfImageResource)mountPoint.get("models/jpeg.gltf/images/0.jpg");
        assertNotNull(image);
        assertArrayEquals(jpeg, image.getContent());
        assertEquals("image/jpeg", image.getMimeType());
        GltfMaterialResource material = (GltfMaterialResource)mountPoint.get(
                "models/jpeg.gltf/materials/0.material");
        assertEquals("images/0.jpg", material.getSamplerBindings()
                .get("PbrMetallicRoughness_baseColorTexture").getImagePath());
    }

    @Test
    public void testExternalImageMimeTypeIsInferredFromBytes() throws Exception {
        fileSystem.addFile("models/external-image", png);
        String source = gltf("external-image")
                .replace("\"name\":\"ExternalImage\",\"uri\":\"external-image\",\"mimeType\":\"image/png\"",
                         "\"name\":\"ExternalImage\",\"uri\":\"external-image\"");
        fileSystem.addFile("models/inferred-mime.gltf", source.getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        GltfImageResource image = (GltfImageResource)mountPoint.get("models/inferred-mime.gltf/images/0.png");
        assertNotNull(image);
        assertArrayEquals(png, image.getContent());
        assertEquals("image/png", image.getMimeType());
    }

    @Test
    public void testPercentEncodedImageDataUri() throws Exception {
        String payload = Base64.getEncoder().encodeToString(png);
        String encodedPayload = payload.replace("=", "%3D");
        String source = gltf("external.png")
                .replace("data:image/png;base64," + payload, "data:image/png;base64," + encodedPayload);
        fileSystem.addFile("models/percent-data.gltf", source.getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        GltfImageResource image = (GltfImageResource)mountPoint.get("models/percent-data.gltf/images/1.png");
        assertNotNull(image);
        assertArrayEquals(png, image.getContent());
        assertEquals("image/png", image.getMimeType());
    }

    @Test
    public void testWalkTracksAddedRemovedAndRecreatedContainers() throws Exception {
        fileSystem.addMountPoint(mountPoint);

        fileSystem.addFile("models/live.gltf", gltf("external.png").getBytes(StandardCharsets.UTF_8));
        Collection<String> results = new ArrayList<String>();
        mountPoint.walk("", new FileSystemWalker(), results);
        assertTrue(results.contains("models/live.gltf/materials/0.material"));

        GltfMaterialResource first = (GltfMaterialResource)mountPoint.get("models/live.gltf/materials/0.material");
        assertEquals("Paint", first.getMaterialDesc().getName());

        fileSystem.get("models/live.gltf").remove();
        assertNull(mountPoint.get("models/live.gltf/materials/0.material"));
        results.clear();
        mountPoint.walk("", new FileSystemWalker(), results);
        for (String result : results) {
            assertFalse(result.startsWith("models/live.gltf/"));
        }
        assertFalse(mountPoint.getErrors().containsKey("models/live.gltf"));

        String recreated = gltf("external.png").replace("\"name\":\"Paint\"", "\"name\":\"Chrome\"");
        fileSystem.addFile("models/live.gltf", recreated.getBytes(StandardCharsets.UTF_8));
        results.clear();
        mountPoint.walk("", new FileSystemWalker(), results);
        assertTrue(results.contains("models/live.gltf/materials/0.material"));

        GltfMaterialResource second = (GltfMaterialResource)mountPoint.get("models/live.gltf/materials/0.material");
        assertNotSame(first, second);
        assertEquals("Paint", first.getMaterialDesc().getName());
        assertEquals("Chrome", second.getMaterialDesc().getName());
    }

    @Test
    public void testDebugOutputRedactsDataUriPayload() throws Exception {
        fileSystem.addMountPoint(mountPoint);
        GltfImageResource image = imageResource(1);
        ByteArrayOutputStream captured = new ByteArrayOutputStream();
        String payload = Base64.getEncoder().encodeToString(png);
        Method printResource = GltfResource.class.getDeclaredMethod(
                "printResource", GltfResource.class, PrintStream.class);
        printResource.setAccessible(true);
        printResource.invoke(null, image, new PrintStream(captured, true, StandardCharsets.UTF_8.name()));

        String output = captured.toString(StandardCharsets.UTF_8.name());
        assertTrue(output.contains("source: data-uri"));
        assertTrue(output.contains("mime_type: image/png"));
        assertTrue(output.contains("texture[1]: name=DataTexture sampler=0"));
        assertTrue(output.contains("uri: data:<redacted>,<"));
        assertTrue(output.contains("encoded characters>"));
        assertFalse(output.contains(payload));
        assertFalse(output.contains(payload.substring(0, Math.min(payload.length(), 16))));
    }

    @Test
    public void testMalformedContainerIsIsolated() throws Exception {
        fileSystem.addFile("models/broken.gltf", "{".getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        assertNull(mountPoint.get("models/broken.gltf/materials/0.material"));
        assertTrue(mountPoint.getErrors().containsKey("models/broken.gltf"));
        assertNotNull(mountPoint.get("models/robot.gltf/materials/0.material"));
    }

    @Test
    public void testProjectMountInstallsGltfMountPoint() throws Exception {
        Project project = new Project(fileSystem);
        try {
            project.mount(new ClassLoaderResourceScanner(), Collections.emptyList());

            assertTrue(project.getResource("/models/robot.gltf/materials/0.material") instanceof GltfMaterialResource);
            Collection<String> paths = new ArrayList<String>();
            project.findResourcePaths("", paths);
            assertTrue(paths.contains("models/robot.gltf/images/0.png"));
            assertTrue(paths.contains("models/robot.gltf/materials/0.material"));
            assertTrue(paths.contains("models/robot.gltf/meshes/Mesh 0"));

            fileSystem.addFile("models/project-added.gltf", gltf("external.png").getBytes(StandardCharsets.UTF_8));
            project.cleanupResourcePathsCache();
            paths.clear();
            project.findResourcePaths("", paths);
            assertTrue(paths.contains("models/project-added.gltf/materials/0.material"));

            fileSystem.get("models/project-added.gltf").remove();
            project.cleanupResourcePathsCache();
            paths.clear();
            project.findResourcePaths("", paths);
            assertFalse(paths.contains("models/project-added.gltf/materials/0.material"));
        } finally {
            project.dispose();
        }
    }

    @Test
    public void testEarlierConcreteMountTakesPrecedence() throws Exception {
        MockFileSystem dependency = new MockFileSystem();
        dependency.setBuildDirectory("build/default");
        byte[] concreteContent = "concrete material".getBytes(StandardCharsets.UTF_8);
        dependency.addFile("models/robot.gltf/materials/0.material", concreteContent);

        fileSystem.addMountPoint(new FileSystemMountPoint(fileSystem, dependency));
        fileSystem.addMountPoint(mountPoint);

        IResource resource = fileSystem.get("models/robot.gltf/materials/0.material");
        assertFalse(resource instanceof GltfResource);
        assertArrayEquals(concreteContent, resource.getContent());
        assertNotNull(mountPoint.get("models/robot.gltf/materials/0.material"));
    }

    @Test
    public void testExternalBufferViewImageBytes() throws Exception {
        fileSystem.addFile("models/image.bin", png);
        String externalBuffer = withExternalImageBuffer(gltf("external.png"), png.length);
        fileSystem.addFile("models/external-buffer.gltf", externalBuffer.getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        GltfImageResource image = (GltfImageResource)mountPoint.get(
                "models/external-buffer.gltf/images/2.png");
        assertNotNull(image);
        assertArrayEquals(png, image.getContent());
        assertEquals("buffer-view", image.getSourceKind());
    }

    @Test
    public void testExtractionSkipsExternalGeometryWhenResolvingImages() throws Exception {
        String source = withExternalImageBuffer(gltf("external.png"), png.length)
                .replace("data:application/octet-stream;base64," + GEOMETRY_BUFFER_BASE64, "geometry.bin");
        byte[] sourceBytes = source.getBytes(StandardCharsets.UTF_8);

        for (byte[] geometry : new byte[][] {Base64.getDecoder().decode(GEOMETRY_BUFFER_BASE64), new byte[1], null}) {
            List<String> resolvedUris = new ArrayList<>();
            GltfContainer.Extraction extraction = GltfContainer.extract(
                    sourceBytes, "models/robot.gltf", (path, uri) -> {
                        resolvedUris.add(uri);
                        switch (uri) {
                            case "image.bin":
                            case "external.png":
                                return png;
                            case "geometry.bin":
                                return geometry;
                            default:
                                return null;
                        }
                    });

            assertEquals(list("image.bin", "external.png"), resolvedUris);
            assertTrue(extraction.getDiagnostics().isEmpty());
            assertEquals(5, extraction.getAssets().size());
            assertArrayEquals(png, extraction.getAssets().get(3).getContent());
            assertEquals(1, extraction.getMeshes().size());
            assertEquals(3, extraction.getMeshes().get(0).getVertexCount());
        }
    }

    @Test
    public void testImageDimensionsAreReadWithoutDecodingRaster() throws Exception {
        byte[] oversizedHeader = png.clone();
        writeIntBigEndian(oversizedHeader, 16, 100000);
        writeIntBigEndian(oversizedHeader, 20, 100000);
        CRC32 crc = new CRC32();
        crc.update(oversizedHeader, 12, 17);
        writeIntBigEndian(oversizedHeader, 29, (int)crc.getValue());

        fileSystem.addFile("models/oversized-header.png", oversizedHeader);
        fileSystem.addFile("models/oversized-image.gltf",
                gltf("oversized-header.png").getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        GltfImageResource image = (GltfImageResource)mountPoint.get("models/oversized-image.gltf/images/0.png");
        assertNotNull(image);
        assertEquals(100000, image.getWidth());
        assertEquals(100000, image.getHeight());
        assertArrayEquals(oversizedHeader, image.getContent());
    }

    @Test
    public void testCyclicVirtualImageReferencesAreIsolated() throws Exception {
        fileSystem.addFile("models/self.gltf", gltf("self.gltf/images/0.png").getBytes(StandardCharsets.UTF_8));
        fileSystem.addFile("models/a.gltf", gltf("b.gltf/images/0.png").getBytes(StandardCharsets.UTF_8));
        fileSystem.addFile("models/b.gltf", gltf("a.gltf/images/0.png").getBytes(StandardCharsets.UTF_8));
        fileSystem.addMountPoint(mountPoint);

        for (String path : list("models/self.gltf", "models/a.gltf", "models/b.gltf")) {
            GltfContainer container = mountPoint.getContainer(path);
            assertNotNull(container.getResource("materials/0.material"));
            assertNull(container.getResource("images/0.png"));
            assertNotNull(container.getResource("images/1.png"));
            assertEquals(1, container.getDiagnostics().size());
        }
    }

    @Test
    public void testContainerCacheTracksSourceAndExternalImages() throws Exception {
        fileSystem.addMountPoint(mountPoint);

        GltfContainer first = mountPoint.getContainer("models/robot.gltf");
        assertSame(first, mountPoint.getContainer("models/robot.gltf"));

        byte[] updatedPng = createPng(0xffcc8844);
        fileSystem.addFile("models/external.png", updatedPng);
        GltfContainer imageUpdated = mountPoint.getContainer("models/robot.gltf");
        assertNotSame(first, imageUpdated);
        assertArrayEquals(updatedPng, imageUpdated.getResource("images/0.png").getContent());

        fileSystem.get("models/external.png").remove();
        GltfContainer imageRemoved = mountPoint.getContainer("models/robot.gltf");
        assertNotSame(imageUpdated, imageRemoved);
        assertNull(imageRemoved.getResource("images/0.png"));
        assertNotNull(imageRemoved.getResource("images/1.png"));
        assertEquals(1, imageRemoved.getDiagnostics().size());

        fileSystem.addFile("models/external.png", updatedPng);
        GltfContainer imageRestored = mountPoint.getContainer("models/robot.gltf");
        assertNotSame(imageRemoved, imageRestored);
        assertNotNull(imageRestored.getResource("images/0.png"));

        String updatedGltf = gltf("external.png").replace("\"name\":\"Paint\"", "\"name\":\"Chrome\"");
        fileSystem.addFile("models/robot.gltf", updatedGltf.getBytes(StandardCharsets.UTF_8));
        GltfContainer sourceUpdated = mountPoint.getContainer("models/robot.gltf");
        assertNotSame(imageRestored, sourceUpdated);
        GltfMaterialResource material = (GltfMaterialResource)sourceUpdated.getResource("materials/0.material");
        assertEquals("Chrome", material.getMaterialDesc().getName());
    }

    @Test
    public void testContainerCacheRecoversAfterExternalImageReadFailure() throws Exception {
        fileSystem.setExternalImageReadable(false);
        fileSystem.addMountPoint(mountPoint);

        GltfContainer unreadable = mountPoint.getContainer("models/robot.gltf");
        assertNull(unreadable.getResource("images/0.png"));
        assertNotNull(unreadable.getResource("images/1.png"));
        assertEquals(1, unreadable.getDiagnostics().size());

        fileSystem.setExternalImageReadable(true);
        GltfContainer recovered = mountPoint.getContainer("models/robot.gltf");
        assertNotSame(unreadable, recovered);
        assertArrayEquals(png, recovered.getResource("images/0.png").getContent());
    }

    private GltfImageResource imageResource(int index) {
        IResource resource = fileSystem.get(String.format("models/robot.gltf/images/%d.png", index));
        assertTrue(resource instanceof GltfImageResource);
        return (GltfImageResource)resource;
    }

    private static List<String> meshPaths(GltfContainer container) {
        List<String> result = new ArrayList<String>();
        for (GltfResource resource : container.getResources()) {
            if (resource.getKind() == GltfResource.Kind.MESH) {
                result.add(resource.getPath().substring(container.getSourceResource().getPath().length() + 1));
            }
        }
        return result;
    }

    private static final class IntermittentlyUnreadableFileSystem extends MockFileSystem {
        private boolean externalImageReadable = true;

        void setExternalImageReadable(boolean externalImageReadable) {
            this.externalImageReadable = externalImageReadable;
        }

        @Override
        public IResource get(String path) {
            IResource resource = super.get(path);
            if (!externalImageReadable && "models/external.png".equals(resource.getPath())) {
                return new UnreadableResource(this, resource);
            }
            return resource;
        }
    }

    private static final class UnreadableResource extends AbstractResource<MockFileSystem> {
        private final IResource delegate;

        UnreadableResource(MockFileSystem fileSystem, IResource delegate) {
            super(fileSystem, delegate.getPath());
            this.delegate = delegate;
        }

        @Override
        public byte[] getContent() throws IOException {
            throw new IOException("temporarily unreadable");
        }

        @Override
        public void setContent(byte[] content) throws IOException {
            delegate.setContent(content);
        }

        @Override
        public void appendContent(byte[] content) throws IOException {
            delegate.appendContent(content);
        }

        @Override
        public boolean exists() {
            return delegate.exists();
        }

        @Override
        public boolean isFile() {
            return delegate.isFile();
        }

        @Override
        public void remove() {
            delegate.remove();
        }

        @Override
        public void setContent(InputStream stream) throws IOException {
            delegate.setContent(stream);
        }

        @Override
        public long getLastModified() {
            return delegate.getLastModified();
        }
    }

    private static List<String> list(String... values) {
        List<String> result = new ArrayList<String>();
        for (String value : values) {
            result.add(value);
        }
        return result;
    }

    private static void assertSamplerBinding(
            Map<String, GltfContainer.SamplerBinding> bindings, String samplerName,
            int materialIndex, int textureIndex, int imageIndex, String imagePath) {
        GltfContainer.SamplerBinding binding = bindings.get(samplerName);
        assertNotNull(binding);
        assertEquals(samplerName, binding.getSamplerName());
        assertEquals(materialIndex, binding.getMaterialIndex());
        assertEquals(textureIndex, binding.getTextureIndex());
        assertEquals(imageIndex, binding.getImageIndex());
        assertEquals(imagePath, binding.getImagePath());
    }

    private static byte[] createPng(int color) throws IOException {
        BufferedImage image = new BufferedImage(1, 1, BufferedImage.TYPE_INT_ARGB);
        image.setRGB(0, 0, color);
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        assertTrue(ImageIO.write(image, "png", output));
        return output.toByteArray();
    }

    private static byte[] createJpeg(int color) throws IOException {
        BufferedImage image = new BufferedImage(1, 1, BufferedImage.TYPE_INT_RGB);
        image.setRGB(0, 0, color);
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        assertTrue(ImageIO.write(image, "jpeg", output));
        return output.toByteArray();
    }

    private static void writeIntBigEndian(byte[] bytes, int offset, int value) {
        bytes[offset] = (byte)(value >>> 24);
        bytes[offset + 1] = (byte)(value >>> 16);
        bytes[offset + 2] = (byte)(value >>> 8);
        bytes[offset + 3] = (byte)value;
    }

    private String withExternalImageBuffer(String source, int declaredLength) {
        String embedded = "\"uri\":\"data:application/octet-stream;base64,"
                + Base64.getEncoder().encodeToString(png) + "\",\"byteLength\":" + png.length;
        return source.replace(embedded, "\"uri\":\"image.bin\",\"byteLength\":" + declaredLength);
    }

    private static String withoutMaterialTextureSlots(String source, boolean keepBaseColorTexture) {
        String result = source
                .replace(",\"metallicRoughnessTexture\":{\"index\":1}", "")
                .replace("\"normalTexture\":{\"index\":1,\"scale\":0.75},", "")
                .replace("\"occlusionTexture\":{\"index\":2,\"strength\":0.5},", "")
                .replace("\"emissiveTexture\":{\"index\":3},", "");
        return keepBaseColorTexture
                ? result
                : result.replace(",\"baseColorTexture\":{\"index\":0}", "");
    }

    private static void assertSetContentFails(IResource resource) throws Exception {
        try {
            resource.setContent(new byte[] { 1 });
            fail("Expected a read-only resource failure");
        } catch (IOException expected) {
        }
    }

    private static void assertExternalResourcePathFails(String sourcePath, String uri) {
        try {
            GltfContainer.resolveExternalResourcePath(sourcePath, uri);
            fail("Expected unsafe external resource path to fail: " + uri);
        } catch (IOException expected) {
        }
    }

    private static void assertAppendContentFails(IResource resource) throws Exception {
        try {
            resource.appendContent(new byte[] { 1 });
            fail("Expected a read-only resource failure");
        } catch (IOException expected) {
        }
    }

    private static void assertSetStreamFails(IResource resource) throws Exception {
        try {
            resource.setContent(new java.io.ByteArrayInputStream(new byte[] { 1 }));
            fail("Expected a read-only resource failure");
        } catch (IOException expected) {
        }
    }

    private String gltf(String externalImageUri) {
        String pngBase64 = Base64.getEncoder().encodeToString(png);
        return "{"
                + "\"asset\":{\"version\":\"2.0\"},"
                + "\"scene\":0,"
                + "\"scenes\":[{\"nodes\":[0]}],"
                + "\"nodes\":[{\"mesh\":0,\"name\":\"Node0\"}],"
                + "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0}]}],"
                + "\"buffers\":["
                + "{\"uri\":\"data:application/octet-stream;base64," + GEOMETRY_BUFFER_BASE64 + "\",\"byteLength\":42},"
                + "{\"uri\":\"data:application/octet-stream;base64," + pngBase64 + "\",\"byteLength\":" + png.length + "}],"
                + "\"bufferViews\":["
                + "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
                + "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6},"
                + "{\"buffer\":1,\"byteOffset\":0,\"byteLength\":" + png.length + "}],"
                + "\"accessors\":["
                + "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
                + "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
                + "\"samplers\":[{\"name\":\"NearestSampler\",\"wrapS\":33071,\"wrapT\":33648,\"minFilter\":9984,\"magFilter\":9728}],"
                + "\"images\":["
                + "{\"name\":\"ExternalImage\",\"uri\":\"" + externalImageUri + "\",\"mimeType\":\"image/png\"},"
                + "{\"name\":\"DataImage\",\"uri\":\"data:image/png;base64," + pngBase64 + "\"},"
                + "{\"name\":\"BufferImage\",\"bufferView\":2,\"mimeType\":\"image/png\"}],"
                + "\"textures\":["
                + "{\"name\":\"ExternalTexture\",\"sampler\":0,\"source\":0},"
                + "{\"name\":\"DataTexture\",\"sampler\":0,\"source\":1},"
                + "{\"name\":\"BufferTexture\",\"sampler\":0,\"source\":2},"
                + "{\"name\":\"ExternalAgain\",\"sampler\":0,\"source\":0}],"
                + "\"materials\":[{"
                + "\"name\":\"Paint\","
                + "\"pbrMetallicRoughness\":{"
                + "\"baseColorFactor\":[0.25,0.5,0.75,1],\"metallicFactor\":0.6,\"roughnessFactor\":0.4,"
                + "\"baseColorTexture\":{\"index\":0},\"metallicRoughnessTexture\":{\"index\":1}},"
                + "\"normalTexture\":{\"index\":1,\"scale\":0.75},"
                + "\"occlusionTexture\":{\"index\":2,\"strength\":0.5},"
                + "\"emissiveTexture\":{\"index\":3},\"emissiveFactor\":[0.1,0.2,0.3],"
                + "\"alphaMode\":\"BLEND\",\"doubleSided\":true}]}";
    }

    private static String meshMetadataGltf(String... names) {
        StringBuilder meshes = new StringBuilder();
        for (int index = 0; index < names.length; ++index) {
            if (index > 0) {
                meshes.append(',');
            }
            meshes.append('{');
            if (names[index] != null) {
                meshes.append("\"name\":").append(jsonString(names[index])).append(',');
            }
            meshes.append("\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}");
        }

        return "{"
                + "\"asset\":{\"version\":\"2.0\"},"
                + "\"scene\":0,"
                + "\"scenes\":[{\"nodes\":[0]}],"
                + "\"nodes\":[{\"mesh\":0}],"
                + "\"meshes\":[" + meshes + "],"
                + "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,"
                + GEOMETRY_BUFFER_BASE64 + "\",\"byteLength\":42}],"
                + "\"bufferViews\":["
                + "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
                + "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
                + "\"accessors\":["
                + "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
                + "\"min\":[0,0,0],\"max\":[1,1,0]},"
                + "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}]}";
    }

    private static String jsonString(String value) {
        StringBuilder result = new StringBuilder(value.length() + 2);
        result.append('"');
        for (int offset = 0; offset < value.length();) {
            int codePoint = value.codePointAt(offset);
            switch (codePoint) {
                case '"':
                    result.append("\\\"");
                    break;
                case '\\':
                    result.append("\\\\");
                    break;
                case '\b':
                    result.append("\\b");
                    break;
                case '\f':
                    result.append("\\f");
                    break;
                case '\n':
                    result.append("\\n");
                    break;
                case '\r':
                    result.append("\\r");
                    break;
                case '\t':
                    result.append("\\t");
                    break;
                default:
                    result.appendCodePoint(codePoint);
                    break;
            }
            offset += Character.charCount(codePoint);
        }
        return result.append('"').toString();
    }

    private byte[] glb(String externalImageUri) {
        return glbFromGltf(gltf(externalImageUri));
    }

    private byte[] glbFromGltf(String source) {
        byte[] geometry = Base64.getDecoder().decode(GEOMETRY_BUFFER_BASE64);
        String embeddedGeometry = "{\"uri\":\"data:application/octet-stream;base64,"
                + GEOMETRY_BUFFER_BASE64 + "\",\"byteLength\":" + geometry.length + "}";
        String json = source.replace(
                embeddedGeometry, "{\"byteLength\":" + geometry.length + "}");
        byte[] jsonBytes = json.getBytes(StandardCharsets.UTF_8);
        int paddedJsonLength = (jsonBytes.length + 3) & ~3;
        int paddedGeometryLength = (geometry.length + 3) & ~3;
        int glbLength = 12 + 8 + paddedJsonLength + 8 + paddedGeometryLength;

        ByteBuffer glb = ByteBuffer.allocate(glbLength).order(ByteOrder.LITTLE_ENDIAN);
        glb.putInt(0x46546c67);
        glb.putInt(2);
        glb.putInt(glbLength);
        glb.putInt(paddedJsonLength);
        glb.putInt(0x4e4f534a);
        glb.put(jsonBytes);
        while (glb.position() < 20 + paddedJsonLength) {
            glb.put((byte)' ');
        }
        glb.putInt(paddedGeometryLength);
        glb.putInt(0x004e4942);
        glb.put(geometry);
        return glb.array();
    }
}
