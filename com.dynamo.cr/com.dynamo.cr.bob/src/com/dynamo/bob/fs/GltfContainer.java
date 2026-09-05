// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

package com.dynamo.bob.fs;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.Normalizer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Base64;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.pipeline.ModelImporterJni;
import com.dynamo.bob.pipeline.ModelUtil;
import com.dynamo.bob.pipeline.Modelimporter;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.google.protobuf.TextFormat;

/**
 * Immutable virtual-resource view of one glTF source resource.
 *
 * The native model importer is authoritative for glTF enumeration and material
 * semantics. Java only resolves encoded image bytes that are not supplied by
 * the importer and adapts the native scene to Bob resources.
 */
public final class GltfContainer {

    /** The kind of virtual asset extracted from a glTF container. */
    public enum AssetKind {
        MATERIAL,
        IMAGE,
        MESH
    }

    /** Immutable metadata for a glTF texture and its selected virtual image. */
    public static final class TextureMetadata {
        private final int index;
        private final String name;
        private final int samplerIndex;
        private final int minFilter;
        private final int magFilter;
        private final int wrapS;
        private final int wrapT;
        private final boolean basisu;

        TextureMetadata(int index, String name, int samplerIndex, int minFilter, int magFilter,
                        int wrapS, int wrapT, boolean basisu) {
            this.index = index;
            this.name = name;
            this.samplerIndex = samplerIndex;
            this.minFilter = minFilter;
            this.magFilter = magFilter;
            this.wrapS = wrapS;
            this.wrapT = wrapT;
            this.basisu = basisu;
        }

        public int getIndex() { return index; }
        public String getName() { return name; }
        public int getSamplerIndex() { return samplerIndex; }
        public int getMinFilter() { return minFilter; }
        public int getMagFilter() { return magFilter; }
        public int getWrapS() { return wrapS; }
        public int getWrapT() { return wrapT; }
        public boolean isBasisu() { return basisu; }
    }

    /**
     * Immutable native-backed relationship from a generated material sampler to
     * the virtual image asset that supplies it.
     */
    public static final class SamplerBinding {
        private final String samplerName;
        private final int materialIndex;
        private final int textureIndex;
        private final int imageIndex;
        private final String imagePath;

        SamplerBinding(String samplerName, int materialIndex, int textureIndex,
                       int imageIndex, String imagePath) {
            this.samplerName = samplerName;
            this.materialIndex = materialIndex;
            this.textureIndex = textureIndex;
            this.imageIndex = imageIndex;
            this.imagePath = imagePath;
        }

        public String getSamplerName() { return samplerName; }
        public int getMaterialIndex() { return materialIndex; }
        public int getTextureIndex() { return textureIndex; }
        public int getImageIndex() { return imageIndex; }
        public String getImagePath() { return imagePath; }
    }

    /**
     * Immutable, file-system-independent description of one virtual glTF asset.
     * The path is relative to the source glTF resource.
     */
    public abstract static class Asset {
        private final String path;
        private final AssetKind kind;
        private final int index;
        private final String name;
        private final byte[] content;

        Asset(String path, AssetKind kind, int index, String name, byte[] content) {
            this.path = path;
            this.kind = kind;
            this.index = index;
            this.name = name;
            this.content = content.clone();
        }

        public final String getPath() {
            return path;
        }

        public final AssetKind getKind() {
            return kind;
        }

        public final int getIndex() {
            return index;
        }

        public final String getName() {
            return name;
        }

        public final byte[] getContent() {
            return content.clone();
        }
    }

    /** A virtual Defold material backed by native ModelImporter material data. */
    public static final class MaterialAsset extends Asset {
        private final MaterialDesc materialDesc;
        private final Modelimporter.Material sourceMaterial;
        private final Map<String, SamplerBinding> samplerBindings;

        MaterialAsset(String path, Modelimporter.Material sourceMaterial, MaterialDesc materialDesc,
                      Map<String, SamplerBinding> samplerBindings) {
            super(path, AssetKind.MATERIAL, sourceMaterial.index, sourceMaterial.name,
                    TextFormat.printToString(materialDesc).getBytes(StandardCharsets.UTF_8));
            this.materialDesc = materialDesc;
            this.sourceMaterial = sourceMaterial;
            this.samplerBindings = immutableSamplerBindings(samplerBindings);
        }

        public MaterialDesc getMaterialDesc() {
            return materialDesc;
        }

        public Map<String, SamplerBinding> getSamplerBindings() {
            return samplerBindings;
        }

        Modelimporter.Material getSourceMaterial() {
            return sourceMaterial;
        }
    }

