package com.dynamo.cr.spine.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.ObjectInputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.media.opengl.GL2;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.bob.textureset.TextureSetGenerator.UVTransform;
import com.dynamo.bob.util.SpineSceneUtil;
import com.dynamo.bob.util.RigUtil.Bone;
import com.dynamo.bob.util.RigUtil.Mesh;
import com.dynamo.bob.util.RigUtil.UVTransformProvider;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.spine.Activator;
import com.dynamo.cr.tileeditor.scene.RuntimeTextureSet;
import com.dynamo.cr.tileeditor.scene.TextureSetNode;
import com.dynamo.spine.proto.Spine.SpineModelDesc.BlendMode;
import com.dynamo.spine.proto.Spine.SpineSceneDesc;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.google.protobuf.TextFormat;

@SuppressWarnings("serial")
public class SpineModelNode extends ComponentTypeNode {

    @Property(displayName="Spine Scene", editorType=EditorType.RESOURCE, extensions={"spinescene"})
    @Resource
    @NotEmpty
    private String spineScene = "";

    private transient SpineSceneDesc sceneDesc;
    private transient SpineSceneUtil scene;

    private transient TextureSetNode textureSetNode = null;

    private transient SpineBoneNode rootBoneNode = null;

    @Property(editorType=EditorType.DROP_DOWN)
    private String defaultAnimation = "";

    @Property(editorType=EditorType.DROP_DOWN)
    private String skin = "";

    @Property(editorType = EditorType.RESOURCE, extensions = { "material" })
    @Resource
    @NotEmpty
    private String material = "";

    @Property
    private BlendMode blendMode = BlendMode.BLEND_MODE_ALPHA;

    private transient CompositeMesh mesh = new CompositeMesh();

    @Override
    public void dispose(GL2 gl) {
        super.dispose(gl);
        if (this.textureSetNode != null) {
            this.textureSetNode.dispose(gl);
        }
        this.mesh.dispose(gl);
    }

    public CompositeMesh getCompositeMesh() {
        return this.mesh;
    }

    public String getSpineScene() {
        return spineScene;
    }

    public void setSpineScene(String spineScene) {
        if (!this.spineScene.equals(spineScene)) {
            this.spineScene = spineScene;
            reloadResources();
        }
    }

    public IStatus validateSpineScene() {
        if (!this.spineScene.isEmpty() && this.sceneDesc == null) {
            return new Status(IStatus.ERROR, Activator.PLUGIN_ID, Messages.SpineModelNode_spineScene_CONTENT_ERROR);
        }
        if (this.textureSetNode != null) {
            this.textureSetNode.updateStatus();
            IStatus status = this.textureSetNode.getStatus();
            if (!status.isOK()) {
                return new Status(IStatus.ERROR, Activator.PLUGIN_ID, Messages.SpineModelNode_atlas_INVALID_REFERENCE);
            }
            if (this.scene != null) {
                List<Mesh> meshes = this.scene.meshes;
                if (!this.skin.isEmpty() && this.scene.skins.containsKey(this.skin)) {
                    meshes = this.scene.skins.get(this.skin);
                }
                RuntimeTextureSet runtimeTextureSet = this.textureSetNode.getRuntimeTextureSet();
                if (runtimeTextureSet != null) {
                    Set<String> missingAnims = new HashSet<String>();
                    for (Mesh mesh : meshes) {
                        if (runtimeTextureSet.getAnimation(mesh.path) == null) {
                            missingAnims.add(mesh.path);
                        }
                    }
                    if (!missingAnims.isEmpty()) {
                        StringBuilder builder = new StringBuilder();
                        Iterator<String> it = missingAnims.iterator();
                        while (it.hasNext()) {
                            builder.append(it.next());
                            if (it.hasNext()) {
                                builder.append(", ");
                            }
                        }
                        return new Status(IStatus.ERROR, Activator.PLUGIN_ID, NLS.bind(Messages.SpineModelNode_atlas_MISSING_ANIMS, builder.toString()));
                    }
                }
            }
        }
        return Status.OK_STATUS;
    }

    public String getDefaultAnimation() {
        return this.defaultAnimation;
    }

    public void setDefaultAnimation(String defaultAnimation) {
        this.defaultAnimation = defaultAnimation;
        updateStatus();
        updateAABB();
    }

    public Object[] getDefaultAnimationOptions() {
        if (this.scene != null) {
            Set<String> animNames = this.scene.animations.keySet();
            return animNames.toArray(new String[animNames.size()]);
        } else {
            return new Object[0];
        }
    }

