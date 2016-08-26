package com.dynamo.cr.guied.core;

import javax.vecmath.Point2d;
import javax.vecmath.Vector3d;

import org.eclipse.core.resources.IFile;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.guied.core.SpineScenesNode;
import com.dynamo.cr.guied.core.GuiTextureNode.UVTransform;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.gui.proto.Gui.NodeDesc.SizeMode;

@SuppressWarnings("serial")
public class SpineNode extends GuiNode {

    @Property(editorType = EditorType.DROP_DOWN, category = "")
    private String spineScene = "";
    
//    @Property(editorType = EditorType.DROP_DOWN, category = "")
//    private String texture = "";

    @Property(editorType = EditorType.DEFAULT, category = "")
    private String defaultAnimationId = "";

    @Property(editorType = EditorType.DEFAULT, category = "")
    private String skinId= "";
    
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
        return false;
    }
    public boolean isInheritAlphaVisible() {
        return false;
    }
    
    public boolean isSizeVisible() {
        return true;
    }

    public boolean isColorVisible() {
        return false;
    }

    public boolean isBlendModeVisible() {
        return false;
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
    
    public String getSkinId() {
        return this.skinId;
    }

    public void setSkinId(String skin_id) {
        this.skinId = skin_id;
    }
    
    public String getDefaultAnimationId() {
        return this.defaultAnimationId;
    }

    public void setDefaultAnimationId(String default_animation_id) {
        this.defaultAnimationId = default_animation_id;
    }
    
    // TEXTURE STUFF
//    public String getTexture() {
//        return this.texture;
//    }
//
//    public void setTexture(String texture) {
//        this.texture = texture;
////        updateTexture();
//        GuiNodeStateBuilder.setField(this, "Texture", texture);
//    }
//
//    public void resetTexture() {
//        this.texture = (String)GuiNodeStateBuilder.resetField(this, "Texture");
////        updateTexture();
//    }
//    
//
//    private TextureNode getTextureNode() {
//        TextureNode textureNode = ((TexturesNode) getScene().getTexturesNode()).getTextureNode(this.texture);
//        if(textureNode == null) {
//            TemplateNode parentTemplate = this.getParentTemplateNode();
//            if(parentTemplate != null && parentTemplate.getTemplateScene() != null) {
//                textureNode = ((TexturesNode) parentTemplate.getTemplateScene().getTexturesNode()).getTextureNode(this.texture);
//            }
//        }
//        return textureNode;
//    }
//
//    private void updateTexture() {
//        if (!this.texture.isEmpty() && getModel() != null) {
//            TextureNode textureNode = this.getTextureNode();
////            if(textureNode != null)
////            {
////                if (this.guiTextureNode == null) {
////                    this.guiTextureNode = new GuiTextureNode();
////                }
////                this.guiTextureNode.setTexture(this, textureNode.getTexture(), this.texture);
////                updateSize();
////                return;
////            }
//        }
////        this.guiTextureNode = null;
//    }

    public Object[] getTextureOptions() {
        TexturesNode node = (TexturesNode) getScene().getTexturesNode();
        return node.getTextures(getModel()).toArray();
    }

    // SPINE STUFF
    public String getSpineScene() {
        return this.spineScene;
    }

    public void setSpineScene(String spineScene) {
        this.spineScene = spineScene;
        //updateTexture();
        GuiNodeStateBuilder.setField(this, "Spine Scene", spineScene);
    }

    public void resetSpineScene() {
        this.spineScene = (String)GuiNodeStateBuilder.resetField(this, "Spine Scene");
        //updateTexture();
    }

    public boolean isSpineSceneOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Spine Scene", this.spineScene);
    }
    
    public Object[] getSpineSceneOptions() {
        SpineScenesNode node = (SpineScenesNode)getScene().getSpineScenesNode();
        return node.getSpineScenes(getModel()).toArray();
    }
    
    private SpineSceneNode getSpineScenesNode() {
        SpineSceneNode spineSceneNode = ((SpineScenesNode) getScene().getSpineScenesNode()).getSpineScenesNode(this.spineScene);
        if(spineSceneNode == null) {
            /*
            TemplateNode parentTemplate = this.getParentTemplateNode();
            if(parentTemplate != null && parentTemplate.getTemplateScene() != null) {
                spineSceneNode = ((TexturesNode) parentTemplate.getTemplateScene().getTexturesNode()).getTextureNode(this.spineScene);
            }
            */
        }
        return spineSceneNode;
    }
    
    private void updateSpineScene() {
        if (!this.spineScene.isEmpty() && getModel() != null) {
            SpineSceneNode spineSceneNode = this.getSpineScenesNode();
            if(spineSceneNode != null)
            {
//                if (this.guiSpineSceneNode == null) {
//                    this.guiSpineSceneNode = new GuiSpineSceneNode();
//                }
//                this.guiTextureNode.setTexture(this, textureNode.getTexture(), this.spineScene);
//                updateSize();
                return;
            }
        }
        //this.guiTextureNode = null;
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        updateSpineScene();
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        boolean reloaded = false;
        /*
        if (this.spineScene != null && !this.spineScene.isEmpty()) {
            TextureNode textureNode = this.getTextureNode();
            if(textureNode != null) {
                IFile imgFile = getModel().getFile(textureNode.getTexture());
                if (file.equals(imgFile)) {
                    updateTexture();
                    reloaded = true;
                }
            }
        }
        */
        return reloaded;
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
    
    /*
    public Vector4d getSlice9()
    {
        return new Vector4d(slice9);
    }

    public void setSlice9(Vector4d slice9)
    {
        this.slice9.set(slice9);
        GuiNodeStateBuilder.setField(this, "Slice9", LoaderUtil.toVector4(slice9));
    }

    public void resetSlice9() {
        this.slice9.set(LoaderUtil.toVector4((Vector4)GuiNodeStateBuilder.resetField(this, "Slice9")));
    }


    public boolean isSlice9Overridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Slice9", LoaderUtil.toVector4(this.slice9));
    }
    */

    @Override
    public void setSizeMode(SizeMode sizeMode) {
        super.setSizeMode(sizeMode);
        updateSpineScene();
    }

    public boolean isSizeEditable() {
        return false;
    }

}
