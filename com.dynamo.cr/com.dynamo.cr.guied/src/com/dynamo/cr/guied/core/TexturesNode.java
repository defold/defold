package com.dynamo.cr.guied.core;

import java.util.ArrayList;
import java.util.List;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.scene.TextureSetNode;


@SuppressWarnings("serial")
public class TexturesNode extends LabelNode {

    private static Logger logger = LoggerFactory.getLogger(TexturesNode.class);

    public TexturesNode() {
        super("Textures");
    }

    String[] supportedFormats = {".atlas", ".tilesource"};
    private boolean IsValidResource(String resourceName) {
        for (String name : this.supportedFormats) {
            if(resourceName.endsWith(name))
                return true;
        }
        return false;
    }

    public List<String> getTextures(ISceneModel model) {
        List<Node> textureNodes = this.getChildren();
        List<String> textures = new ArrayList<String>(textureNodes.size());

        for (Node node : textureNodes) {
            TextureNode textureNode = (TextureNode) node;

            if(this.IsValidResource(textureNode.getTexture())) {
                TextureSetNode textureSetNode = null;
                try {
                    Node n = model.loadNode(textureNode.getTexture());
                    textureSetNode = (TextureSetNode) n;
                } catch (Exception e) {
                    logger.error("Failed loading resource: " + textureNode.getTexture(), e);
                    continue;
                }

                List<String> animationsIds = textureSetNode.getAnimationIds();
                for(String animationsId : animationsIds) {
                    textures.add(textureNode.getId() + "/" + animationsId);
                }
            } else {
                textures.add(textureNode.getId());
            }

        }
        return textures;
    }

    public TextureNode getTextureNode(String texture) {
        String baseTexture = texture.split("/")[0];
        for (Node n : getChildren()) {
            TextureNode textureNode = (TextureNode) n;
            if (textureNode.getId().equals(baseTexture)) {
                return textureNode;
            }
        }
        return null;
    }

}
