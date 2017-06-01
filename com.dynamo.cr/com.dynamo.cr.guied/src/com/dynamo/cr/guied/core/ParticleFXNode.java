package com.dynamo.cr.guied.core;

import java.io.InputStream;
import java.io.InputStreamReader;

import javax.media.opengl.GL2;
import javax.vecmath.Vector3d;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.resources.IFile;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.particle.proto.Particle;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.google.protobuf.TextFormat;

@SuppressWarnings("serial")
public class ParticleFXNode extends ClippingNode {

    @Property(editorType = EditorType.DROP_DOWN, category = "")
    private String particleFX = "";
    
    private transient Particle.ParticleFX pfxDesc;

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
        //return true;
        return false;
    }

    public boolean isBlendModeVisible() {
        //return true;
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

    // ParticleFX property
    public String getParticleFX() {
        return this.particleFX;
    }
    
    public void setParticleFX(String particleFX) {
        this.particleFX = particleFX;
        reloadResources();
        GuiNodeStateBuilder.setField(this, "particlefx", particleFX);
    }
    
    public void resetParticleFX() {
        this.particleFX = (String)GuiNodeStateBuilder.resetField(this, "particlefx");
        reloadResources();
    }
    
    public boolean isParticleFXOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "particlefx", this.particleFX);
    }

    @Override
    public void dispose(GL2 gl) {
        super.dispose(gl);
        /*if (this.textureSetNode != null) {
            this.textureSetNode.dispose(gl);
        }*/
    }
    
    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null) {
            reloadResources();
        }
    }

    /*private static SpineSceneDesc loadSpineSceneDesc(ISceneModel model, String path) {
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
    }*/

    /*public CompositeMesh getCompositeMesh() {
        return this.mesh;
    }*/


    /*public TextureSetNode getTextureSetNode() {
        return this.textureSetNode;
    }*/

    /*public Object[] getSpineSceneOptions() {
        SpineScenesNode node = (SpineScenesNode)getScene().getSpineScenesNode();
        return node.getSpineScenes(getModel()).toArray();
    }*/

    /*private SpineSceneNode getSpineScenesNode() {
        SpineSceneNode spineSceneNode = ((SpineScenesNode) getScene().getSpineScenesNode()).getSpineScenesNode(this.spineScene);
        if(spineSceneNode == null) {
            TemplateNode parentTemplate = this.getParentTemplateNode();
            if(parentTemplate != null && parentTemplate.getTemplateScene() != null) {
                spineSceneNode = ((SpineScenesNode) parentTemplate.getTemplateScene().getSpineScenesNode()).getSpineScenesNode(this.spineScene);
            }
        }
        return spineSceneNode;
    }*/

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        return reloadResources();
    }

    @Override
    public Image getIcon() {
        if(GuiNodeStateBuilder.isStateSet(this)) {
            if(isTemplateNodeChild()) {
                return Activator.getDefault().getImageRegistry().get(Activator.BOX_NODE_OVERRIDDEN_TEMPLATE_IMAGE_ID);
            }
            return Activator.getDefault().getImageRegistry().get(Activator.BOX_NODE_OVERRIDDEN_IMAGE_ID);
        }
        return Activator.getDefault().getImageRegistry().get(Activator.BOX_NODE_IMAGE_ID);
    }

    public boolean isSizeEditable() {
        return false;
    }

    private void updateAABB() {
        AABB aabb = new AABB();
        aabb.setIdentity();
        /*if (this.scene != null) {
            for (Mesh mesh : this.scene.meshes) {
                float[] v = mesh.vertices;
                int vertexCount = v.length / 5;
                for (int i = 0; i < vertexCount; ++i) {
                    int vi = i * 5;
                    aabb.union(v[vi+0], v[vi+1], v[vi+2]);
                }
            }
        }*/
        setAABB(aabb);
    }
    
    private ParticleFXSceneNode getParticleFXSceneNode() {
        ParticleFXSceneNode pfxSceneNode = ((ParticleFXScenesNode) getScene().getParticleFXScenesNode()).getParticleFXScenesNode(this.particleFX);
        if (pfxSceneNode != null) {
            TemplateNode parentTemplate = this.getParentTemplateNode();
            if (parentTemplate != null && parentTemplate.getTemplateScene() != null) {
                pfxSceneNode = ((ParticleFXScenesNode) parentTemplate.getTemplateScene().getParticleFXScenesNode()).getParticleFXScenesNode(this.particleFX);
            }
        }
        return pfxSceneNode;
    }
    
    private static ParticleFX loadParticleFXDesc(ISceneModel model, String path) {
        if (!path.isEmpty()) {
            InputStream in = null;
            try {
                IFile file = model.getFile(path);
                in = file.getContents();
                Particle.ParticleFX.Builder builder = Particle.ParticleFX.newBuilder();
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

    private boolean reloadResources() {
        System.out.println("RELOADING PFX RESOURCE!");
        ISceneModel model = getModel();
        if (model != null && getParticleFXSceneNode() != null) {
            this.pfxDesc = loadParticleFXDesc(model, getParticleFXSceneNode().getParticlefx());
            
            updateAABB();
            updateStatus();
        }
        /*ISceneModel model = getModel();
        if (model != null && getSpineScenesNode() != null) {
            this.sceneDesc = loadSpineSceneDesc(model, getSpineScenesNode().getSpineScene());
            if (this.sceneDesc != null) {
                this.textureSetNode = loadTextureSetNode(model, this.sceneDesc.getAtlas());
                if (this.textureSetNode != null) {
                    this.scene = loadSpineScene(model, this.sceneDesc.getSpineJson(), new TransformProvider(this.textureSetNode.getRuntimeTextureSet()));
                }
            }
            updateAABB();
            updateStatus();
            // attempted to reload
            return true;
        } else {
            this.scene = null;
            this.sceneDesc = null;
            this.textureSetNode = null;
        }
        return false;*/
        
        return true;
    }
}


/*package com.dynamo.cr.guied.core;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.validators.Unique;

@SuppressWarnings("serial")
public class ParticleFXNode extends ClippingNode {

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    @Unique(scope = ParticleFXNode.class, base = ParticleFXsNode.class)
    private String id;

    @Property(editorType = EditorType.RESOURCE, extensions = { "particlefx" })
    private String particlefx;

    public ParticleFXNode() {
        this.particlefx = "";
        this.id = "";
    }

    public ParticleFXNode(String particlefx) {
        this.particlefx = particlefx;
        this.id = new Path(particlefx).removeFileExtension().lastSegment();
    }

    @Override
    public String getId() {
        return this.id;
    }

    @Override
    public void setId(String id) {
        this.id = id;
    }

    public String getParticleFX() {
        return this.particlefx;
    }

    public void setParticleFX(String particlefx) {
        this.particlefx = particlefx;
    }

    @Override
    public String toString() {
        return this.id;
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.SPINESCENE_IMAGE_ID);
    }

}
*/