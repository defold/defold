package com.dynamo.cr.tileeditor.scene;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.TextureHandle;

/**
 * Abstract base-class for nodes representing a set of textures and animations, i.e. TextureSet
 *
 * @author chmu
 *
 */

@SuppressWarnings("serial")
public abstract class TextureSetNode extends Node {

    /**
     * Get {@link RuntimeTextureSet}
     * @return
     */
    public abstract RuntimeTextureSet getRuntimeTextureSet();

    /**
     * Get texture of the laid out images
     * @return {@link TextureHandle}
     */
    public abstract TextureHandle getTextureHandle();

}