    private void updateAABB() {
        AABB aabb = new AABB();
        aabb.setIdentity();
        if (this.scene != null) {
            for (Mesh mesh : this.scene.meshes) {
                float[] v = mesh.vertices;
                int vertexCount = v.length / 5;
                for (int i = 0; i < vertexCount; ++i) {
                    int vi = i * 5;
                    aabb.union(v[vi+0], v[vi+1], v[vi+2]);
                }
            }
        }
        setAABB(aabb);
    }

    public IStatus validateDefaultAnimation() {
        if (!this.defaultAnimation.isEmpty() && this.scene != null) {
            boolean exists = this.scene.getAnimation(this.defaultAnimation) != null;
            if (!exists) {
                return new Status(IStatus.ERROR, Activator.PLUGIN_ID, NLS.bind(Messages.SpineModelNode_defaultAnimation_INVALID, this.defaultAnimation));
            }
        }
        return Status.OK_STATUS;
    }

    public String getSkin() {
        return this.skin;
    }

    public void setSkin(String skin) {
        if (!this.skin.equals(skin)) {
            this.skin = skin;
            updateMesh();
            updateStatus();
            updateAABB();
        }
    }

    public Object[] getSkinOptions() {
        if (this.scene != null) {
            Set<String> skinNames = this.scene.skins.keySet();
            return skinNames.toArray(new String[skinNames.size()]);
        } else {
            return new Object[0];
        }
    }

    public IStatus validateSkin() {
        if (!this.skin.isEmpty() && this.scene != null) {
            boolean exists = this.scene.skins.containsKey(this.skin);
            if (!exists) {
                return new Status(IStatus.ERROR, Activator.PLUGIN_ID, NLS.bind(Messages.SpineModelNode_skin_INVALID, this.skin));
            }
        }
        return Status.OK_STATUS;
    }

    public TextureSetNode getTextureSetNode() {
        return this.textureSetNode;
    }

    public SpineSceneUtil getScene() {
        return this.scene;
    }

    public String getMaterial() {
        return this.material;
    }

    public void setMaterial(String material) {
        this.material = material;
    }

    public BlendMode getBlendMode() {
        return this.blendMode;
    }

    public void setBlendMode(BlendMode blendMode) {
        this.blendMode = blendMode;
    }

