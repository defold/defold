package com.dynamo.cr.tileeditor;

import javax.annotation.PreDestroy;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.ScenePresenter;
import com.dynamo.cr.tileeditor.scene.AtlasAnimationNode;
import com.dynamo.cr.tileeditor.scene.AtlasNode;

public class AtlasEditorPresenter extends ScenePresenter {
    private AtlasNode root;

    @PreDestroy
    public void dispose() {
    }

    @Override
    public void onProjectPropertiesChanged(ProjectProperties properties) throws CoreException {
        super.onProjectPropertiesChanged(properties);
    }

    @Override
    public void rootChanged(Node root) {
        AtlasNode atlasNode = (AtlasNode)root;
        this.root = atlasNode;
        super.rootChanged(root);
    }

    public void setPlayAnimationNode(AtlasAnimationNode node) {
        root.setPlaybackNode(node);
    }
}
