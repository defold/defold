package com.dynamo.cr.tileeditor.core;

import java.util.List;

import com.dynamo.cr.tileeditor.core.GridModel.Layer;

public interface IGridView {

    void setTileSetProperty(String newValue);

    void setCellWidthProperty(float newValue);

    void setCellHeightProperty(float newValue);

    void setLayers(List<Layer> newValue);

    void refreshProperties();

    void setValid(boolean valid);

    void setDirty(boolean dirty);

}
