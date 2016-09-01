package com.dynamo.cr.guied.core;

import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

import javax.media.opengl.GL2;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.swt.graphics.Image;

import com.dynamo.bob.util.RigScene;
import com.dynamo.bob.util.RigScene.Mesh;
import com.dynamo.bob.util.RigScene.UVTransformProvider;
import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Identifiable;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.validators.Unique;
import com.dynamo.cr.spine.scene.CompositeMesh;
import com.dynamo.cr.spine.scene.SpineModelNode.TransformProvider;
import com.dynamo.cr.tileeditor.scene.RuntimeTextureSet;
import com.dynamo.cr.tileeditor.scene.TextureSetNode;
import com.dynamo.spine.proto.Spine.SpineSceneDesc;
import com.google.protobuf.TextFormat;

@SuppressWarnings("serial")
public class SpineSceneNode extends Node implements Identifiable {

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    @Unique(scope = SpineSceneNode.class, base = SpineScenesNode.class)
    private String id;

    @Property(editorType = EditorType.RESOURCE, extensions = { "spinescene" })
    private String spineScene;
    
    public SpineSceneNode() {
        this.spineScene = "";
        this.id = "";
    }
    
    public SpineSceneNode(String spineScene) {
        this.spineScene = spineScene;
        this.id = new Path(spineScene).removeFileExtension().lastSegment();
    }

    @Override
    public String getId() {
        return this.id;
    }

    @Override
    public void setId(String id) {
        this.id = id;
    }

    public String getSpineScene() {
        return this.spineScene;
    }

    public void setSpineScene(String spineScene) {
        this.spineScene = spineScene;
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
