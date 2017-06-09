package com.dynamo.cr.guied.core;

import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;


@SuppressWarnings("serial")
public class ParticleFXScenesNode extends LabelNode {

    public ParticleFXScenesNode() {
        super("ParticleFX");
    }

    public List<String> getParticleFXScenes(ISceneModel model) {
        List<Node> particlefxNodes = this.getChildren();
        List<String> particleFXs = new ArrayList<String>(particlefxNodes.size());

        for (Node node : particlefxNodes) {
            ParticleFXSceneNode particlefxNode = (ParticleFXSceneNode) node;
            particleFXs.add(particlefxNode.getId());
        }

        return particleFXs;
    }
    
    public ParticleFXSceneNode getParticleFXScenesNode(String particlefxScene) {
        String baseParticleFXScene = particlefxScene.split("/")[0];
        for (Node n : getChildren()) {
            ParticleFXSceneNode particlefxSceneNode = (ParticleFXSceneNode) n;
            if (particlefxSceneNode.getId().equals(baseParticleFXScene)) {
                return particlefxSceneNode;
            }
        }
        return null;
    }
}