    /** A virtual encoded image plus metadata for textures that select it. */
    public static final class ImageAsset extends Asset {
        private final String uri;
        private final String mimeType;
        private final String sourceKind;
        private final List<TextureMetadata> textures;

        ImageAsset(String path, int index, String name, String uri, String mimeType, String sourceKind,
                   byte[] content, List<TextureMetadata> textures) {
            super(path, AssetKind.IMAGE, index, name, content);
            this.uri = uri;
            this.mimeType = mimeType;
            this.sourceKind = sourceKind;
            this.textures = Collections.unmodifiableList(
                    new ArrayList<TextureMetadata>(textures));
        }

        public String getUri() {
            return uri;
        }

        public String getMimeType() {
            return mimeType;
        }

        public String getSourceKind() {
            return sourceKind;
        }

        /** Textures selecting this image; each extracted texture belongs to one image. */
        public List<TextureMetadata> getTextures() {
            return textures;
        }
    }

    /** A virtual glTF mesh plus its immutable native-backed summary metadata. */
    public static final class MeshMetadata extends Asset {
        private final boolean nameGenerated;
        private final int primitiveCount;
        private final int vertexCount;

        MeshMetadata(String path, int index, String name, boolean nameGenerated,
                     int primitiveCount, int vertexCount) {
            super(path, AssetKind.MESH, index, name, new byte[0]);
            this.nameGenerated = nameGenerated;
            this.primitiveCount = primitiveCount;
            this.vertexCount = vertexCount;
        }

        public boolean isNameGenerated() { return nameGenerated; }
        public int getPrimitiveCount() { return primitiveCount; }
        public int getVertexCount() { return vertexCount; }
    }

    /** Immutable result from the native-backed, file-system-independent extractor. */
    public static final class Extraction {
        private final List<Asset> assets;
        private final List<MeshMetadata> meshes;
        private final List<String> diagnostics;

        Extraction(List<Asset> assets, List<MeshMetadata> meshes, List<String> diagnostics) {
            this.assets = Collections.unmodifiableList(new ArrayList<Asset>(assets));
            this.meshes = Collections.unmodifiableList(new ArrayList<MeshMetadata>(meshes));
            this.diagnostics = Collections.unmodifiableList(new ArrayList<String>(diagnostics));
        }

        public List<Asset> getAssets() {
            return assets;
        }

        public List<MeshMetadata> getMeshes() {
            return meshes;
        }

        public List<String> getDiagnostics() {
            return diagnostics;
        }
    }

    private static final long MAX_IMAGE_BYTES = 256L * 1024L * 1024L;
    private static final long MAX_TOTAL_IMAGE_BYTES = 512L * 1024L * 1024L;
    private static final int MAX_EXTERNAL_URI_CHARACTERS = 8192;
    private static final int MAX_DATA_URI_METADATA_CHARACTERS = 1024;
    private static final long MAX_DATA_URI_BASE64_CHARACTERS = ((MAX_IMAGE_BYTES + 2L) / 3L) * 4L;
    private static final int MAX_MESH_PATH_NAME_BYTES = 255;

    private final IResource sourceResource;
    private final List<GltfResource> resources;
    private final Map<String, GltfResource> resourcesByPath;
    private final List<String> diagnostics;
    private final Map<String, byte[]> dependencyDigests;

    private GltfContainer(IResource sourceResource, List<GltfResource> resources, List<String> diagnostics,
                          Map<String, byte[]> dependencyDigests) {
        this.sourceResource = sourceResource;
        this.resources = Collections.unmodifiableList(new ArrayList<GltfResource>(resources));
        this.diagnostics = Collections.unmodifiableList(new ArrayList<String>(diagnostics));
        this.dependencyDigests = immutableDigestMap(dependencyDigests);

        Map<String, GltfResource> resourcesByPath = new LinkedHashMap<String, GltfResource>();
        for (GltfResource resource : resources) {
            resourcesByPath.put(resource.getPath(), resource);
            String childPath = resource.getPath().substring(sourceResource.getPath().length() + 1);
            resourcesByPath.put(childPath, resource);
        }
        this.resourcesByPath = Collections.unmodifiableMap(resourcesByPath);
    }

