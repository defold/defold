package com.defold.editor.pipeline;

import java.util.List;

import com.defold.editor.pipeline.TextureSetLayout.Layout;
import com.defold.editor.pipeline.TextureSetLayout.Rect;

public interface TextureSetLayoutStrategy {
    List<Layout> createLayout(List<Rect> srcRects);
}
