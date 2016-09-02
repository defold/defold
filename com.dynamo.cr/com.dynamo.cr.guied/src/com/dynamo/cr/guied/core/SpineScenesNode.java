package com.dynamo.cr.guied.core;

import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;


@SuppressWarnings("serial")
public class SpineScenesNode extends LabelNode {

    public SpineScenesNode() {
        super("Spine Scenes");
    }

    public List<String> getSpineScenes(ISceneModel model) {
        List<Node> spineSceneNodes = this.getChildren();
        List<String> spineScenes = new ArrayList<String>(spineSceneNodes.size());

        for (Node node : spineSceneNodes) {
            SpineSceneNode spineSceneNode = (SpineSceneNode) node;
            spineScenes.add(spineSceneNode.getId());

        }
        return spineScenes;
    }

    public SpineSceneNode getSpineScenesNode(String spinescene) {
        String baseSpineScene = spinescene.split("/")[0];
        for (Node n : getChildren()) {
            SpineSceneNode spineSceneNode = (SpineSceneNode) n;
            if (spineSceneNode.getId().equals(baseSpineScene)) {
                return spineSceneNode;
            }
        }
        return null;
    }

}