    public static GltfContainer load(IFileSystem fileSystem, IResource sourceResource) throws IOException {
        byte[] sourceBytes = sourceResource.getContent();
        if (sourceBytes == null) {
            throw new IOException(String.format("glTF source '%s' does not exist", sourceResource.getPath()));
        }

        DependencyTracker dependencyTracker = new DependencyTracker();
        dependencyTracker.record(sourceResource.getPath(), sourceBytes);
        ResourceDataResolver dataResolver = new ResourceDataResolver(fileSystem, sourceResource, dependencyTracker);
        Extraction extraction = extract(sourceBytes, sourceResource.getPath(), dataResolver);

        List<GltfResource> resources = new ArrayList<GltfResource>();
        for (Asset asset : extraction.getAssets()) {
            String path = sourceResource.getPath() + "/" + asset.getPath();
            if (asset instanceof MaterialAsset) {
                MaterialAsset material = (MaterialAsset)asset;
                resources.add(new GltfMaterialResource(fileSystem, sourceResource, path,
                        material.getSourceMaterial(), material.getMaterialDesc(), material.getSamplerBindings()));
            } else if (asset instanceof ImageAsset) {
                ImageAsset image = (ImageAsset)asset;
                resources.add(new GltfImageResource(fileSystem, sourceResource, path, image.getIndex(),
                        image.getName(), image.getUri(), image.getMimeType(), image.getSourceKind(),
                        image.getContent(), image.getTextures()));
            } else if (asset instanceof MeshMetadata) {
                resources.add(new GltfMeshResource(fileSystem, sourceResource, path,
                        (MeshMetadata)asset));
            }
        }

        return new GltfContainer(sourceResource, resources, extraction.getDiagnostics(),
                dependencyTracker.getDigests());
    }

    /**
     * Extracts virtual glTF assets without requiring Bob's file-system API.
     *
     * The supplied resolver is used by native ModelImporter for external buffers
     * and by this adapter for external image bytes. Asset paths in the result are
     * relative to {@code sourcePath}; returned byte arrays are defensive copies.
     */
    public static Extraction extract(byte[] sourceBytes, String sourcePath,
                                     ModelImporterJni.DataResolver dataResolver) throws IOException {
        if (sourceBytes == null) {
            throw new IllegalArgumentException("sourceBytes cannot be null");
        }
        if (sourcePath == null || sourcePath.isEmpty()) {
            throw new IllegalArgumentException("sourcePath cannot be null or empty");
        }

        Modelimporter.Options options = new Modelimporter.Options();
        options.loadMaterialsOnly = true;
        options.loadMeshMetadata = true;
        Modelimporter.Scene scene = ModelUtil.loadScene(
                sourceBytes, sourcePath, options, dataResolver);

        List<Asset> assets = new ArrayList<Asset>();
        List<String> diagnostics = new ArrayList<String>();
        Map<Integer, String> imagePaths = extractImages(
                sourcePath, dataResolver, scene, assets, diagnostics);
        extractMaterials(scene, imagePaths, assets);
        List<MeshMetadata> meshes = extractMeshMetadata(scene);
        assets.addAll(meshes);

        assets.sort(Comparator.comparing(Asset::getKind).thenComparingInt(Asset::getIndex));
        return new Extraction(assets, meshes, diagnostics);
    }

    /**
     * Resolves a glTF external-resource URI to a normalized project-relative path.
     *
     * {@link ModelImporterJni.DataResolver} implementations can use this helper
     * before looking up an external buffer or image in their own resource system.
     * It rejects absolute URIs, authorities, queries, fragments, backslashes,
     * project-root traversal, and control or format characters.
     */
    public static String resolveExternalResourcePath(String sourcePath, String rawUri) throws IOException {
        if (sourcePath == null || sourcePath.isEmpty()) {
            throw new IllegalArgumentException("sourcePath cannot be null or empty");
        }
        if (rawUri == null) {
            throw new IllegalArgumentException("rawUri cannot be null");
        }
        if (rawUri.length() > MAX_EXTERNAL_URI_CHARACTERS) {
            throw new IOException("external resource URI exceeds the length limit");
        }

        final String decodedPath;
        try {
            URI uri = new URI(rawUri.replace(" ", "%20"));
            if (uri.isAbsolute() || uri.getRawAuthority() != null || uri.getRawQuery() != null
                    || uri.getRawFragment() != null) {
                throw new IOException(String.format("unsupported external URI '%s'", uriForDiagnostic(rawUri)));
            }
            decodedPath = uri.getPath();
        } catch (URISyntaxException e) {
            throw new IOException(String.format("invalid external URI '%s'", uriForDiagnostic(rawUri)), e);
        }

        if (decodedPath == null || decodedPath.isEmpty() || decodedPath.indexOf('\\') >= 0
                || FilenameUtils.getPrefixLength(decodedPath) != 0) {
            throw new IOException(String.format("invalid external resource path '%s'", uriForDiagnostic(rawUri)));
        }
        for (int i = 0; i < decodedPath.length(); ++i) {
            char c = decodedPath.charAt(i);
            if (Character.isISOControl(c) || Character.getType(c) == Character.FORMAT) {
                throw new IOException(String.format("invalid external resource path '%s'", uriForDiagnostic(rawUri)));
            }
        }

        String basePath = FilenameUtils.getPath(sourcePath);
        String path = FilenameUtils.normalize(FilenameUtils.concat(basePath, decodedPath), true);
        if (path == null || path.startsWith("../") || path.startsWith("/")) {
            throw new IOException(String.format("external resource escapes the project root: '%s'",
                    uriForDiagnostic(rawUri)));
        }
        return path;
    }

