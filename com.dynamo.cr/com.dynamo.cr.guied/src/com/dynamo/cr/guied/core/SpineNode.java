package com.dynamo.cr.guied.core;

import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

import javax.media.opengl.GL2;
import javax.vecmath.Vector3d;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.Image;

import com.dynamo.bob.util.RigUtil;
import com.dynamo.bob.util.RigUtil.Animation;
import com.dynamo.bob.util.RigUtil.SkinSlot;
import com.dynamo.bob.util.RigUtil.MeshAttachment;
import com.dynamo.bob.util.RigUtil.UVTransformProvider;
import com.dynamo.bob.util.SpineSceneUtil;
import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.guied.core.SpineScenesNode;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.spine.scene.CompositeMesh;
import com.dynamo.cr.spine.scene.Messages;
import com.dynamo.cr.spine.scene.SpineModelNode.TransformProvider;
import com.dynamo.cr.tileeditor.scene.RuntimeTextureSet;
import com.dynamo.cr.tileeditor.scene.TextureSetNode;
import com.dynamo.gui.proto.Gui.NodeDesc.SizeMode;
import com.dynamo.spine.proto.Spine.SpineSceneDesc;
import com.google.protobuf.TextFormat;

@SuppressWarnings("serial")
public class SpineNode extends ClippingNode {

    @Property(editorType = EditorType.DROP_DOWN, category = "")
    private String spineScene = "";

    @Property(editorType = EditorType.DROP_DOWN, category = "")
    private String spineDefaultAnimation = "";

    @Property(editorType = EditorType.DROP_DOWN, category = "")
    private String skin = "";

    private transient SpineSceneDesc sceneDesc;
    private transient SpineSceneUtil scene;
    private transient TextureSetNode textureSetNode = null;
    private transient CompositeMesh mesh = new CompositeMesh();

    public Vector3d getSize() {
        return new Vector3d(1.0, 1.0, 0.0);
    }

    public void setSize() {
        return;
    }

    public boolean isTextureVisible() {
        return false;
    }

    public boolean isAlphaVisible() {
        return true;
    }
    public boolean isInheritAlphaVisible() {
        return true;
    }

    public boolean isSizeVisible() {
        return false;
    }

    public boolean isColorVisible() {
        return true;
    }

    public boolean isBlendModeVisible() {
        return true;
    }

    public boolean isPivotVisible() {
        return true;
    }

    public boolean isAdjustModeVisible() {
        return true;
    }

    public boolean isXanchorVisible() {
        return true;
    }

    public boolean isYanchorVisible() {
        return true;
    }

    public boolean isSizeModeVisible() {
        return false;
    }

    // Skin Id property
    public String getSkin() {
        return this.skin;
    }

    public void setSkin(String skin) {
        this.skin = skin;
        reloadResources();
        GuiNodeStateBuilder.setField(this, "SpineSkin", skin);
    }

    public void resetSkin() {
        this.skin = (String)GuiNodeStateBuilder.resetField(this, "SpineSkin");
        reloadResources();
    }