    @Override
    public void parentSet() {
        if (getParent() != null) {
            setFlags(Flags.TRANSFORMABLE);
        } else {
            clearFlags(Flags.TRANSFORMABLE);
        }
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null && this.textureSetNode == null) {
            reloadResources();
        }
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        boolean reloaded = false;
        if (!this.spineScene.isEmpty()) {
            IFile spineSceneFile = getModel().getFile(this.spineScene);
            if (spineSceneFile.exists() && spineSceneFile.equals(file)) {
                if (reloadResources()) {
                    reloaded = true;
                }
            }
        }
        if (this.sceneDesc != null) {
            String atlas = this.sceneDesc.getAtlas();
            if (!atlas.isEmpty()) {
                IFile atlasFile = getModel().getFile(atlas);
                if (atlasFile.exists() && atlasFile.equals(file)) {
                    if (reloadResources()) {
                        reloaded = true;
                    }
                }
            }
            String json = this.sceneDesc.getSpineJson();
            if (!json.isEmpty()) {
                IFile jsonFile = getModel().getFile(json);
                if (jsonFile.exists() && jsonFile.equals(file)) {
                    if (reloadResources()) {
                        reloaded = true;
                    }
                }
            }
        }
        if (this.textureSetNode != null && this.textureSetNode.handleReload(file, childWasReloaded)) {
            reloaded = true;
        }
        return reloaded;
    }

    private void collectBones(Node node, Map<String, SpineBoneNode> result) {
        for (Node child : node.getChildren()) {
            if (child instanceof SpineBoneNode) {
                result.put(((SpineBoneNode)child).getId(), (SpineBoneNode)child);
                collectBones(child, result);
            }
        }
    }

    private void rebuildBoneHierarchy() {
        if (this.scene != null) {
            Map<String, SpineBoneNode> nodes = new HashMap<String, SpineBoneNode>();
            collectBones(this, nodes);
            Set<String> nodesToRemove = new HashSet<String>(nodes.keySet());
            for (Bone b : this.scene.bones) {
                nodesToRemove.remove(b.name);
            }
            for (String name : nodesToRemove) {
                SpineBoneNode n = nodes.get(name);
                n.setFlags(Flags.LOCKED);
                n.getParent().removeChild(n);
            }
            for (Bone b : this.scene.bones) {
                SpineBoneNode node = nodes.get(b.name);
                if (node == null) {
                    node = new SpineBoneNode(b.name);
                    node.setTranslation(b.localT.position);
                    node.setRotation(b.localT.rotation);
                    node.setScale(b.localT.scale);
                    nodes.put(b.name, node);
                }
                Node parent = this;
                if (b.parent != null) {
                    parent = nodes.get(b.parent.name);
                }
                parent.addChild(node);
            }
        } else {
            if (this.rootBoneNode != null) {
                removeChild(this.rootBoneNode);
                this.rootBoneNode = null;
            }
        }
    }

    private void updateMesh() {
        if (this.scene == null || this.textureSetNode == null) {
            return;
        }
        RuntimeTextureSet ts = this.textureSetNode.getRuntimeTextureSet();
        if (ts == null) {
            return;
        }
        List<Mesh> meshes = new ArrayList<Mesh>(this.scene.meshes.size());
        for (Mesh mesh : this.scene.meshes) {
            if (mesh.visible) {
                meshes.add(mesh);
            }
        }
        if (!this.skin.isEmpty() && this.scene.skins.containsKey(this.skin)) {
            List<Mesh> source = this.scene.skins.get(this.skin);
            for (Mesh mesh : source) {
                if (mesh.visible) {
                    meshes.add(mesh);
                }
            }
        }
        Collections.sort(meshes, new Comparator<Mesh>() {
            @Override
            public int compare(Mesh o1, Mesh o2) {
                return o1.slot.index - o2.slot.index;
            }
        });
        this.mesh.update(meshes);
    }

    public static class TransformProvider implements UVTransformProvider {
        RuntimeTextureSet textureSet;

        public TransformProvider(RuntimeTextureSet textureSet) {
            this.textureSet = textureSet;
        }

        @Override
        public UVTransform getUVTransform(String animId) {
            TextureSetAnimation anim = this.textureSet.getAnimation(animId);
            if (anim != null) {
                return this.textureSet.getUvTransform(anim, 0);
            }
            return null;
        }
    }

    private static SpineSceneDesc loadSpineSceneDesc(ISceneModel model, String path) {
        if (!path.isEmpty()) {
            InputStream in = null;
            try {
                IFile file = model.getFile(path);
                in = file.getContents();
                SpineSceneDesc.Builder builder = SpineSceneDesc.newBuilder();
                TextFormat.merge(new InputStreamReader(in), builder);
                return builder.build();
            } catch (Exception e) {
                // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
            } finally {
                IOUtils.closeQuietly(in);
            }
        }
        return null;
    }

    private static TextureSetNode loadTextureSetNode(ISceneModel model, String path) {
        if (!path.isEmpty()) {
            try {
                Node node = model.loadNode(path);
                if (node instanceof TextureSetNode) {
                    node.setModel(model);
                    return (TextureSetNode)node;
                }
            } catch (Exception e) {
                // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
            }
        }
        return null;
    }

    private static SpineSceneUtil loadSpineScene(ISceneModel model, String path, UVTransformProvider provider) {
        if (!path.isEmpty()) {
            InputStream in = null;
            try {
                IFile file = model.getFile(path);
                in = file.getContents();
                return SpineSceneUtil.loadJson(in, provider);
            } catch (Exception e) {
                // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
            } finally {
                IOUtils.closeQuietly(in);
            }
        }
        return null;
    }

    private boolean reloadResources() {
        ISceneModel model = getModel();
        if (model != null) {
            this.sceneDesc = loadSpineSceneDesc(model, this.spineScene);
            if (this.sceneDesc != null) {
                this.textureSetNode = loadTextureSetNode(model, this.sceneDesc.getAtlas());
                if (this.textureSetNode != null) {
                    this.scene = loadSpineScene(model, this.sceneDesc.getSpineJson(), new TransformProvider(this.textureSetNode.getRuntimeTextureSet()));
                }
            }
            updateMesh();
            updateAABB();
            rebuildBoneHierarchy();
            updateStatus();
            // attempted to reload
            return true;
        }
        return false;
    }

    private void readObject(ObjectInputStream in) throws IOException, ClassNotFoundException {
        in.defaultReadObject();
        this.mesh = new CompositeMesh();
    }
}