    public IResource getSourceResource() {
        return sourceResource;
    }

    public Collection<GltfResource> getResources() {
        return resources;
    }

    public List<String> getDiagnostics() {
        return diagnostics;
    }

    public GltfResource getResource(String path) {
        String normalizedPath = FilenameUtils.normalize(path, true);
        if (normalizedPath == null) {
            return null;
        }
        while (normalizedPath.startsWith("/")) {
            normalizedPath = normalizedPath.substring(1);
        }
        return resourcesByPath.get(normalizedPath);
    }

    public boolean isStale(IFileSystem fileSystem) {
        for (Map.Entry<String, byte[]> entry : dependencyDigests.entrySet()) {
            IResource resource = fileSystem.get(entry.getKey());
            boolean exists = resource != null && resource.exists() && resource.isFile();
            byte[] expectedDigest = entry.getValue();
            if (expectedDigest == null) {
                if (exists) {
                    return true;
                }
                continue;
            }
            if (!exists) {
                return true;
            }
            try {
                if (!Arrays.equals(expectedDigest, resource.sha1())) {
                    return true;
                }
            } catch (IOException | RuntimeException e) {
                return true;
            }
        }
        return false;
    }

    private static Map<String, byte[]> immutableDigestMap(Map<String, byte[]> dependencyDigests) {
        Map<String, byte[]> result = new LinkedHashMap<String, byte[]>();
        for (Map.Entry<String, byte[]> entry : dependencyDigests.entrySet()) {
            byte[] digest = entry.getValue();
            result.put(entry.getKey(), digest == null ? null : digest.clone());
        }
        return Collections.unmodifiableMap(result);
    }

    static Map<String, SamplerBinding> immutableSamplerBindings(
            Map<String, SamplerBinding> samplerBindings) {
        return Collections.unmodifiableMap(
                new LinkedHashMap<String, SamplerBinding>(samplerBindings));
    }

    private static List<MeshMetadata> extractMeshMetadata(Modelimporter.Scene scene) {
        List<MeshMetadata> meshes = new ArrayList<MeshMetadata>();
        if (scene.models == null) {
            return meshes;
        }

        List<String> pathNames = meshPathNames(scene.models);
        for (int modelOrdinal = 0; modelOrdinal < scene.models.length; ++modelOrdinal) {
            Modelimporter.Model model = scene.models[modelOrdinal];
            int primitiveCount = model.meshes == null ? 0 : model.meshes.length;
            long vertexCount = 0;
            if (model.meshes != null) {
                for (Modelimporter.Mesh mesh : model.meshes) {
                    vertexCount += Math.max(0, mesh.vertexCount);
                    if (vertexCount >= Integer.MAX_VALUE) {
                        vertexCount = Integer.MAX_VALUE;
                        break;
                    }
                }
            }
            String path = "meshes/" + pathNames.get(modelOrdinal);
            meshes.add(new MeshMetadata(path, model.index, model.name, model.nameIsGenerated,
                    primitiveCount, (int)vertexCount));
        }
        return meshes;
    }

    private static List<String> meshPathNames(Modelimporter.Model[] models) {
        List<String> pathNames = new ArrayList<String>(models.length);
        for (Modelimporter.Model model : models) {
            String name = model.name;
            if (model.nameIsGenerated || isBlank(name) || !isPortableFilenameSegment(name)) {
                name = "Mesh " + model.index;
            }
            pathNames.add(name);
        }

        while (true) {
            Map<String, List<Integer>> ordinalsByName = new LinkedHashMap<String, List<Integer>>();
            for (int ordinal = 0; ordinal < pathNames.size(); ++ordinal) {
                String comparisonName = meshPathComparisonName(pathNames.get(ordinal));
                List<Integer> ordinals = ordinalsByName.get(comparisonName);
                if (ordinals == null) {
                    ordinals = new ArrayList<Integer>();
                    ordinalsByName.put(comparisonName, ordinals);
                }
                ordinals.add(ordinal);
            }

            boolean foundCollision = false;
            for (List<Integer> ordinals : ordinalsByName.values()) {
                if (ordinals.size() < 2) {
                    continue;
                }
                foundCollision = true;
                for (int ordinal : ordinals) {
                    Modelimporter.Model model = models[ordinal];
                    String suffixedName = pathNames.get(ordinal) + " [" + model.index + "]";
                    pathNames.set(ordinal, utf8Length(suffixedName) <= MAX_MESH_PATH_NAME_BYTES
                            ? suffixedName
                            : "Mesh " + model.index);
                }
            }
            if (!foundCollision) {
                return pathNames;
            }
        }
    }