    public boolean isSkinOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "SpineSkin", this.skin);
    }

    public Object[] getSkinOptions() {
        List<String> ids = new ArrayList<String>();
        if (this.scene != null) {
            for (Map.Entry<String, List<SkinSlot>> n : this.scene.skins.entrySet()) {
                ids.add(n.getKey());
            }
        }
        return ids.toArray();
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

    // Default Animation Id property
    public String getSpineDefaultAnimation() {
        return this.spineDefaultAnimation;
    }

    public void setSpineDefaultAnimation(String spineDefaultAnimation) {
        this.spineDefaultAnimation = spineDefaultAnimation;
        reloadResources();
        GuiNodeStateBuilder.setField(this, "SpineDefaultAnimation", spineDefaultAnimation);
    }

    public void resetSpineDefaultAnimation() {
        this.spineDefaultAnimation = (String)GuiNodeStateBuilder.resetField(this, "SpineDefaultAnimation");
        reloadResources();
    }

    public boolean isSpineDefaultAnimationOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "SpineDefaultAnimation", this.spineDefaultAnimation);
    }

    public Object[] getSpineDefaultAnimationOptions() {
        List<String> ids = new ArrayList<String>();
        if (this.scene != null) {
            for (Map.Entry<String, Animation> n : this.scene.animations.entrySet()) {
                ids.add(n.getKey());
            }
        }
        return ids.toArray();
    }

    public IStatus validateSpineDefaultAnimation() {
        if (!this.spineDefaultAnimation.isEmpty() && this.scene != null) {
            boolean exists = this.scene.getAnimation(this.spineDefaultAnimation) != null;
            if (!exists) {
                return new Status(IStatus.ERROR, Activator.PLUGIN_ID, NLS.bind(Messages.SpineModelNode_defaultAnimation_INVALID, this.spineDefaultAnimation));
            }
        }
        return Status.OK_STATUS;
    }


    // Spine Scene property
    public String getSpineScene() {
        return this.spineScene;
    }

    public void setSpineScene(String spineScene) {
        this.spineScene = spineScene;
        reloadResources();
        GuiNodeStateBuilder.setField(this, "SpineScene", spineScene);
    }

    public void resetSpineScene() {
        this.spineScene = (String)GuiNodeStateBuilder.resetField(this, "SpineScene");
        reloadResources();
    }

    public boolean isSpineSceneOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "SpineScene", this.spineScene);
    }


    @Override
    public void dispose(GL2 gl) {
        super.dispose(gl);
        if (this.textureSetNode != null) {
            this.textureSetNode.dispose(gl);
        }
        this.mesh.dispose(gl);
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

    public CompositeMesh getCompositeMesh() {
        return this.mesh;
    }


    public TextureSetNode getTextureSetNode() {
        return this.textureSetNode;
    }

    public Object[] getSpineSceneOptions() {
        SpineScenesNode node = (SpineScenesNode)getScene().getSpineScenesNode();
        return node.getSpineScenes(getModel()).toArray();
    }

    private SpineSceneNode getSpineScenesNode() {
        SpineSceneNode spineSceneNode = ((SpineScenesNode) getScene().getSpineScenesNode()).getSpineScenesNode(this.spineScene);
        if(spineSceneNode == null) {
            TemplateNode parentTemplate = this.getParentTemplateNode();
            if(parentTemplate != null && parentTemplate.getTemplateScene() != null) {
                spineSceneNode = ((SpineScenesNode) parentTemplate.getTemplateScene().getSpineScenesNode()).getSpineScenesNode(this.spineScene);
            }
        }
        return spineSceneNode;
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
        return reloadResources();
    }

    @Override
    public Image getIcon() {
        if(GuiNodeStateBuilder.isStateSet(this)) {
            if(isTemplateNodeChild()) {
                return Activator.getDefault().getImageRegistry().get(Activator.SPINE_NODE_OVERRIDDEN_TEMPLATE_IMAGE_ID);
            }
            return Activator.getDefault().getImageRegistry().get(Activator.SPINE_NODE_OVERRIDDEN_IMAGE_ID);
        }
        return Activator.getDefault().getImageRegistry().get(Activator.SPINE_NODE_IMAGE_ID);
    }

    public boolean isSizeEditable() {
        return false;
    }

    private void updateMesh() {
        if (this.scene == null || this.textureSetNode == null) {
            return;
        }

        if (this.mesh == null) {
            this.mesh = new CompositeMesh();
        }

        RuntimeTextureSet ts = this.textureSetNode.getRuntimeTextureSet();
        if (ts == null) {
            return;
        }
        List<MeshAttachment> allMeshes = this.scene.getDefaultAttachments();
        List<MeshAttachment> meshes = new ArrayList<MeshAttachment>(allMeshes.size());
        if (!this.skin.isEmpty() && this.scene.skins.containsKey(this.skin)) {
            meshes = this.scene.getAttachmentsForSkin(this.skin);
        } else {
            meshes = allMeshes;
        }
        this.mesh.update(meshes);
    }

    private void updateAABB() {
        AABB aabb = new AABB();
        aabb.setIdentity();
        if (this.scene != null) {
            List<MeshAttachment> meshes = this.scene.getDefaultAttachments();
            for (MeshAttachment mesh : meshes) {
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

    private boolean reloadResources() {
        ISceneModel model = getModel();
        if (model != null && getSpineScenesNode() != null) {
            this.sceneDesc = loadSpineSceneDesc(model, getSpineScenesNode().getSpineScene());
            if (this.sceneDesc != null) {
                this.textureSetNode = loadTextureSetNode(model, this.sceneDesc.getAtlas());
                if (this.textureSetNode != null) {
                    this.scene = loadSpineScene(model, this.sceneDesc.getSpineJson(), new TransformProvider(this.textureSetNode.getRuntimeTextureSet()));
                }
            }
            updateMesh();
            updateAABB();
            updateStatus();
            // attempted to reload
            return true;
        } else {
            this.scene = null;
            this.sceneDesc = null;
            this.textureSetNode = null;
        }
        return false;
    }

}
