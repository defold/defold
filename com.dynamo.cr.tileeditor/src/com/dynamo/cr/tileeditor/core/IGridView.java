package com.dynamo.cr.tileeditor.core;

import java.io.InputStream;
import java.io.OutputStream;
import java.util.List;

import com.dynamo.cr.tileeditor.core.GridModel.Layer;

public interface IGridView {

    public interface Presenter {

        void onLoad(InputStream is);

        void onSave(OutputStream os);

        void onTileSelected(int tileIndex);
    }

    void setTileSetProperty(String newValue);

    void setCellWidthProperty(float newValue);

    void setCellHeightProperty(float newValue);

    void setLayers(List<Layer> newValue);

    void refreshProperties();

    void setValid(boolean valid);

    void setDirty(boolean dirty);

}