    private static String meshPathComparisonName(String name) {
        String normalizedName = Normalizer.normalize(name, Normalizer.Form.NFC);
        return Normalizer.normalize(normalizedName.toLowerCase(Locale.ROOT), Normalizer.Form.NFC);
    }

    private static boolean isBlank(String name) {
        if (name == null || name.isEmpty()) {
            return true;
        }
        for (int offset = 0; offset < name.length();) {
            int codePoint = name.codePointAt(offset);
            if (!Character.isWhitespace(codePoint) && !Character.isSpaceChar(codePoint)) {
                return false;
            }
            offset += Character.charCount(codePoint);
        }
        return true;
    }

    private static boolean isPortableFilenameSegment(String name) {
        if (name == null || name.isEmpty() || ".".equals(name) || "..".equals(name)) {
            return false;
        }
        if (utf8Length(name) > MAX_MESH_PATH_NAME_BYTES) {
            return false;
        }

        int firstCodePoint = name.codePointAt(0);
        int finalCodePoint = name.codePointBefore(name.length());
        if (Character.isWhitespace(firstCodePoint) || Character.isSpaceChar(firstCodePoint)
                || finalCodePoint == '.' || Character.isWhitespace(finalCodePoint)
                || Character.isSpaceChar(finalCodePoint)) {
            return false;
        }

        for (int offset = 0; offset < name.length();) {
            int codePoint = name.codePointAt(offset);
            int characterType = Character.getType(codePoint);
            if (Character.isISOControl(codePoint) || characterType == Character.FORMAT
                    || characterType == Character.SURROGATE || codePoint == '/'
                    || codePoint == '\\' || codePoint == '<' || codePoint == '>'
                    || codePoint == ':' || codePoint == '"' || codePoint == '|'
                    || codePoint == '?' || codePoint == '*') {
                return false;
            }
            offset += Character.charCount(codePoint);
        }

        String deviceName = name;
        int extensionSeparator = deviceName.indexOf('.');
        if (extensionSeparator >= 0) {
            deviceName = deviceName.substring(0, extensionSeparator);
        }
        deviceName = deviceName.toUpperCase(Locale.ROOT);
        if ("CON".equals(deviceName) || "PRN".equals(deviceName)
                || "AUX".equals(deviceName) || "NUL".equals(deviceName)
                || "CLOCK$".equals(deviceName)) {
            return false;
        }
        return !deviceName.matches("(?:COM|LPT)[1-9]");
    }

    private static int utf8Length(String value) {
        return value.getBytes(StandardCharsets.UTF_8).length;
    }

    private static void extractMaterials(Modelimporter.Scene scene, Map<Integer, String> imagePaths,
                                         List<Asset> assets) {
        if (scene.materials == null) {
            return;
        }

        Set<Modelimporter.Material> dynamicMaterials = Collections.newSetFromMap(
                new IdentityHashMap<Modelimporter.Material, Boolean>());
        if (scene.dynamicMaterials != null) {
            Collections.addAll(dynamicMaterials, scene.dynamicMaterials);
        }
        for (Modelimporter.Material material : scene.materials) {
            if (dynamicMaterials.contains(material)) {
                continue;
            }
            String path = String.format("materials/%d.material", material.index);
            assets.add(new MaterialAsset(path, material, GltfMaterialResource.createMaterialDesc(material),
                    samplerBindings(material, imagePaths)));
        }
    }

