package com.dynamo.cr.guied.core;

import java.util.ArrayList;
import java.util.List;

//import org.slf4j.Logger;
//import org.slf4j.LoggerFactory;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;


@SuppressWarnings("serial")
public class SpineScenesNode extends LabelNode {

//    private static Logger logger = LoggerFactory.getLogger(SpineScenesNode.class);

    public SpineScenesNode() {
        super("Spine Scenes");
    }

    /*
    String[] supportedFormats = {".spinescene"};
    private boolean IsValidResource(String resourceName) {
        for (String name : this.supportedFormats) {
            if(resourceName.endsWith(name))
                return true;
        }
        return false;
    }
    */

    public List<String> getSpineScenes(ISceneModel model) {
        List<Node> spineSceneNodes = this.getChildren();
        List<String> spineScenes = new ArrayList<String>(spineSceneNodes.size());

        for (Node node : spineSceneNodes) {
            SpineSceneNode spineSceneNode = (SpineSceneNode) node;

            /*
            if(this.IsValidResource(spineSceneNode.getSpineScene())) {
                TextureSetNode textureSetNode = null;
                try {
                    Node n = model.loadNode(spineSceneNode.getSpineScene());
                    textureSetNode = (TextureSetNode) n;
                } catch (Exception e) {
                    logger.error("Failed loading resource: " + spineSceneNode.getSpineScene(), e);
                    continue;
                }

                if (textureSetNode == null) {
                    logger.error("Failed loading resource: " + spineSceneNode.getSpineScene());
                    continue;
                }

                List<String> animationsIds = textureSetNode.getAnimationIds();
                for(String animationsId : animationsIds) {
                    spineScenes.add(spineSceneNode.getId() + "/" + animationsId);
                }
            } else {
                spineScenes.add(spineSceneNode.getId());
            }
            */
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
