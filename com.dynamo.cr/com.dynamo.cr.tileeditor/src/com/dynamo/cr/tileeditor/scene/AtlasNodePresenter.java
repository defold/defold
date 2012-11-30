package com.dynamo.cr.tileeditor.scene;

import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.sceneed.core.ISceneView.INodePresenter;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.operations.AddAnimationGroupNodeOperation;
import com.dynamo.cr.tileeditor.operations.AddImagesNodeOperation;


public class AtlasNodePresenter implements INodePresenter<AtlasNode> {

    public void onAddAnimationGroup(IPresenterContext presenterContext) {
        Node parent = (Node) presenterContext.getSelection().getFirstElement();
        while (parent != null) {
            if (parent instanceof AtlasNode) {
                break;
            }
            parent = parent.getParent();
        }

        if (parent != null) {
            presenterContext.executeOperation(new AddAnimationGroupNodeOperation((AtlasNode) parent, new AtlasAnimationNode(), presenterContext));
        }
    }

    public void onAddImages(IPresenterContext presenterContext) {
        Node parent = (Node) presenterContext.getSelection().getFirstElement();

        while (parent != null) {
            if (parent instanceof AtlasNode || parent instanceof AtlasAnimationNode) {
                break;
            }
            parent = parent.getParent();
        }

        if (parent != null) {
            String[] files = presenterContext.selectFiles("Add Images", new String[] {"png"});
            if (files != null) {

                List<Node> nodes = new ArrayList<Node>();
                for (String image : files) {
                    nodes.add(new AtlasImageNode(image));
                }
                presenterContext.executeOperation(new AddImagesNodeOperation(parent, nodes, presenterContext));
            }
        }
    }
}