    private static Map<Integer, String> extractImages(
            String sourcePath, ModelImporterJni.DataResolver dataResolver,
            Modelimporter.Scene scene, List<Asset> assets, List<String> diagnostics) {
        Map<Integer, String> imagePaths = new LinkedHashMap<Integer, String>();
        if (scene.images == null) {
            return imagePaths;
        }

        Map<Integer, ResolvedImage> resolvedImages = new LinkedHashMap<Integer, ResolvedImage>();
        ExtractionBudget extractionBudget = new ExtractionBudget();
        for (Modelimporter.Image image : scene.images) {
            try {
                ResolvedImage resolvedImage = resolveImage(sourcePath, dataResolver, image);
                extractionBudget.include(resolvedImage.content);
                String extension = extensionForImage(
                        resolvedImage.mimeType, resolvedImage.uri, resolvedImage.content);
                String mimeType = resolvedImage.mimeType == null
                        ? mimeTypeForExtension(extension)
                        : resolvedImage.mimeType;
                String path = String.format("images/%d.%s", image.index, extension);
                imagePaths.put(image.index, path);
                resolvedImages.put(image.index, new ResolvedImage(resolvedImage.uri, mimeType,
                        resolvedImage.sourceKind, resolvedImage.content));
            } catch (IOException e) {
                diagnostics.add(String.format("Image %d: %s", image.index, e.getMessage()));
            }
        }
        // Select texture images only after all extraction successes and failures are known.
        for (Modelimporter.Image image : scene.images) {
            ResolvedImage resolvedImage = resolvedImages.remove(image.index);
            if (resolvedImage == null) {
                continue;
            }
            assets.add(new ImageAsset(imagePaths.get(image.index), image.index, image.name,
                    resolvedImage.uri, resolvedImage.mimeType, resolvedImage.sourceKind,
                    resolvedImage.content, textureMetadata(scene, image.index, imagePaths)));
        }
        return imagePaths;
    }

    private static Map<String, SamplerBinding> samplerBindings(
            Modelimporter.Material material, Map<Integer, String> imagePaths) {
        Map<String, SamplerBinding> result = new LinkedHashMap<String, SamplerBinding>();
        for (Map.Entry<String, Modelimporter.TextureView> entry
                : GltfMaterialResource.samplerTextureViews(material).entrySet()) {
            Modelimporter.Texture texture = entry.getValue().texture;
            Modelimporter.Image image = referencedImage(texture, imagePaths);
            if (image == null) {
                continue;
            }

            String imagePath = imagePaths.get(image.index);
            result.put(entry.getKey(), new SamplerBinding(
                    entry.getKey(), material.index, texture.index, image.index, imagePath));
        }
        return result;
    }

    private static Modelimporter.Image referencedImage(
            Modelimporter.Texture texture, Map<Integer, String> imagePaths) {
        if (texture.basisuImage != null && imagePaths.containsKey(texture.basisuImage.index)) {
            return texture.basisuImage;
        }
        if (texture.image != null && imagePaths.containsKey(texture.image.index)) {
            return texture.image;
        }
        return null;
    }

    private static final class ResolvedImage {
        final String uri;
        final String mimeType;
        final String sourceKind;
        final byte[] content;

        ResolvedImage(String uri, String mimeType, String sourceKind, byte[] content) {
            this.uri = uri;
            this.mimeType = mimeType;
            this.sourceKind = sourceKind;
            this.content = content;
        }
    }

    private static ResolvedImage resolveImage(String sourcePath, ModelImporterJni.DataResolver dataResolver,
                                              Modelimporter.Image image) throws IOException {
        String uri = image.uri;
        String mimeType = normalizedMimeType(image.mimeType);
        if (uri != null && !uri.isEmpty()) {
            if (isDataUri(uri)) {
                DataUri dataUri = decodeDataUri(uri);
                if (mimeType != null && dataUri.mimeType != null && !mimeType.equals(dataUri.mimeType)) {
                    throw new IOException(String.format("image MIME type '%s' conflicts with data URI MIME type '%s'",
                            mimeType, dataUri.mimeType));
                }
                return new ResolvedImage(uri, mimeType == null ? dataUri.mimeType : mimeType,
                        "data-uri", dataUri.content);
            }

            byte[] content = dataResolver == null ? null : dataResolver.getData(sourcePath, uri);
            if (content == null) {
                throw new IOException(String.format("external resource does not exist: '%s'",
                        uriForDiagnostic(uri)));
            }
            return new ResolvedImage(uri, mimeType, "external-uri", content);
        }

        if (image.buffer == null || image.buffer.buffer == null || image.buffer.buffer.length == 0) {
            throw new IOException("embedded image bytes were not supplied by the model importer");
        }
        if (mimeType == null || mimeType.isEmpty()) {
            throw new IOException("embedded image has no mimeType");
        }
        return new ResolvedImage(null, mimeType, "buffer-view", image.buffer.buffer);
    }

