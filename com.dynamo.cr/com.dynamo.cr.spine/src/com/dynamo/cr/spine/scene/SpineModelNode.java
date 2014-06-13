package com.dynamo.cr.spine.scene;

import java.io.InputStream;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.bob.util.SpineScene;
import com.dynamo.bob.util.SpineScene.Bone;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.GreaterThanZero;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.spine.Activator;
import com.dynamo.cr.tileeditor.scene.TextureSetNode;
import com.dynamo.spine.proto.Spine.SpineModelDesc.BlendMode;

@SuppressWarnings("serial")
public class SpineModelNode extends ComponentTypeNode {

    @Property(displayName="Spine Scene", editorType=EditorType.RESOURCE, extensions={"json"})
    @Resource
    @NotEmpty
    private String spineScene = "";

    private transient SpineScene scene;

    @Property(displayName="Atlas", editorType=EditorType.RESOURCE, extensions={"atlas"})
    @Resource
    @NotEmpty
    private String atlas = "";

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

    @Property
    @GreaterThanZero
    private float sampleRate = 30.0f;

    @Override
    public void dispose() {
        super.dispose();
        if (this.textureSetNode != null) {
            this.textureSetNode.dispose();
        }
    }

    public String getSpineScene() {
        return spineScene;
    }

    public void setSpineScene(String spineScene) {
        if (!this.spineScene.equals(spineScene)) {
            this.spineScene = spineScene;
            reloadSpineScene();
        }
    }

    public String getAtlas() {
        return atlas;
    }

    public void setAtlas(String atlas) {
        if (!this.atlas.equals(atlas)) {
            this.atlas = atlas;
            reloadAtlas();
        }
    }

    public IStatus validateAtlas() {
        if (this.textureSetNode != null) {
            this.textureSetNode.updateStatus();
            IStatus status = this.textureSetNode.getStatus();
            if (!status.isOK()) {
                return new Status(IStatus.ERROR, Activator.PLUGIN_ID, Messages.SpineModelNode_atlas_INVALID_REFERENCE);
            }
        } else if (!this.atlas.isEmpty()) {
            return new Status(IStatus.ERROR, Activator.PLUGIN_ID, Messages.SpineModelNode_atlas_CONTENT_ERROR);
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
        // TODO: fix as part of rendering DEF-238
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
        this.skin = skin;
        updateStatus();
        updateAABB();
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

    public float getSampleRate() {
        return this.sampleRate;
    }

    public void setSampleRate(float sampleRate) {
        this.sampleRate = sampleRate;
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
            reloadSpineScene();
            reloadAtlas();
        }
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        boolean reloaded = false;
        if (!this.spineScene.isEmpty()) {
            IFile spineSceneFile = getModel().getFile(this.spineScene);
            if (spineSceneFile.exists() && spineSceneFile.equals(file)) {
                if (reloadSpineScene()) {
                    reloaded = true;
                }
            }
        }
        if (!this.atlas.isEmpty()) {
            IFile atlasFile = getModel().getFile(this.atlas);
            if (atlasFile.exists() && atlasFile.equals(file)) {
                if (reloadAtlas()) {
                    reloaded = true;
                }
            }
            if (this.textureSetNode != null) {
                if (this.textureSetNode.handleReload(file, childWasReloaded)) {
                    reloaded = true;
                }
            }
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

    private boolean reloadSpineScene() {
        ISceneModel model = getModel();
        if (model != null) {
            this.scene = null;
            if (!this.spineScene.isEmpty()) {
                IFile spineSceneFile = model.getFile(this.spineScene);
                try {
                    InputStream in = spineSceneFile.getContents();
                    this.scene = SpineScene.loadJson(in);
                    updateStatus();
                } catch (Exception e) {
                    // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
                }
            }
            updateAABB();
            rebuildBoneHierarchy();
            // attempted to reload
            return true;
        }
        return false;
    }

    private boolean reloadAtlas() {
        ISceneModel model = getModel();
        if (model != null) {
            this.textureSetNode = null;
            if (!this.atlas.isEmpty()) {
                try {
                    Node node = model.loadNode(this.atlas);
                    if (node instanceof TextureSetNode) {
                        this.textureSetNode = (TextureSetNode)node;
                        this.textureSetNode.setModel(getModel());
                        updateStatus();
                    }
                } catch (Exception e) {
                    // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
                }
            }
            updateAABB();
            // attempted to reload
            return true;
        }
        return false;
    }

}
