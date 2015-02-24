package com.dynamo.bob.textureset;

import java.util.List;

import com.dynamo.bob.textureset.TextureSetLayout.Layout;
import com.dynamo.bob.textureset.TextureSetLayout.Rect;

public interface TextureSetLayoutStrategy {
    List<Layout> createLayout(List<Rect> srcRects);
}