    private static List<TextureMetadata> textureMetadata(Modelimporter.Scene scene, int imageIndex,
                                                        Map<Integer, String> imagePaths) {
        List<TextureMetadata> result = new ArrayList<TextureMetadata>();
        if (scene.textures == null) {
            return result;
        }

        for (Modelimporter.Texture texture : scene.textures) {
            Modelimporter.Image image = referencedImage(texture, imagePaths);
            if (image == null || image.index != imageIndex) {
                continue;
            }

            Modelimporter.Sampler sampler = texture.sampler;
            int samplerIndex = sampler == null ? -1 : sampler.index;
            int minFilter = sampler == null ? 0 : sampler.minFilter;
            int magFilter = sampler == null ? 0 : sampler.magFilter;
            int wrapS = sampler == null ? 10497 : sampler.wrapS;
            int wrapT = sampler == null ? 10497 : sampler.wrapT;
            result.add(new TextureMetadata(texture.index, texture.name, samplerIndex,
                    minFilter, magFilter, wrapS, wrapT, image == texture.basisuImage));
        }
        result.sort(Comparator.comparingInt(TextureMetadata::getIndex));
        return result;
    }

    private static final class ExtractionBudget {
        private long totalImageBytes;

        void include(byte[] content) throws IOException {
            if (content.length > MAX_IMAGE_BYTES) {
                throw new IOException("encoded image exceeds the virtual image size limit");
            }
            if (content.length > MAX_TOTAL_IMAGE_BYTES - totalImageBytes) {
                throw new IOException("container exceeds the total virtual image size limit");
            }
            totalImageBytes += content.length;
        }
    }

    private static final class DataUri {
        final String mimeType;
        final byte[] content;

        DataUri(String mimeType, byte[] content) {
            this.mimeType = mimeType;
            this.content = content;
        }
    }

    private static DataUri decodeDataUri(String uri) throws IOException {
        int comma = uri.indexOf(',');
        if (comma < 5) {
            throw new IOException("malformed data URI");
        }
        if (comma - 5 > MAX_DATA_URI_METADATA_CHARACTERS) {
            throw new IOException("data URI metadata exceeds the length limit");
        }

        String metadata = uri.substring(5, comma);
        String[] parts = metadata.split(";");
        String mimeType = parts.length > 0 && parts[0].contains("/")
                ? normalizedMimeType(parts[0])
                : null;
        boolean base64 = false;
        for (int i = 1; i < parts.length; ++i) {
            if ("base64".equalsIgnoreCase(parts[i])) {
                base64 = true;
            }
        }

        int payloadLength = uri.length() - comma - 1;
        long maximumPayloadCharacters = base64 ? MAX_DATA_URI_BASE64_CHARACTERS : MAX_IMAGE_BYTES;
        if (payloadLength > maximumPayloadCharacters) {
            throw new IOException("data URI payload exceeds the encoded image size limit");
        }
        String payload = uri.substring(comma + 1);

        try {
            byte[] encodedPayload = percentDecode(payload);
            byte[] content = base64 ? Base64.getDecoder().decode(encodedPayload) : encodedPayload;
            return new DataUri(mimeType, content);
        } catch (IllegalArgumentException e) {
            throw new IOException("invalid data URI encoding", e);
        }
    }

    private static byte[] percentDecode(String value) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream(value.length());
        int segmentStart = 0;
        for (int i = 0; i < value.length(); ++i) {
            if (value.charAt(i) != '%') {
                continue;
            }

            if (segmentStart < i) {
                byte[] bytes = value.substring(segmentStart, i).getBytes(StandardCharsets.UTF_8);
                output.write(bytes, 0, bytes.length);
            }
            if (i + 2 >= value.length()) {
                throw new IOException("truncated percent encoding");
            }
            int high = Character.digit(value.charAt(i + 1), 16);
            int low = Character.digit(value.charAt(i + 2), 16);
            if (high < 0 || low < 0) {
                throw new IOException("invalid percent encoding");
            }
            output.write((high << 4) | low);
            i += 2;
            segmentStart = i + 1;
        }
        if (segmentStart < value.length()) {
            byte[] bytes = value.substring(segmentStart).getBytes(StandardCharsets.UTF_8);
            output.write(bytes, 0, bytes.length);
        }
        return output.toByteArray();
    }

    private static String normalizedMimeType(String mimeType) {
        return mimeType == null ? null : mimeType.replace("\\/", "/").toLowerCase(Locale.ROOT);
    }

    private static String extensionForImage(String mimeType, String uri, byte[] content) throws IOException {
        String mimeExtension = null;
        if (mimeType != null) {
            switch (mimeType) {
                case "image/png":
                    mimeExtension = "png";
                    break;
                case "image/jpeg":
                    mimeExtension = "jpg";
                    break;
                default:
                    throw new IOException(String.format("unsupported image MIME type '%s'", mimeType));
            }
        }

        String signatureExtension = null;
        if (content.length >= 8
                && (content[0] & 0xff) == 0x89
                && content[1] == 0x50
                && content[2] == 0x4e
                && content[3] == 0x47
                && content[4] == 0x0d
                && content[5] == 0x0a
                && content[6] == 0x1a
                && content[7] == 0x0a) {
            signatureExtension = "png";
        } else if (content.length >= 3
                && (content[0] & 0xff) == 0xff
                && (content[1] & 0xff) == 0xd8
                && (content[2] & 0xff) == 0xff) {
            signatureExtension = "jpg";
        }

        if (signatureExtension == null) {
            throw new IOException("unsupported encoded image format; expected PNG or JPEG");
        }
        if (mimeExtension != null && !mimeExtension.equals(signatureExtension)) {
            throw new IOException(String.format("image MIME type '%s' does not match encoded bytes", mimeType));
        }

        if (uri != null && !isDataUri(uri)) {
            String uriExtension = FilenameUtils.getExtension(uri).toLowerCase(Locale.ROOT);
            if ("jpeg".equals(uriExtension)) {
                uriExtension = "jpg";
            }
            if (("png".equals(uriExtension) || "jpg".equals(uriExtension))
                    && !uriExtension.equals(signatureExtension)) {
                throw new IOException(String.format("image URI '%s' does not match encoded bytes",
                        uriForDiagnostic(uri)));
            }
        }
        return signatureExtension;
    }

    private static String mimeTypeForExtension(String extension) {
        return "png".equals(extension) ? "image/png" : "image/jpeg";
    }

    private static boolean isDataUri(String uri) {
        return uri != null && uri.regionMatches(true, 0, "data:", 0, 5);
    }

    private static String resolveExternalResourcePath(IResource sourceResource, String rawUri) throws IOException {
        return resolveExternalResourcePath(sourceResource.getPath(), rawUri);
    }

    private static byte[] resolveExternalResource(IFileSystem fileSystem, IResource sourceResource, String rawUri,
                                                  DependencyTracker dependencyTracker) throws IOException {
        String path = resolveExternalResourcePath(sourceResource, rawUri);
        IResource resource = fileSystem.get(path);
        if (resource == null || !resource.exists() || !resource.isFile()) {
            dependencyTracker.recordMissing(path);
            throw new IOException(String.format("external resource does not exist: '%s'",
                    uriForDiagnostic(rawUri)));
        }
        dependencyTracker.recordMissing(path);
        byte[] content = resource.getContent();
        if (content == null) {
            throw new IOException(String.format("external resource has no content: '%s'",
                    uriForDiagnostic(rawUri)));
        }
        dependencyTracker.record(path, content);
        return content;
    }

    private static String uriForDiagnostic(String uri) {
        if (uri == null) {
            return "";
        }

        String trimmed = uri.trim();
        if (isDataUri(trimmed)) {
            return "<data URI>";
        }

        StringBuilder sanitized = new StringBuilder();
        int limit = Math.min(uri.length(), 160);
        for (int i = 0; i < limit; ++i) {
            char c = uri.charAt(i);
            sanitized.append(Character.isISOControl(c) || Character.getType(c) == Character.FORMAT ? '?' : c);
        }
        if (uri.length() > limit) {
            sanitized.append("...");
        }
        return sanitized.toString();
    }

    private static final class DependencyTracker {
        private final Map<String, byte[]> digests = new LinkedHashMap<String, byte[]>();

        void record(String path, byte[] content) {
            digests.put(path, sha1(content));
        }

        void recordMissing(String path) {
            digests.put(path, null);
        }

        Map<String, byte[]> getDigests() {
            return digests;
        }

        private static byte[] sha1(byte[] content) {
            try {
                MessageDigest digest = MessageDigest.getInstance("SHA-1");
                return digest.digest(content);
            } catch (NoSuchAlgorithmException e) {
                throw new RuntimeException(e);
            }
        }
    }

    private static final class ResourceDataResolver implements ModelImporterJni.DataResolver {
        private final IFileSystem fileSystem;
        private final IResource sourceResource;
        private final DependencyTracker dependencyTracker;

        ResourceDataResolver(IFileSystem fileSystem, IResource sourceResource,
                             DependencyTracker dependencyTracker) {
            this.fileSystem = fileSystem;
            this.sourceResource = sourceResource;
            this.dependencyTracker = dependencyTracker;
        }

        @Override
        public byte[] getData(String path, String uri) {
            if (uri == null || uri.isEmpty()) {
                return null;
            }
            try {
                return resolveExternalResource(fileSystem, sourceResource, uri, dependencyTracker);
            } catch (IOException e) {
                return null;
            }
        }
    }
}
